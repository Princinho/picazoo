#include "jobs/ThumbnailPipeline.h"

#include "index/Database.h"
#include "index/LibraryDb.h"
#include "index/ThumbStore.h"
#include "media/Decoder.h"
#include "media/Formats.h"
#include "media/JpegDecoder.h"
#include "platform/win/FileIdentity.h"

#include <QDebug>

#include <chrono>

namespace picazoo {

struct ThumbnailPipeline::Pending {
    QList<ItemUpdate> updates;
    QList<StoredThumb> thumbs;
    QList<ItemId> requeue;
};

namespace {

constexpr auto kFlushInterval = std::chrono::milliseconds(200);
constexpr qsizetype kFlushBatch = 128;

StoredThumb makeThumb(ItemId id, int version, ThumbKind kind, const QImage& image)
{
    const QImage fitted = fitInto(image, QSize(kThumbSize, kThumbSize));
    return StoredThumb{id, kind, version, fitted.size(), JpegDecoder::encode(fitted, 85)};
}

} // namespace

ThumbnailPipeline::ThumbnailPipeline(Database& readDb, LibraryDb& writer, ThumbStore& thumbs, Notify notify)
    : readDb_(readDb), writer_(writer), thumbs_(thumbs), notify_(std::move(notify)), pending_(std::make_unique<Pending>())
{
}

ThumbnailPipeline::~ThumbnailPipeline() { stop(); }

void ThumbnailPipeline::start(int threads)
{
    if (!workers_.empty())
        return;
    stopping_ = false;
    for (int i = 0; i < std::max(1, threads); ++i)
        workers_.emplace_back([this] { workerLoop(); });
    writerThread_ = std::thread([this] { writerLoop(); });
}

void ThumbnailPipeline::stop()
{
    if (workers_.empty())
        return;
    stopping_ = true;
    queue_.stop();
    for (auto& t : workers_)
        t.join();
    workers_.clear();
    pendingCv_.notify_all();
    if (writerThread_.joinable())
        writerThread_.join();
    flush();
}

void ThumbnailPipeline::enqueue(const QList<ItemId>& ids, Priority p)
{
    for (ItemId id : ids)
        queue_.push(id, p);
}

void ThumbnailPipeline::prioritize(const QList<ItemId>& visible, const QList<ItemId>& nearby)
{
    queue_.reprioritize(visible, nearby);
}

void ThumbnailPipeline::remove(const QList<ItemId>& ids)
{
    for (ItemId id : ids)
        queue_.remove(id);
}

void ThumbnailPipeline::applyPause()
{
    if (userPaused_ || holds_ > 0)
        queue_.pause();
    else
        queue_.resume();
}

void ThumbnailPipeline::pause()
{
    std::lock_guard lock(holdMutex_);
    userPaused_ = true;
    applyPause();
}

void ThumbnailPipeline::resume()
{
    std::lock_guard lock(holdMutex_);
    userPaused_ = false;
    applyPause();
}

bool ThumbnailPipeline::paused() const { return queue_.paused(); }

void ThumbnailPipeline::hold()
{
    std::lock_guard lock(holdMutex_);
    ++holds_;
    applyPause();
}

void ThumbnailPipeline::release()
{
    std::lock_guard lock(holdMutex_);
    holds_ = std::max(0, holds_ - 1);
    applyPause();
}

void ThumbnailPipeline::setVideoHandler(VideoHandler h)
{
    std::lock_guard lock(videoMutex_);
    video_ = std::move(h);
}

bool ThumbnailPipeline::handlesVideos() const
{
    std::lock_guard lock(videoMutex_);
    return bool(video_);
}

size_t ThumbnailPipeline::pending() const { return queue_.size(); }

bool ThumbnailPipeline::idle() const
{
    if (queue_.outstanding() > 0)
        return false;
    std::lock_guard lock(pendingMutex_);
    return pending_->updates.isEmpty() && pending_->thumbs.isEmpty() && pending_->requeue.isEmpty() &&
           inFlight_.load() == 0;
}

void ThumbnailPipeline::workerLoop()
{
    while (auto job = queue_.pop()) {
        ++inFlight_;
        try {
            process(*job);
        } catch (const std::exception& e) {
            qWarning() << "Vignette" << job->id << ":" << e.what();
        }
        queue_.taskDone();
        --inFlight_;
        ++processed_;
    }
}

void ThumbnailPipeline::process(const QueuedItem& job)
{
    const auto item = LibraryDb::item(readDb_, job.id);
    if (!item)
        return; // supprimé ou fusionné entre-temps
    const bool thumbFinished = item->thumbState == ThumbState::Done || item->thumbState == ThumbState::Unreadable ||
                               item->thumbState == ThumbState::NoPreview;
    if (item->metaState != MetaState::Pending && thumbFinished)
        return; // rien à faire
    if (item->kind == Kind::Video && item->metaState != MetaState::Pending && !handlesVideos())
        return;
    const QString path = item->path();
    ItemUpdate u;
    u.id = item->id;
    u.version = item->version;

    Metadata md;
    int orientation = item->orientation;
    if (item->metaState == MetaState::Pending) {
        md = Decoder::readMetadata(path, item->format);
        if (item->kind == Kind::Video) {
            u.meta = Metadata{}; // dimensions et durée fournies par le lecteur vidéo (US3)
        } else if (md.ok) {
            Metadata stored = md;
            stored.embeddedThumb.clear();
            stored.size = orientedSize(md.size, md.orientation);
            u.meta = stored;
            orientation = md.orientation;
        } else {
            u.metaFailed = true;
        }
        u.quickHash = platform::quickHash(path, item->sizeBytes);
    }

    StoredThumb thumb;
    bool haveThumb = false;
    bool requeue = false;

    if (item->kind == Kind::Video) {
        VideoHandler handler;
        {
            std::lock_guard lock(videoMutex_);
            handler = video_;
        }
        if (handler) {
            if (auto v = handler(*item)) {
                thumb = makeThumb(item->id, item->version, ThumbKind::Final, v->image);
                haveThumb = true;
                u.thumbState = ThumbState::Done;
                u.durationMs = v->durationMs;
                if (u.meta)
                    u.meta->size = v->image.size();
            } else {
                u.thumbState = ThumbState::Unreadable;
            }
        }
    } else {
        // 1) Vignette provisoire à partir de la vignette intégrée, pour remplir la grille vite.
        if (item->thumbState == ThumbState::Pending && job.priority == Priority::Background) {
            std::optional<QImage> embedded;
            if (item->format == Format::Jpeg && !md.embeddedThumb.isEmpty()) {
                if (auto img = JpegDecoder::decode(md.embeddedThumb))
                    embedded = applyOrientation(*img, orientation);
            } else if (item->format == Format::Heic) {
                embedded = Decoder::embeddedThumbnail(path, Format::Heic);
            }
            if (embedded && !embedded->isNull()) {
                const bool goodEnough = std::max(embedded->width(), embedded->height()) >= kThumbSize - 20;
                thumb = makeThumb(item->id, item->version, goodEnough ? ThumbKind::Final : ThumbKind::Provisional,
                                  *embedded);
                haveThumb = true;
                u.thumbState = goodEnough ? ThumbState::Done : ThumbState::Provisional;
                requeue = !goodEnough;
            }
        }
        // 2) Vignette définitive de 320 px.
        if (!u.thumbState) {
            DecodeStatus status = DecodeStatus::Unreadable;
            auto r = Decoder::decode({path, item->format, QSize(kThumbSize, kThumbSize), orientation}, &status);
            if (r && !r->image.isNull()) {
                thumb = makeThumb(item->id, item->version, ThumbKind::Final, r->image);
                haveThumb = true;
                u.thumbState = ThumbState::Done;
                if (u.metaFailed || (u.meta && !u.meta->size.isValid())) {
                    Metadata m = u.meta.value_or(Metadata{});
                    m.ok = true;
                    m.size = r->fullSize;
                    m.orientation = orientation;
                    u.meta = m;
                    u.metaFailed = false;
                }
            } else {
                u.thumbState = status == DecodeStatus::NoPreview ? ThumbState::NoPreview : ThumbState::Unreadable;
            }
        }
    }

    bool flushNow = false;
    {
        std::lock_guard lock(pendingMutex_);
        pending_->updates << u;
        if (haveThumb && !thumb.jpeg.isEmpty())
            pending_->thumbs << thumb;
        if (requeue)
            pending_->requeue << item->id;
        flushNow = pending_->updates.size() >= kFlushBatch;
    }
    if (flushNow)
        pendingCv_.notify_one();
}

void ThumbnailPipeline::writerLoop()
{
    while (!stopping_) {
        {
            std::unique_lock lock(pendingMutex_);
            pendingCv_.wait_for(lock, kFlushInterval);
        }
        flush();
    }
}

void ThumbnailPipeline::flush()
{
    Pending batch;
    {
        std::lock_guard lock(pendingMutex_);
        std::swap(batch, *pending_);
        ++inFlight_; // le lot n'est pas encore écrit
    }
    QSet<FolderId> folders;
    QList<ItemId> ids;
    try {
        thumbs_.putBatch(batch.thumbs);
        folders = writer_.applyUpdates(batch.updates);
        for (const auto& u : batch.updates)
            ids << u.id;
    } catch (const std::exception& e) {
        qWarning() << "Écriture des vignettes :" << e.what();
    }
    for (ItemId id : batch.requeue)
        queue_.push(id, Priority::Background);
    --inFlight_;
    if (notify_ && !ids.isEmpty())
        notify_(ids, folders);
}

} // namespace picazoo

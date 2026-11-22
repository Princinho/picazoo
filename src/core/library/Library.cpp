#include "library/Library.h"

#include "index/Database.h"
#include "index/Migrator.h"
#include "index/ThumbStore.h"
#include "jobs/ThumbnailPipeline.h"
#include "library/Settings.h"
#include "media/JpegDecoder.h"
#include "platform/win/FileIdentity.h"
#include "platform/win/KnownFolders.h"
#include "platform/win/Paths.h"
#include "platform/win/CpuInfo.h"
#include "platform/win/DirectoryWalker.h"
#include "scan/Discovery.h"
#include "scan/Watcher.h"
#include "video/VideoProbe.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QThread>

#include <algorithm>

namespace picazoo {

namespace {

qint64 nowMs() { return QDateTime::currentMSecsSinceEpoch(); }

} // namespace

Library::Library(const QString& dataDir, QObject* parent) : QObject(parent), dataDir_(dataDir) {}

std::unique_ptr<Library> Library::open(const QString& dataDir, QObject* parent)
{
    qRegisterMetaType<ScanProgress>();
    QDir().mkpath(dataDir);
    std::unique_ptr<Library> lib(new Library(dataDir, parent));
    const QString dbPath = QDir(dataDir).filePath(QStringLiteral("library.db"));
    lib->writeDb_ = Database::open(dbPath);
    migrateLibrary(*lib->writeDb_);
    lib->uiDb_ = Database::open(dbPath, true);
    lib->workerDb_ = Database::open(dbPath, true);
    lib->thumbs_ = std::make_unique<ThumbStore>(QDir(dataDir).filePath(QStringLiteral("thumbs.db")));
    lib->settings_ = std::make_unique<Settings>(*lib->writeDb_);
    lib->repo_ = std::make_unique<LibraryDb>(*lib->writeDb_);

    Library* self = lib.get();
    lib->pipeline_ = std::make_unique<ThumbnailPipeline>(
        *lib->workerDb_, *lib->repo_, *lib->thumbs_,
        [self](const QList<ItemId>& ids, const QSet<FolderId>& folders) {
            self->noteItems(ids);
            if (!folders.isEmpty())
                self->noteFolders(folders, false);
        });
    const int threads = platform::recommendedWorkerThreads();
    lib->pipeline_->start(threads);
    lib->indexer_ = std::thread([self] { self->indexerLoop(); });

    lib->notifyTimer_.setInterval(250);
    connect(&lib->notifyTimer_, &QTimer::timeout, self, &Library::flushNotifications);
    lib->notifyTimer_.start();

    // Reprise : éléments dont la vignette ou les métadonnées n'ont pas été calculées.
    lib->pipeline_->enqueue(lib->repo_->pendingWork(lib->pipeline_->handlesVideos()));

    // Suivi en direct (US4) : les lots d'événements sont traités sur le thread d'indexation.
    lib->watcher_ = std::make_unique<Watcher>([self](const Watcher::Batch& batch) {
        self->post([self, batch] {
            for (RootId id : batch.lost) {
                self->repo_->setRootState(id, RootState::Unavailable); // rien n'est supprimé (FR-009)
                std::lock_guard lock(self->notifyMutex_);
                self->rootsDirty_ = true;
                self->foldersDirty_ = true;
            }
            if (!batch.paths.isEmpty())
                self->partialScan(batch.paths);
            if (!batch.overflow.isEmpty())
                self->runScan(batch.overflow);
            else if (!batch.lost.isEmpty())
                self->refreshWatchers();
        });
    });
    lib->availabilityTimer_.setInterval(30000);
    connect(&lib->availabilityTimer_, &QTimer::timeout, self, &Library::checkRoots);
    lib->availabilityTimer_.start();

    // Rattrapage au démarrage, en arrière-plan : la bibliothèque est déjà affichable.
    lib->startScan();
    return lib;
}

Library::~Library()
{
    notifyTimer_.stop();
    availabilityTimer_.stop();
    cancelScan_ = true;
    {
        std::lock_guard lock(tasksMutex_);
        stopIndexer_ = true; // post() refuse désormais toute tâche, y compris celles de la surveillance
        tasks_.clear();
    }
    tasksCv_.notify_all();
    paused_ = false;
    pauseCv_.notify_all();
    // L'indexeur s'arrête avant la surveillance : il peut être en train de la reconfigurer.
    if (indexer_.joinable())
        indexer_.join();
    watcher_.reset();
    if (pipeline_)
        pipeline_->stop();
}

// ---------------------------------------------------------- Thread d'indexation

void Library::post(std::function<void()> task)
{
    {
        std::lock_guard lock(tasksMutex_);
        if (stopIndexer_)
            return;
        tasks_.push_back(std::move(task));
    }
    tasksCv_.notify_one();
}

void Library::indexerLoop()
{
    for (;;) {
        std::function<void()> task;
        {
            std::unique_lock lock(tasksMutex_);
            tasksCv_.wait(lock, [this] { return stopIndexer_ || !tasks_.empty(); });
            if (stopIndexer_)
                return;
            task = std::move(tasks_.front());
            tasks_.pop_front();
            indexerBusy_ = true;
        }
        try {
            task();
        } catch (const DbError& e) {
            reportStorageError(QString::fromUtf8(e.what()));
        } catch (const std::exception& e) {
            qWarning() << "Indexation :" << e.what();
        }
        indexerBusy_ = false;
    }
}

bool Library::indexerIdle() const
{
    std::lock_guard lock(tasksMutex_);
    return tasks_.empty() && !indexerBusy_.load();
}

qint64 Library::nextStamp()
{
    lastStamp_ = std::max(nowMs(), lastStamp_ + 1);
    return lastStamp_;
}

void Library::runScan(const QList<RootId>& only)
{
    cancelScan_ = false;
    phase_ = int(ScanProgress::Phase::Discovering);
    foldersScanned_ = 0;
    const qint64 stamp = nextStamp();

    const QList<WatchedRoot> all = LibraryDb::roots(*writeDb_);
    std::vector<RootSpec> specs;
    for (const auto& r : all)
        specs.push_back(RootSpec{r.id, r.path, r.excluded});
    std::vector<RootSpec> effective = Discovery::effectiveRoots(specs);
    if (!only.isEmpty()) {
        std::vector<RootSpec> filtered;
        for (const auto& e : effective) {
            const bool wanted = std::any_of(all.begin(), all.end(), [&](const WatchedRoot& r) {
                return only.contains(r.id) && platform::isInside(platform::normalizePath(r.path), e.path);
            });
            if (wanted)
                filtered.push_back(e);
        }
        effective = std::move(filtered);
    }

    QList<RootId> scanned;
    QStringList skipped;
    QList<ItemId> created;
    DiscoveryCallbacks cb;
    cb.onFolder = [&](const DiscoveredFolder& folder) {
        const SyncResult r = repo_->syncFolder(folder, stamp, [](const QString& p) { return platform::fileIdentity(p); });
        created += r.created;
        pipeline_->enqueue(r.created + r.changed);
        noteFolders({r.folderId}, true);
        ++foldersScanned_;
    };
    cb.onFolderFailed = [&](const QString& path) { skipped << path; };
    cb.cancelled = [this] { return cancelScan_.load(); };
    cb.waitIfPaused = [this] {
        std::unique_lock lock(pauseMutex_);
        pauseCv_.wait(lock, [this] { return !paused_.load() || cancelScan_.load(); });
    };

    for (const auto& root : effective) {
        const auto before = std::find_if(all.begin(), all.end(), [&](const WatchedRoot& r) { return r.id == root.id; });
        const RootState state = Discovery::walkRoot(root, cb);
        if (cancelScan_)
            break;
        if (state == RootState::Available) {
            repo_->setRootScanned(root.id, stamp);
            scanned << root.id;
        } else {
            repo_->setRootState(root.id, state); // rien n'est supprimé pour une racine indisponible
        }
        if (before == all.end() || before->state != state) {
            std::lock_guard lock(notifyMutex_);
            rootsDirty_ = true;
        }
    }

    if (!cancelScan_ && !scanned.isEmpty()) {
        applyOutcome(repo_->finishScan(scanned, skipped, stamp, created,
                                       [](const QString& p, qint64 size) { return platform::quickHash(p, size); }));
    }
    phase_ = int(ScanProgress::Phase::Idle);
    refreshWatchers();
    std::lock_guard lock(notifyMutex_);
    scanFinishedPending_ = true;
    rootsDirty_ = true;
}

void Library::applyOutcome(const ScanOutcome& out)
{
    const QList<ItemId> gone = out.removed + out.mergedAway;
    if (!gone.isEmpty()) {
        pipeline_->remove(gone);
        thumbs_->remove(gone);
    }
    if (!out.moved.isEmpty())
        pipeline_->enqueue(out.moved);
    noteFolders(out.touchedFolders, true);
}

void Library::refreshWatchers()
{
    if (!watcher_)
        return;
    std::vector<RootSpec> specs;
    for (const auto& r : LibraryDb::roots(*writeDb_)) {
        if (r.state == RootState::Available)
            specs.push_back(RootSpec{r.id, r.path, r.excluded});
    }
    watcher_->setRoots(Discovery::effectiveRoots(specs));
}

bool Library::isWatching(RootId id) const { return watcher_ && watcher_->watching(id); }

void Library::checkRoots()
{
    post([this] {
        QList<RootId> back;
        for (const auto& r : LibraryDb::roots(*writeDb_)) {
            if (r.state != RootState::Available &&
                platform::probeDirectory(platform::normalizePath(r.path)) == platform::ListResult::Ok)
                back << r.id;
        }
        if (!back.isEmpty())
            runScan(back); // la racine redevient disponible et surveillée
    });
}

void Library::partialScan(const QHash<RootId, QStringList>& paths)
{
    const qint64 stamp = nextStamp();
    const QList<WatchedRoot> all = LibraryDb::roots(*writeDb_);
    std::vector<RootSpec> specs;
    for (const auto& r : all)
        specs.push_back(RootSpec{r.id, r.path, r.excluded});
    const std::vector<RootSpec> effective = Discovery::effectiveRoots(specs);
    const QList<QPair<FolderId, QString>> known = repo_->folderPaths();

    QSet<FolderId> scope;
    QList<ItemId> created;
    QStringList skipped;
    auto foldersInside = [&](const QString& dir) {
        for (const auto& [id, path] : known)
            if (platform::isInside(path, dir))
                scope.insert(id);
    };
    auto sync = [&](const DiscoveredFolder& folder) {
        const SyncResult r = repo_->syncFolder(folder, stamp, [](const QString& p) { return platform::fileIdentity(p); });
        created += r.created;
        scope.insert(r.folderId);
        pipeline_->enqueue(r.created + r.changed);
        noteFolders({r.folderId}, true);
    };

    for (auto it = paths.constBegin(); it != paths.constEnd(); ++it) {
        const auto root = std::find_if(effective.begin(), effective.end(), [&](const RootSpec& r) { return r.id == it.key(); });
        if (root == effective.end())
            continue;
        QStringList walk;
        QSet<QString> list;
        for (const QString& raw : it.value()) {
            const QString p = platform::normalizePath(raw);
            if (Discovery::isExcluded(p, root->excluded) || !platform::isInside(p, root->path))
                continue;
            bool isDir = false;
            const bool exists = platform::pathExists(p, &isDir);
            if (exists && isDir) {
                walk << p; // dossier créé, renommé ou déplacé ici : parcours récursif
            } else if (!exists) {
                foldersInside(p); // dossier supprimé ou déplacé ailleurs
            }
            const QString parent = platform::parentPath(p);
            if (!parent.isEmpty() && platform::isInside(parent, root->path))
                list.insert(parent.toLower() + u'|' + parent);
        }
        for (const QString& d : walk) {
            foldersInside(d);
            DiscoveryCallbacks cb;
            cb.onFolder = sync;
            cb.onFolderFailed = [&](const QString& failed) { skipped << failed; };
            Discovery::walkRoot(RootSpec{root->id, d, root->excluded}, cb);
        }
        for (const QString& entry : list) {
            const QString dir = entry.section(u'|', 1);
            if (std::any_of(walk.begin(), walk.end(), [&](const QString& w) { return platform::isInside(dir, w); }))
                continue;
            DiscoveredFolder folder;
            if (Discovery::listFolder(root->id, dir, folder)) {
                if (auto id = LibraryDb::folderIdForPath(*writeDb_, dir))
                    scope.insert(*id);
                if (!folder.files.empty())
                    sync(folder);
            } else {
                foldersInside(dir);
            }
        }
    }
    if (scope.isEmpty())
        return;
    applyOutcome(repo_->finishScan({}, skipped, stamp, created,
                                   [](const QString& p, qint64 size) { return platform::quickHash(p, size); },
                                   QList<FolderId>(scope.begin(), scope.end())));
}

// ------------------------------------------------------------ Dossiers surveillés

QList<WatchedRoot> Library::roots() const { return LibraryDb::roots(*uiDb_); }

QStringList Library::defaultRoots() { return platform::defaultPictureFolders(); }

RootId Library::addRoot(const QString& path)
{
    const QString normalized = platform::normalizePath(path);
    RootId id = 0;
    try {
        id = repo_->addRoot(normalized, nowMs());
    } catch (const DbError& e) {
        reportStorageError(QString::fromUtf8(e.what()));
        return 0;
    }
    {
        std::lock_guard lock(notifyMutex_);
        rootsDirty_ = true;
    }
    scanRoot(id);
    return id;
}

void Library::removeRoot(RootId id)
{
    cancelScan_ = true;
    pauseCv_.notify_all();
    post([this, id] {
        const QList<ItemId> ids = repo_->removeRoot(id);
        pipeline_->remove(ids);
        thumbs_->remove(ids);
        {
            std::lock_guard lock(notifyMutex_);
            foldersDirty_ = true;
            rootsDirty_ = true;
        }
        runScan({}); // une racine imbriquée peut ne plus être couverte ; l'analyse interrompue reprend
    });
}

void Library::excludePath(RootId id, const QString& subPath)
{
    const QString normalized = platform::normalizePath(subPath);
    cancelScan_ = true;
    pauseCv_.notify_all();
    post([this, id, normalized] {
        repo_->addExcluded(id, normalized);
        QSet<FolderId> touched;
        const QList<ItemId> ids = repo_->removeItemsUnder(normalized, &touched);
        pipeline_->remove(ids);
        thumbs_->remove(ids);
        noteFolders(touched, true);
        {
            std::lock_guard lock(notifyMutex_);
            foldersDirty_ = true;
            rootsDirty_ = true;
        }
        runScan({});
    });
}

void Library::includePath(RootId id, const QString& subPath)
{
    const QString normalized = platform::normalizePath(subPath);
    post([this, id, normalized] {
        repo_->removeExcluded(id, normalized);
        {
            std::lock_guard lock(notifyMutex_);
            rootsDirty_ = true;
        }
        runScan({id});
    });
}

// ------------------------------------------------------------------- Analyse

void Library::startScan()
{
    post([this] { runScan({}); });
}

void Library::scanRoot(RootId id)
{
    post([this, id] { runScan({id}); });
}

void Library::pauseScan()
{
    paused_ = true;
    pipeline_->pause();
    std::lock_guard lock(notifyMutex_);
    lastProgress_.paused = !lastProgress_.paused; // force la notification
}

void Library::resumeScan()
{
    paused_ = false;
    pauseCv_.notify_all();
    pipeline_->resume();
}

bool Library::isPaused() const { return paused_.load(); }

ScanProgress Library::progress() const
{
    ScanProgress p;
    p.paused = paused_.load();
    const bool busy = !indexerIdle();
    if (busy && phase_.load() == int(ScanProgress::Phase::Discovering)) {
        p.phase = ScanProgress::Phase::Discovering;
        p.done = foldersScanned_.load();
        return p;
    }
    const qint64 processed = pipeline_->processed();
    const qint64 pending = qint64(pipeline_->pending());
    if (pending == 0 && pipeline_->idle()) {
        thumbsBase_ = processed;
        p.phase = busy ? ScanProgress::Phase::Indexing : ScanProgress::Phase::Idle;
        return p;
    }
    p.phase = ScanProgress::Phase::Thumbnails;
    p.done = processed - thumbsBase_.load();
    p.total = p.done + pending;
    return p;
}

// ------------------------------------------------------------------ Lectures

QList<Folder> Library::folders(FolderSort sort) const { return LibraryDb::folders(*uiDb_, sort); }

QList<ItemSummary> Library::itemSummaries(FolderId folder, ItemSort sort) const
{
    return LibraryDb::itemSummaries(*uiDb_, folder, sort);
}

QHash<FolderId, QList<ItemSummary>> Library::allItemSummaries(ItemSort sort) const
{
    return LibraryDb::allItemSummaries(*uiDb_, sort);
}

QList<ItemId> Library::items(FolderId folder, ItemSort sort) const
{
    QList<ItemId> ids;
    for (const auto& s : itemSummaries(folder, sort))
        ids << s.id;
    return ids;
}

std::optional<Item> Library::item(ItemId id) const { return LibraryDb::item(*uiDb_, id); }

qint64 Library::itemCount() const { return LibraryDb::itemCount(*uiDb_); }

void Library::prioritize(const QList<ItemId>& visible, const QList<ItemId>& nearby)
{
    pipeline_->prioritize(visible, nearby);
}

std::optional<QImage> Library::thumbnail(ItemId id, int version) const
{
    auto t = thumbs_->get(id, ThumbKind::Final, version);
    if (!t)
        t = thumbs_->get(id, ThumbKind::Provisional, version);
    if (!t)
        return std::nullopt;
    return JpegDecoder::decode(t->jpeg);
}

void Library::enableVideoThumbnails()
{
    if (video_)
        return;
    video_ = std::make_unique<VideoProbe>();
    VideoProbe* probe = video_.get();
    pipeline_->setVideoHandler([probe](const Item& item) -> std::optional<ThumbnailPipeline::VideoThumb> {
        auto info = probe->probe(item.path());
        if (!info)
            return std::nullopt;
        return ThumbnailPipeline::VideoThumb{info->frame, info->durationMs};
    });
    pipeline_->enqueue(repo_->pendingWork(true));
}

QVariant Library::setting(const QString& key) const { return settings_->value(key); }

void Library::setSetting(const QString& key, const QVariant& value)
{
    try {
        settings_->setValue(key, value);
    } catch (const DbError& e) {
        reportStorageError(QString::fromUtf8(e.what()));
    }
}

// ------------------------------------------------------------- Notifications

void Library::noteFolders(const QSet<FolderId>& folders, bool structure)
{
    std::lock_guard lock(notifyMutex_);
    dirtyFolders_.unite(folders);
    if (structure || !folders.isEmpty())
        foldersDirty_ = true;
}

void Library::noteItems(const QList<ItemId>& ids)
{
    std::lock_guard lock(notifyMutex_);
    for (ItemId id : ids)
        dirtyItems_.insert(id);
}

void Library::reportStorageError(const QString& what)
{
    qWarning() << "Stockage :" << what;
    std::lock_guard lock(notifyMutex_);
    storageErrors_ << what;
}

void Library::flushNotifications()
{
    bool folders = false, roots = false, finished = false;
    QSet<FolderId> dirtyFolders;
    QSet<ItemId> dirtyItems;
    QList<QString> errors;
    {
        std::lock_guard lock(notifyMutex_);
        std::swap(folders, foldersDirty_);
        std::swap(roots, rootsDirty_);
        std::swap(finished, scanFinishedPending_);
        dirtyFolders.swap(dirtyFolders_);
        dirtyItems.swap(dirtyItems_);
        errors.swap(storageErrors_);
    }
    if (roots)
        emit rootsChanged();
    if (folders)
        emit foldersChanged();
    for (FolderId f : dirtyFolders)
        emit itemsChanged(f);
    for (ItemId i : dirtyItems)
        emit itemUpdated(i);
    if (!errors.isEmpty())
        emit storageError(QStringLiteral("Picazoo ne peut plus enregistrer sa bibliothèque (espace disque "
                                         "insuffisant ?). Vos photos ne sont pas concernées."));
    const ScanProgress p = progress();
    ScanProgress last;
    {
        std::lock_guard lock(notifyMutex_);
        last = lastProgress_;
        lastProgress_ = p;
    }
    if (p.phase != last.phase || p.done != last.done || p.total != last.total || p.paused != last.paused)
        emit progressChanged();
    if (finished)
        emit scanFinished();
}

bool Library::waitForIdle(int timeoutMs, bool includeThumbnails)
{
    QElapsedTimer t;
    t.start();
    int stable = 0;
    while (t.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        const bool idle = indexerIdle() && (!includeThumbnails || pipeline_->idle());
        stable = idle ? stable + 1 : 0;
        if (stable >= 3) {
            flushNotifications();
            QCoreApplication::processEvents();
            return true;
        }
        QThread::msleep(10);
    }
    return false;
}

} // namespace picazoo

#include "FileImageProvider.h"

#include "media/Formats.h"
#include "media/Metadata.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QQuickTextureFactory>
#include <QRunnable>
#include <QUrl>

#include <atomic>

using namespace picazoo;

namespace {

class PhotoResponse : public QQuickImageResponse, public QRunnable {
public:
    PhotoResponse(FileImageProvider& p, QString kind, QString path, qint64 stamp)
        : p_(p), kind_(std::move(kind)), path_(std::move(path)), stamp_(stamp)
    {
        setAutoDelete(false);
    }
    void run() override
    {
        if (!cancelled_.load())
            image_ = p_.load(kind_, path_, stamp_);
        emit finished();
    }
    void cancel() override { cancelled_.store(true); }
    QQuickTextureFactory* textureFactory() const override { return QQuickTextureFactory::textureFactoryForImage(image_); }
    QString errorString() const override
    {
        return image_.isNull() && !cancelled_.load() ? QStringLiteral("Image illisible.") : QString();
    }

private:
    FileImageProvider& p_;
    QString kind_;
    QString path_;
    qint64 stamp_;
    QImage image_;
    std::atomic<bool> cancelled_{false};
};

QImage scaledWithin(const QImage& img, int side)
{
    if (img.isNull() || (img.width() <= side && img.height() <= side))
        return img;
    return img.scaled(side, side, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

} // namespace

FileImageProvider::FileImageProvider()
{
    pool_.setMaxThreadCount(2);
    preloadPool_.setMaxThreadCount(2);
    screen_.setMaxCost(8);
    full_.setMaxCost(1);
    thumbs_.setMaxCost(64);
}

FileImageProvider::~FileImageProvider()
{
    preloadPool_.clear();
    pool_.clear();
    preloadPool_.waitForDone();
    pool_.waitForDone();
}

QString FileImageProvider::makeId(const QString& kind, const QString& path, qint64 stamp)
{
    return kind + u'/' + QString::number(stamp) + u'/' + QString::fromLatin1(path.toUtf8().toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

void FileImageProvider::setScreenSize(QSize s)
{
    std::lock_guard lock(mutex_);
    if (s.isValid() && !s.isEmpty())
        screenSize_ = s;
}

QSize FileImageProvider::screenSize() const
{
    std::lock_guard lock(mutex_);
    return screenSize_;
}

QSize FileImageProvider::fullSize(const QString& path) const
{
    std::lock_guard lock(mutex_);
    return fullSizes_.value(path);
}

QString FileImageProvider::errorKind(const QString& path) const
{
    std::lock_guard lock(mutex_);
    const auto it = errors_.constFind(path);
    if (it == errors_.constEnd())
        return {};
    switch (*it) {
    case DecodeStatus::Ok: return {};
    case DecodeStatus::NoPreview: return QStringLiteral("noPreview");
    case DecodeStatus::Unsupported: return QStringLiteral("unsupported");
    case DecodeStatus::Unreadable: break;
    }
    return QStringLiteral("unreadable");
}

QCache<QString, QImage>& FileImageProvider::cacheFor(const QString& kind)
{
    if (kind == u"full")
        return full_;
    if (kind == u"thumb")
        return thumbs_;
    return screen_;
}

QImage FileImageProvider::load(const QString& kind, const QString& path, qint64 stamp)
{
    const QString key = kind + u'|' + QString::number(stamp) + u'|' + path;
    std::shared_ptr<std::promise<QImage>> promise;
    std::shared_future<QImage> future;
    {
        std::lock_guard lock(mutex_);
        if (QImage* hit = cacheFor(kind).object(key))
            return *hit;
        const auto it = inflight_.constFind(key);
        if (it != inflight_.constEnd()) {
            future = *it;
        } else {
            promise = std::make_shared<std::promise<QImage>>();
            future = promise->get_future().share();
            inflight_.insert(key, future);
        }
    }
    if (!promise)
        return future.get(); // décodage déjà en cours (préchargement)

    QImage img = decode(kind, path);
    {
        std::lock_guard lock(mutex_);
        if (!img.isNull())
            cacheFor(kind).insert(key, new QImage(img), 1);
        inflight_.remove(key);
    }
    promise->set_value(img);
    return img;
}

QImage FileImageProvider::decode(const QString& kind, const QString& path)
{
    QElapsedTimer timer;
    timer.start();
    const Format format = formatFromPath(path);
    DecodeStatus status = DecodeStatus::Unsupported;
    QImage out;
    const bool supported = format != Format::Unknown && kindOf(format) != Kind::Video;
    if (supported) {
        int orientation = 1;
        if (format == Format::Jpeg)
            orientation = Decoder::readMetadata(path, format).orientation;
        if (kind == u"thumb") {
            if (auto t = Decoder::embeddedThumbnail(path, format)) {
                out = scaledWithin(*t, kThumbSide);
                status = DecodeStatus::Ok;
            } else if (auto r = Decoder::decode({path, format, QSize(kThumbSide, kThumbSide), orientation}, &status)) {
                out = scaledWithin(r->image, kThumbSide);
            }
        } else {
            const bool full = kind == u"full";
            const QSize target = full ? QSize() : screenSize();
            if (auto r = Decoder::decode({path, format, target, orientation}, &status)) {
                out = r->image;
                if (full && std::max(out.width(), out.height()) > kMaxFullSide)
                    out = scaledWithin(out, kMaxFullSide);
                std::lock_guard lock(mutex_);
                fullSizes_.insert(path, r->fullSize.isValid() ? r->fullSize : r->image.size());
            }
        }
    }
    if (qEnvironmentVariableIsSet("PICAZOO_VIEWER_TIMING"))
        fprintf(stderr, "picazoo-viewer : décodage %s %s : %lld ms\n", qPrintable(kind),
                qPrintable(QFileInfo(path).fileName()), (long long)timer.elapsed());
    std::lock_guard lock(mutex_);
    if (kind != u"thumb" || out.isNull())
        errors_.insert(path, out.isNull() ? (status == DecodeStatus::Ok ? DecodeStatus::Unreadable : status)
                                          : DecodeStatus::Ok);
    return out;
}

void FileImageProvider::preload(const QString& path, qint64 stamp)
{
    if (path.isEmpty())
        return;
    const QString key = QStringLiteral("screen|") + QString::number(stamp) + u'|' + path;
    {
        std::lock_guard lock(mutex_);
        if (screen_.contains(key) || inflight_.contains(key))
            return;
    }
    preloadPool_.start([this, path, stamp] { load(QStringLiteral("screen"), path, stamp); });
}

QQuickImageResponse* FileImageProvider::requestImageResponse(const QString& id, const QSize& requestedSize)
{
    Q_UNUSED(requestedSize);
    // <kind>/<stamp>/<chemin en base64url>
    const qsizetype a = id.indexOf(u'/');
    const qsizetype b = a < 0 ? -1 : id.indexOf(u'/', a + 1);
    QString kind, path;
    qint64 stamp = 0;
    if (a > 0 && b > a) {
        kind = id.left(a);
        stamp = id.mid(a + 1, b - a - 1).toLongLong();
        path = QString::fromUtf8(QByteArray::fromBase64(id.mid(b + 1).toLatin1(), QByteArray::Base64UrlEncoding));
    }
    auto* r = new PhotoResponse(*this, kind, path, stamp);
    pool_.start(r);
    return r;
}

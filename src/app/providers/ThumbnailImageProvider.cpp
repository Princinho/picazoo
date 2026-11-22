#include "providers/ThumbnailImageProvider.h"

#include "library/Library.h"

#include <QQuickTextureFactory>
#include <QRunnable>

namespace {

class ThumbResponse : public QQuickImageResponse, public QRunnable {
public:
    ThumbResponse(ThumbnailImageProvider& provider, QString id, QSize requested)
        : provider_(provider), id_(std::move(id)), requested_(requested)
    {
        setAutoDelete(false);
    }

    void run() override
    {
        image_ = provider_.load(id_);
        if (!image_.isNull() && requested_.isValid() && !requested_.isEmpty() &&
            (image_.width() > requested_.width() || image_.height() > requested_.height()))
            image_ = image_.scaled(requested_, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        emit finished();
    }

    QQuickTextureFactory* textureFactory() const override
    {
        return QQuickTextureFactory::textureFactoryForImage(image_);
    }

private:
    ThumbnailImageProvider& provider_;
    QString id_;
    QSize requested_;
    QImage image_;
};

} // namespace

ThumbnailImageProvider::ThumbnailImageProvider(picazoo::Library& library) : lib_(library)
{
    pool_.setMaxThreadCount(4);
    cache_.setMaxCost(256 * 1024 * 1024);
}

QImage ThumbnailImageProvider::load(const QString& id)
{
    {
        std::lock_guard lock(mutex_);
        if (QImage* hit = cache_.object(id))
            return *hit;
    }
    // id = "<itemId>/<version>/<état>"
    const QStringList parts = id.split(u'/');
    if (parts.size() < 2)
        return {};
    const auto img = lib_.thumbnail(parts[0].toLongLong(), parts[1].toInt());
    if (!img)
        return {};
    std::lock_guard lock(mutex_);
    cache_.insert(id, new QImage(*img), int(img->sizeInBytes()));
    return *img;
}

QQuickImageResponse* ThumbnailImageProvider::requestImageResponse(const QString& id, const QSize& requestedSize)
{
    auto* response = new ThumbResponse(*this, id, requestedSize);
    pool_.start(response);
    return response;
}

#include "providers/ViewerImageProvider.h"

#include "jobs/ThumbnailPipeline.h"
#include "library/Library.h"
#include "media/Decoder.h"
#include "media/JpegDecoder.h"

#include <QQuickTextureFactory>
#include <QRunnable>

namespace {

class ViewerResponse : public QQuickImageResponse, public QRunnable {
public:
    ViewerResponse(ViewerImageProvider& p, QString id, QSize screen) : p_(p), id_(std::move(id)), screen_(screen)
    {
        setAutoDelete(false);
    }
    void run() override
    {
        image_ = p_.load(id_, screen_);
        emit finished();
    }
    QQuickTextureFactory* textureFactory() const override { return QQuickTextureFactory::textureFactoryForImage(image_); }
    QString errorString() const override
    {
        return image_.isNull() ? QStringLiteral("Picazoo ne sait pas afficher ce fichier.") : QString();
    }

private:
    ViewerImageProvider& p_;
    QString id_;
    QSize screen_;
    QImage image_;
};

} // namespace

ViewerImageProvider::ViewerImageProvider(picazoo::Library& library) : lib_(library)
{
    pool_.setMaxThreadCount(2);
    preloadPool_.setMaxThreadCount(2);
    cache_.setMaxCost(5);
}

ViewerImageProvider::~ViewerImageProvider()
{
    preloadPool_.clear();
    preloadPool_.waitForDone();
    pool_.waitForDone();
}

void ViewerImageProvider::setScreenSize(QSize s)
{
    std::lock_guard lock(mutex_);
    if (s.isValid() && !s.isEmpty())
        screen_ = s;
}

QSize ViewerImageProvider::screenSize() const
{
    std::lock_guard lock(mutex_);
    return screen_;
}

QImage ViewerImageProvider::load(const QString& id, QSize screen)
{
    {
        std::lock_guard lock(mutex_);
        if (QImage* hit = cache_.object(id))
            return *hit;
    }
    const QStringList parts = id.split(u'/');
    if (parts.size() < 3)
        return {};
    const auto item = lib_.item(parts[0].toLongLong());
    if (!item)
        return {};
    const bool full = parts[2] == u"full";
    picazoo::DecodeRequest req{item->path(), item->format, full ? QSize() : screen, item->orientation};
    lib_.pipeline().hold(); // l'image affichée passe avant les vignettes
    auto r = picazoo::Decoder::decode(req);
    lib_.pipeline().release();
    if (!r || r->image.isNull())
        return {};
    QImage img = r->image;
    if (full && std::max(img.width(), img.height()) > kMaxFullSide)
        img = picazoo::fitInto(img, QSize(kMaxFullSide, kMaxFullSide));
    std::lock_guard lock(mutex_);
    cache_.insert(id, new QImage(img), 1);
    return img;
}

void ViewerImageProvider::preload(qint64 itemId, int version, QSize screen)
{
    const QString id = QStringLiteral("%1/%2/screen").arg(itemId).arg(version);
    {
        std::lock_guard lock(mutex_);
        if (cache_.contains(id))
            return;
    }
    preloadPool_.start([this, id, screen] { load(id, screen); });
}

QQuickImageResponse* ViewerImageProvider::requestImageResponse(const QString& id, const QSize& requestedSize)
{
    const QSize screen = requestedSize.isValid() && !requestedSize.isEmpty() ? requestedSize : screenSize();
    auto* r = new ViewerResponse(*this, id, screen);
    pool_.start(r);
    return r;
}

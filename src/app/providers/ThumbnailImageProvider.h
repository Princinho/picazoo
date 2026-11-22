#pragma once

// Fournisseur asynchrone « image://thumb/<id>/<version>/<état> » (T040).
// Cache LRU de QImage plafonné à 256 Mo devant thumbs.db.

#include <QCache>
#include <QImage>
#include <QQuickAsyncImageProvider>
#include <QThreadPool>

#include <mutex>

namespace picazoo {
class Library;
}

class ThumbnailImageProvider : public QQuickAsyncImageProvider {
public:
    explicit ThumbnailImageProvider(picazoo::Library& library);
    QQuickImageResponse* requestImageResponse(const QString& id, const QSize& requestedSize) override;

    QImage load(const QString& id);

private:
    picazoo::Library& lib_;
    QThreadPool pool_;
    std::mutex mutex_;
    QCache<QString, QImage> cache_;
};

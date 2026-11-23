#pragma once

// Fournisseur « image://photo/<kind>/<stamp>/<chemin en base64url> » de la visionneuse
// autonome (research V8). kind : screen (taille de l'écran), full (pleine
// résolution, côté plafonné), thumb (160 px, bande des voisines). Tout reste en
// mémoire ; les fichiers sont lus en lecture seule et partagée.

#include "media/Decoder.h"

#include <QCache>
#include <QHash>
#include <QImage>
#include <QQuickAsyncImageProvider>
#include <QSize>
#include <QThreadPool>

#include <future>
#include <mutex>

class FileImageProvider : public QQuickAsyncImageProvider {
public:
    FileImageProvider();
    ~FileImageProvider() override;

    QQuickImageResponse* requestImageResponse(const QString& id, const QSize& requestedSize) override;

    static QString makeId(const QString& kind, const QString& path, qint64 stamp);

    // Décodage bloquant (thread appelant), avec cache et sans double décodage.
    QImage load(const QString& kind, const QString& path, qint64 stamp);
    // Décodage anticipé à la taille de l'écran, en arrière-plan.
    void preload(const QString& path, qint64 stamp);

    void setScreenSize(QSize s);
    QSize screenSize() const;

    // Dimensions pleine résolution (orientées) connues après décodage, sinon vide.
    QSize fullSize(const QString& path) const;
    // "" si décodé, sinon "unreadable", "noPreview" ou "unsupported".
    QString errorKind(const QString& path) const;

    static constexpr int kMaxFullSide = 16384;
    static constexpr int kThumbSide = 160;

private:
    QImage decode(const QString& kind, const QString& path);
    QCache<QString, QImage>& cacheFor(const QString& kind);

    QThreadPool pool_;
    QThreadPool preloadPool_;
    mutable std::mutex mutex_;
    QCache<QString, QImage> screen_;
    QCache<QString, QImage> full_;
    QCache<QString, QImage> thumbs_;
    QHash<QString, std::shared_future<QImage>> inflight_;
    QHash<QString, QSize> fullSizes_;
    QHash<QString, picazoo::DecodeStatus> errors_;
    QSize screenSize_{1920, 1080};
};

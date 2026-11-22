#pragma once

// Fournisseur de la visionneuse « image://viewer/<id>/<version>/<screen|full> » (T053, R8).
// screen : décodage réduit à la taille de l'écran (facteur TurboJPEG le plus proche
// par excès) ; full : pleine résolution, au premier zoom au-delà de l'aperçu.
// Cache de 5 images ; préchargement des voisins.

#include <QCache>
#include <QImage>
#include <QQuickAsyncImageProvider>
#include <QSize>
#include <QThreadPool>

#include <mutex>

namespace picazoo {
class Library;
}

class ViewerImageProvider : public QQuickAsyncImageProvider {
public:
    explicit ViewerImageProvider(picazoo::Library& library);
    ~ViewerImageProvider() override;

    QQuickImageResponse* requestImageResponse(const QString& id, const QSize& requestedSize) override;

    // Décodage (thread appelant). Utilise le cache.
    QImage load(const QString& id, QSize screen);
    // Préchargement en arrière-plan à la taille de l'écran.
    void preload(qint64 itemId, int version, QSize screen);
    void setScreenSize(QSize s);
    QSize screenSize() const;

    // Côté le plus long d'une image « full » (limite des textures).
    static constexpr int kMaxFullSide = 16384;

private:
    picazoo::Library& lib_;
    QThreadPool pool_;
    QThreadPool preloadPool_;
    mutable std::mutex mutex_;
    QCache<QString, QImage> cache_;
    QSize screen_{1920, 1080};
};

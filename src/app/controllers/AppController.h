#pragma once

// Pont entre le cœur et le QML : dossiers surveillés, analyse, préférences,
// actions sur les éléments (Explorateur, application par défaut).

#include "platform/win/Wallpaper.h"

#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QVariantList>

namespace picazoo {
class Library;
}

class AppController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList roots READ roots NOTIFY rootsChanged)
    Q_PROPERTY(bool scanActive READ scanActive NOTIFY progressChanged)
    Q_PROPERTY(bool scanPaused READ scanPaused NOTIFY progressChanged)
    Q_PROPERTY(QString scanText READ scanText NOTIFY progressChanged)
    Q_PROPERTY(double scanProgress READ scanProgress NOTIFY progressChanged) // 0–1, ou -1 si inconnue
    Q_PROPERTY(QString folderSort READ folderSort WRITE setFolderSort NOTIFY sortChanged)
    Q_PROPERTY(QString itemSort READ itemSort WRITE setItemSort NOTIFY sortChanged)
    Q_PROPERTY(qint64 lastFolderId READ lastFolderId WRITE setLastFolderId NOTIFY lastFolderChanged)
    Q_PROPERTY(QString version READ version CONSTANT)
    // 007 : incrémenté quand une réponse « ce disque a-t-il une corbeille ? » arrive.
    Q_PROPERTY(int recycleRevision READ recycleRevision NOTIFY recycleInfoChanged)

public:
    explicit AppController(picazoo::Library& library, QObject* parent = nullptr);

    QVariantList roots() const;
    bool scanActive() const;
    bool scanPaused() const;
    QString scanText() const;
    double scanProgress() const;
    QString folderSort() const;
    void setFolderSort(const QString& s);
    QString itemSort() const;
    void setItemSort(const QString& s);
    qint64 lastFolderId() const;
    QString version() const;
    void setLastFolderId(qint64 id);

    Q_INVOKABLE void addRoot(const QUrl& folder);
    Q_INVOKABLE void addRootPath(const QString& folder);
    // « Ouvrir dans Picazoo » depuis la visionneuse autonome (005, research V12) : montre
    // la photo si son dossier est dans la bibliothèque, sinon propose d'ajouter le dossier.
    void showPath(const QString& path);
    Q_INVOKABLE void removeRoot(qint64 rootId);
    Q_INVOKABLE void excludeFolder(qint64 rootId, const QUrl& folder);
    Q_INVOKABLE void includeFolder(qint64 rootId, const QString& path);
    Q_INVOKABLE void rescan();
    Q_INVOKABLE void togglePause();
    Q_INVOKABLE void revealInExplorer(qint64 itemId);
    Q_INVOKABLE void openWithDefaultApp(qint64 itemId);
    // Ouvre l'élément dans la visionneuse autonome plein écran (005, picazoo-viewer.exe).
    // Faux si la visionneuse est introuvable ou n'a pas pu démarrer.
    Q_INVOKABLE bool openInStandaloneViewer(qint64 itemId);
    Q_INVOKABLE QString itemPath(qint64 itemId) const;
    Q_INVOKABLE QVariantMap itemInfo(qint64 itemId) const { return describe(lib_, itemId); }
    static QVariantMap describe(picazoo::Library& lib, qint64 itemId);
    Q_INVOKABLE static QString formatDuration(qint64 ms);
    Q_INVOKABLE static QString formatBytes(qint64 bytes);
    Q_INVOKABLE static QString urlToPath(const QUrl& url);

    // --- 007 : actions des menus contextuels (contracts/ui-contract.md)
    // Copier : une photo -> image + fichier ; sinon fichiers seulement (coller copie, ne déplace jamais).
    Q_INVOKABLE void copyItems(const QVariantList& ids);
    Q_INVOKABLE void copyPaths(const QVariantList& ids);
    Q_INVOKABLE void copyFolderPath(const QString& path);
    Q_INVOKABLE void showProperties(const QVariantList& ids);
    Q_INVOKABLE void revealFolder(const QString& path);
    Q_INVOKABLE bool canSetWallpaper(qint64 itemId) const;
    Q_INVOKABLE void setWallpaper(qint64 itemId);
    Q_INVOKABLE void undoWallpaper();
    // 1 : tous sur un disque avec corbeille ; 0 : au moins un sans ; -1 : pas encore connu (la
    // réponse arrive par recycleInfoChanged, sans bloquer l'interface).
    Q_INVOKABLE int recycleState(const QVariantList& ids);
    int recycleRevision() const { return recycleRevision_; }

signals:
    void rootsChanged();
    void progressChanged();
    void sortChanged();
    void lastFolderChanged();
    void storageError(const QString& message);
    void showItem(qint64 itemId);
    void showMessage(const QString& message);
    void askAddFolder(const QString& folder);
    void recycleInfoChanged();
    // Fond d'écran changé (`ok`) ; `restorable` : « Annuler » peut rétablir l'ancien.
    void wallpaperChanged(bool ok, bool restorable);
    void wallpaperRestored(bool ok);

private:
    QStringList pathsOf(const QVariantList& ids) const;

    picazoo::Library& lib_;
    int recycleRevision_ = 0;
    std::optional<picazoo::platform::WallpaperSnapshot> wallpaperBefore_;
};

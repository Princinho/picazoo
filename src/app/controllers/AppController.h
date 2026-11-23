#pragma once

// Pont entre le cœur et le QML : dossiers surveillés, analyse, préférences,
// actions sur les éléments (Explorateur, application par défaut).

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
    Q_INVOKABLE QString itemPath(qint64 itemId) const;
    Q_INVOKABLE QVariantMap itemInfo(qint64 itemId) const { return describe(lib_, itemId); }
    static QVariantMap describe(picazoo::Library& lib, qint64 itemId);
    Q_INVOKABLE static QString formatDuration(qint64 ms);
    Q_INVOKABLE static QString formatBytes(qint64 bytes);
    Q_INVOKABLE static QString urlToPath(const QUrl& url);

signals:
    void rootsChanged();
    void progressChanged();
    void sortChanged();
    void lastFolderChanged();
    void storageError(const QString& message);
    void showItem(qint64 itemId);
    void showMessage(const QString& message);
    void askAddFolder(const QString& folder);

private:
    picazoo::Library& lib_;
};

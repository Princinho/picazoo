#pragma once

// Façade du cœur (contracts/core-api.md, T039). Vit sur le thread de l'interface ;
// l'analyse tourne sur un thread d'indexation, les vignettes sur un pool de calcul.
// Garantie : aucune écriture dans les dossiers de l'utilisateur.

#include "index/LibraryDb.h"
#include "jobs/MemoryBudget.h"
#include "library/Types.h"

#include <QImage>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <QVariant>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

namespace picazoo {

class Database;
class Settings;
class ThumbStore;
class ThumbnailPipeline;

class Library : public QObject {
    Q_OBJECT

public:
    // Ouvre (ou crée) la bibliothèque dans `dataDir`. Lève DbError si le stockage est inaccessible.
    static std::unique_ptr<Library> open(const QString& dataDir, QObject* parent = nullptr);
    ~Library() override;

    // --- Dossiers surveillés (FR-001, FR-002)
    QList<WatchedRoot> roots() const;
    RootId addRoot(const QString& path);
    void removeRoot(RootId id);                 // index seulement, jamais le disque
    void excludePath(RootId id, const QString& subPath);
    void includePath(RootId id, const QString& subPath);
    static QStringList defaultRoots();          // Images, Bureau, Documents

    // --- Analyse (FR-005, FR-007)
    void startScan();                           // toutes les racines, en arrière-plan
    void scanRoot(RootId id);
    void pauseScan();
    void resumeScan();
    bool isPaused() const;
    ScanProgress progress() const;

    // --- Lectures (thread de l'interface ; n'attendent jamais l'indexeur)
    QList<Folder> folders(FolderSort sort) const;
    QList<ItemSummary> itemSummaries(FolderId folder, ItemSort sort) const;
    QHash<FolderId, QList<ItemSummary>> allItemSummaries(ItemSort sort) const;
    QList<ItemId> items(FolderId folder, ItemSort sort) const;
    std::optional<Item> item(ItemId id) const;
    qint64 itemCount() const;
    QHash<RootId, qint64> rootItemCounts() const;

    // --- Vignettes (R6)
    void prioritize(const QList<ItemId>& visible, const QList<ItemId>& nearby);
    // Vignette finale, sinon provisoire, pour cette version de l'élément. Thread-safe.
    std::optional<QImage> thumbnail(ItemId id, int version) const;

    // --- Préférences
    QVariant setting(const QString& key) const;
    void setSetting(const QString& key, const QVariant& value);

    // Vignettes et durées des vidéos via Qt Multimedia (US3). À appeler une fois.
    void enableVideoThumbnails();

    // Accès pour les modules qui s'appuient sur le cœur (US3, US4) et les tests.
    ThumbnailPipeline& pipeline() { return *pipeline_; }
    QString dataDir() const { return dataDir_; }
    // Budget mémoire du scan (008), calculé une fois à l'ouverture (research B2).
    MemoryBudget memoryBudget() const { return memoryBudget_; }

    // Tests et banc d'essai : traite les événements jusqu'à ce que l'analyse et les
    // vignettes soient terminées. Faux si le délai expire.
    bool waitForIdle(int timeoutMs, bool includeThumbnails = true);
    bool indexerIdle() const;

    // Exécute une tâche sur le thread d'indexation (sérialisée avec les analyses).
    void post(std::function<void()> task);

    // Retire ces éléments de l'index et leurs vignettes, sans toucher au disque (fichiers
    // mis à la corbeille par la 002). Asynchrone, sur le thread d'indexation ; `done` y est
    // appelée une fois les éléments retirés de l'index.
    void forgetItems(const QList<ItemId>& ids, std::function<void()> done = {});

    // Réessaie les racines indisponibles (toutes les 30 s, et sur WM_DEVICECHANGE).
    void checkRoots();
    // Vrai si la racine est surveillée en direct (US4).
    bool isWatching(RootId id) const;

signals:
    void foldersChanged();
    void itemsChanged(qint64 folderId);
    void itemUpdated(qint64 itemId);
    void rootsChanged();
    void rootStateChanged(qint64 rootId, int state);
    void progressChanged();
    void scanFinished();
    void storageError(const QString& message);

private:
    explicit Library(const QString& dataDir, QObject* parent);
    void indexerLoop();
    void runScan(const QList<RootId>& only);
    void partialScan(const QHash<RootId, QStringList>& paths);
    void applyOutcome(const struct ScanOutcome& out);
    void refreshWatchers();
    void flushNotifications();
    void reviewMemoryBudget();
    void noteFolders(const QSet<FolderId>& folders, bool structure);
    void noteItems(const QList<ItemId>& ids);
    void reportStorageError(const QString& what);
    qint64 nextStamp();

    QString dataDir_;
    std::unique_ptr<Database> writeDb_;   // indexeur + pipeline (sérialisé)
    std::unique_ptr<Database> uiDb_;      // lectures de l'interface
    std::unique_ptr<Database> workerDb_;  // lectures du pipeline
    std::unique_ptr<ThumbStore> thumbs_;
    std::unique_ptr<Settings> settings_;
    std::unique_ptr<LibraryDb> repo_;
    std::unique_ptr<ThumbnailPipeline> pipeline_;
    MemoryBudget memoryBudget_;
    std::unique_ptr<class VideoProbe> video_;
    std::unique_ptr<class Watcher> watcher_;
    QTimer availabilityTimer_;
    // 008 (research B3) : relit la RAM disponible pendant un scan actif et ajuste le
    // nombre de fils de vignettes en conséquence.
    QTimer memoryBudgetTimer_;

    // Thread d'indexation
    std::thread indexer_;
    mutable std::mutex tasksMutex_;
    std::condition_variable tasksCv_;
    std::deque<std::function<void()>> tasks_;
    bool stopIndexer_ = false;
    std::atomic<bool> indexerBusy_{false};
    std::atomic<bool> cancelScan_{false};
    std::atomic<bool> paused_{false};
    std::mutex pauseMutex_;
    std::condition_variable pauseCv_;
    qint64 lastStamp_ = 0;

    // Progression
    std::atomic<int> phase_{int(ScanProgress::Phase::Idle)};
    std::atomic<qint64> foldersScanned_{0};
    mutable std::atomic<qint64> thumbsBase_{0};

    // Notifications regroupées (au plus toutes les 250 ms)
    mutable std::mutex notifyMutex_;
    bool foldersDirty_ = false;
    bool rootsDirty_ = false;
    bool scanFinishedPending_ = false;
    QSet<FolderId> dirtyFolders_;
    QSet<ItemId> dirtyItems_;
    QList<QString> storageErrors_;
    QTimer notifyTimer_;
    ScanProgress lastProgress_;
};

} // namespace picazoo

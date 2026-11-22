#pragma once

// Dépôt de l'index library.db (T036, T066) : racines, dossiers, éléments, rattrapage.
// Les écritures passent par une seule connexion, en transactions par lots.

#include "library/Types.h"
#include "media/Metadata.h"
#include "platform/win/FileIdentity.h"
#include "scan/Discovery.h"

#include <QHash>
#include <QList>
#include <QSet>

#include <functional>
#include <optional>

namespace picazoo {

class Database;

struct ItemSummary {
    ItemId id = 0;
    FolderId folderId = 0;
    QString fileName;
    Kind kind = Kind::Image;
    ThumbState thumbState = ThumbState::Pending;
    int version = 1;
    std::optional<qint64> durationMs;
};

struct SyncResult {
    FolderId folderId = 0;
    QList<ItemId> created;
    QList<ItemId> changed;
};

struct ScanOutcome {
    QList<ItemId> removed;        // supprimés de l'index (fichier disparu)
    QList<ItemId> mergedAway;     // lignes nouvelles fusionnées dans un élément existant (renommage)
    QList<ItemId> moved;          // éléments conservés à un nouvel emplacement
    QSet<FolderId> touchedFolders;
};

// Résultat du pipeline pour un élément.
struct ItemUpdate {
    ItemId id = 0;
    int version = 1; // version traitée : ignorée si l'élément a changé entre-temps
    std::optional<Metadata> meta;
    bool metaFailed = false;
    std::optional<ThumbState> thumbState;
    std::optional<quint64> quickHash;
    std::optional<qint64> durationMs;
};

using IdentityFn = std::function<std::optional<platform::FileId>(const QString& path)>;
using QuickHashFn = std::function<std::optional<quint64>(const QString& path, qint64 size)>;

class LibraryDb {
public:
    explicit LibraryDb(Database& writeDb) : db_(writeDb) {}

    // --- Racines (FR-001, FR-002)
    static QList<WatchedRoot> roots(Database& read);
    RootId addRoot(const QString& normalizedPath, qint64 now);
    QList<ItemId> removeRoot(RootId id);
    void setRootState(RootId id, RootState state);
    void setRootScanned(RootId id, qint64 when);
    void addExcluded(RootId id, const QString& path);
    void removeExcluded(RootId id, const QString& path);
    // Retire de l'index (jamais du disque) tout ce qui se trouve sous `path`.
    QList<ItemId> removeItemsUnder(const QString& path, QSet<FolderId>* touched = nullptr);

    // --- Analyse
    SyncResult syncFolder(const DiscoveredFolder& folder, qint64 stamp, const IdentityFn& identity);
    // Rattrapage (règles 1 à 5 de data-model.md) sur les racines parcourues.
    // `folderScope` non vide : seulement ces dossiers (surveillance en direct).
    ScanOutcome finishScan(const QList<RootId>& roots, const QStringList& skippedPaths, qint64 stamp,
                           const QList<ItemId>& createdThisScan, const QuickHashFn& quickHash,
                           const QList<FolderId>& folderScope = {});
    void refreshFolders(const QSet<FolderId>& folders);

    // --- Pipeline
    QSet<FolderId> applyUpdates(const QList<ItemUpdate>& updates);
    QList<ItemId> pendingWork(bool includeVideos) const;

    // --- Lectures
    static QList<Folder> folders(Database& read, FolderSort sort);
    static QList<ItemSummary> itemSummaries(Database& read, FolderId folder, ItemSort sort);
    // Tous les éléments, groupés par dossier, dans l'ordre de tri (une seule requête).
    static QHash<FolderId, QList<ItemSummary>> allItemSummaries(Database& read, ItemSort sort);
    static std::optional<Item> item(Database& read, ItemId id);
    static std::optional<FolderId> folderIdForPath(Database& read, const QString& path);
    static qint64 itemCount(Database& read);
    // Nombre d'éléments par racine surveillée.
    static QHash<RootId, qint64> rootItemCounts(Database& read);
    // Tous les dossiers (identifiant, chemin), pour les mises à jour partielles.
    QList<QPair<FolderId, QString>> folderPaths() const;

private:
    Database& db_;
};

} // namespace picazoo

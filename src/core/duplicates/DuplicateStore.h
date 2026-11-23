#pragma once

// Lectures et décisions sur les doublons pour l'interface (contracts/core-api.md de la 002 et de
// la 006). Connexions propres à library.db (lecture + écriture des décisions) ; ne lit jamais un
// fichier de l'utilisateur. Les décisions qui changent les groupes préviennent le moteur, qui les
// recalcule.

#include "duplicates/DuplicateTypes.h"

#include <QList>
#include <QPair>
#include <QString>

#include <functional>
#include <memory>
#include <optional>

namespace picazoo {

class Database;

class DuplicateStore {
public:
    explicit DuplicateStore(const QString& libraryDbPath);
    ~DuplicateStore();

    // Appelé après chaque décision qui demande de recalculer les groupes.
    void setOnChanged(std::function<void()> fn) { onChanged_ = std::move(fn); }

    // --- Lectures (groupes « à revoir » ; les groupes « Garder toutes » sont masqués)
    QList<GroupId> groups(LevelFilter filter = LevelFilter::All) const;
    QList<GroupId> groups(const GroupQuery& query) const; // 006 : filtres combinés et tri
    int count(const GroupQuery& query) const;
    std::optional<DuplicateGroup> group(GroupId id) const;
    std::optional<GroupId> groupOf(ItemId item) const;
    int duplicateCount(ItemId item) const; // autres copies du groupe de `item`
    DuplicateSummary summary(LevelFilter filter = LevelFilter::All) const;

    // 006 : fichiers « à la corbeille » (choix ou suggestion) et leurs octets.
    SelectionTotals totals(const QList<GroupId>& groups) const;
    SelectionTotals trashTotals() const; // tous les groupes « à revoir », en une requête
    // Dossiers des copies de ces groupes, avec le nombre de groupes où ils apparaissent.
    QList<QPair<QString, int>> foldersOf(const QList<GroupId>& groups, int limit = 20) const;

    // --- Décisions unitaires (002, FR-013, FR-014)
    // Choix « à garder » (true) ou « à la corbeille » (false). Refuse (faux) si plus aucune
    // copie du groupe ne serait gardée (FR-017).
    bool setKeep(GroupId group, ItemId item, bool keep);
    void resetChoice(GroupId group);
    void markNotDuplicates(GroupId group);
    void removeFromGroup(GroupId group, ItemId item);
    void keepAll(GroupId group);
    // Annule les décisions de paires qui concernent ces éléments (« Ce ne sont pas des
    // doublons », « Garder toutes ») et les choix de copie.
    void undoDecisions(const QList<ItemId>& items);

    // --- 006 : actions en lot (research R4). Une transaction, un lot, une notification.
    BatchResult applyBatch(BatchKind kind, const QList<GroupId>& groups, const QString& folder = {});
    UndoResult undoBatch(qint64 batchId);
    QList<BatchInfo> batches(int limit = 50) const;

    // --- 006 : règles de dossier (research R1–R3) ; recalculent les suggestions.
    QList<FolderRule> folderRules() const;
    qint64 setFolderRule(const QString& path, RuleKind kind);
    void removeFolderRule(qint64 id);
    void relinkFolderRule(qint64 id, const QString& path);

    // --- 006 : paires de dossiers et doublons internes (research R6)
    QList<FolderPair> folderPairs(int limit = 50) const;
    QList<FolderStat> internalDuplicates(int limit = 50) const;

    // --- 006 : « Écartés de la revue » (US5)
    QList<SetAsideEntry> setAside() const;
    void restore(const QList<SetAsideEntry>& entries);

private:
    void pairDecision(const QList<ItemId>& a, const QList<ItemId>& b, const char* kind, qint64 batchId = 0);
    void refreshReclaimable(GroupId group);
    void changed();

    std::unique_ptr<Database> read_;
    std::unique_ptr<Database> write_;
    std::function<void()> onChanged_;
};

// Requête commune : place récupérable d'un groupe selon les choix actuels et la suggestion.
extern const char* const kReclaimableSql;

} // namespace picazoo

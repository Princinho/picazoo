#pragma once

// Lectures et décisions sur les doublons pour l'interface (contracts/core-api.md). Connexions
// propres à library.db (lecture + écriture des décisions) ; ne lit jamais un fichier de
// l'utilisateur. Les décisions préviennent le moteur, qui recalcule les groupes.

#include "duplicates/DuplicateTypes.h"

#include <QList>
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

    // Appelé après chaque décision (le moteur recalcule les groupes).
    void setOnChanged(std::function<void()> fn) { onChanged_ = std::move(fn); }

    // --- Lectures (groupes « à revoir » ; les groupes « Garder toutes » sont masqués)
    QList<GroupId> groups(LevelFilter filter = LevelFilter::All) const;
    std::optional<DuplicateGroup> group(GroupId id) const;
    std::optional<GroupId> groupOf(ItemId item) const;
    int duplicateCount(ItemId item) const; // autres copies du groupe de `item`
    DuplicateSummary summary(LevelFilter filter = LevelFilter::All) const;

    // --- Décisions (FR-013, FR-014)
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

private:
    void pairDecision(const QList<ItemId>& a, const QList<ItemId>& b, const char* kind);
    void refreshReclaimable(GroupId group);
    void changed();

    std::unique_ptr<Database> read_;
    std::unique_ptr<Database> write_;
    std::function<void()> onChanged_;
};

// Requête commune : place récupérable d'un groupe selon les choix actuels.
extern const char* const kReclaimableSql;

} // namespace picazoo

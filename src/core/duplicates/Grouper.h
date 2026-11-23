#pragma once

// Regroupement en étoile (FR-007, research D7) : chaque groupe a pour centre sa meilleure
// copie (règle de KeeperRule), et ne contient que des copies identiques au centre ou dont la
// paire avec le centre a été vérifiée. Aucune chaîne A–B–C où A et C ne se ressemblent pas.
// Logique pure, sans base de données.

#include "duplicates/DuplicateTypes.h"
#include "duplicates/KeeperRule.h"

#include <QHash>
#include <QList>
#include <QSet>

#include <functional>

namespace picazoo {

struct VisualEdge {
    ItemId a = 0;
    ItemId b = 0;
    DupLevel level = DupLevel::SameImage; // SameImage ou Similar
    double similarity = 0;
    int rotation = 0; // rotation (degrés) à appliquer à `b` pour retrouver `a`
};

struct GroupMember {
    ItemId item = 0;
    DupLevel level = DupLevel::Identical; // par rapport au centre
    std::optional<double> similarity;
    int rotation = 0;
};

struct BuiltGroup {
    ItemId center = 0;
    KeeperReason reason = KeeperReason::Path;
    DupLevel level = DupLevel::Identical;
    QList<GroupMember> members; // le centre en premier
};

class Grouper {
public:
    using Forbidden = std::function<bool(ItemId, ItemId)>; // paire « Ce ne sont pas des doublons »

    // `facts` : toutes les copies citées dans `exactSets` et `edges`.
    // `exactSets` : ensembles de contenu identique (liens physiques déjà fusionnés).
    static QList<BuiltGroup> build(const QHash<ItemId, CopyFacts>& facts, const QList<QList<ItemId>>& exactSets,
                                   const QList<VisualEdge>& edges, const Forbidden& forbidden = {});
};

} // namespace picazoo

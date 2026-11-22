#include "duplicates/Grouper.h"

#include <algorithm>

namespace picazoo {

namespace {

// Ensemble de copies identiques, représenté par sa meilleure copie.
struct Unit {
    ItemId rep = 0;
    QList<ItemId> items; // rep en premier, puis ordre de KeeperRule
};

struct UnitEdge {
    DupLevel level = DupLevel::SameImage;
    double similarity = 0;
    int rotation = 0; // à appliquer à l'unité « autre » pour retrouver celle-ci
};

quint64 key(int a, int b) { return (quint64(quint32(a)) << 32) | quint32(b); }

} // namespace

QList<BuiltGroup> Grouper::build(const QHash<ItemId, CopyFacts>& facts, const QList<QList<ItemId>>& exactSets,
                                 const QList<VisualEdge>& edges, const Forbidden& forbidden)
{
    auto isForbidden = [&](ItemId a, ItemId b) { return forbidden && a != b && forbidden(a, b); };
    auto factsOf = [&](ItemId id) {
        auto it = facts.constFind(id);
        if (it != facts.constEnd())
            return *it;
        CopyFacts f;
        f.id = id;
        return f;
    };
    auto sortIds = [&](QList<ItemId>& ids) {
        QList<CopyFacts> list;
        for (ItemId id : ids)
            list << factsOf(id);
        KeeperRule::sort(list);
        ids.clear();
        for (const auto& f : list)
            ids << f.id;
    };

    // 1) Unités : ensembles identiques, découpés si l'utilisateur a séparé certaines copies.
    QList<Unit> units;
    QHash<ItemId, int> unitOf;
    for (QList<ItemId> set : exactSets) {
        sortIds(set);
        while (!set.isEmpty()) {
            Unit u;
            u.rep = set.takeFirst();
            u.items << u.rep;
            for (qsizetype i = 0; i < set.size();) {
                if (!isForbidden(u.rep, set[i]))
                    u.items << set.takeAt(i);
                else
                    ++i;
            }
            for (ItemId id : u.items)
                unitOf.insert(id, int(units.size()));
            units << u;
        }
    }
    auto unitFor = [&](ItemId id) {
        auto it = unitOf.constFind(id);
        if (it != unitOf.constEnd())
            return *it;
        Unit u;
        u.rep = id;
        u.items << id;
        unitOf.insert(id, int(units.size()));
        units << u;
        return int(units.size() - 1);
    };

    // 2) Arêtes visuelles entre unités (la meilleure si plusieurs).
    QHash<quint64, UnitEdge> unitEdges;
    QHash<int, QList<int>> neighbours;
    for (const VisualEdge& e : edges) {
        const int ua = unitFor(e.a), ub = unitFor(e.b);
        if (ua == ub)
            continue;
        auto put = [&](int from, int to, int rotation) {
            const quint64 k = key(from, to);
            auto it = unitEdges.find(k);
            if (it == unitEdges.end()) {
                unitEdges.insert(k, UnitEdge{e.level, e.similarity, rotation});
                neighbours[from] << to;
            } else if (e.level < it->level || (e.level == it->level && e.similarity > it->similarity)) {
                *it = UnitEdge{e.level, e.similarity, rotation};
            }
        };
        put(ua, ub, e.rotation);               // tourner b de r retrouve a
        put(ub, ua, (360 - e.rotation) % 360); // tourner a de -r retrouve b
    }

    // 3) Étoiles : la meilleure unité restante devient centre ; ses voisines directes la rejoignent.
    QList<int> order;
    for (int i = 0; i < units.size(); ++i)
        order << i;
    std::stable_sort(order.begin(), order.end(),
                     [&](int x, int y) { return KeeperRule::compare(factsOf(units[x].rep), factsOf(units[y].rep)) < 0; });
    QSet<int> assigned;
    QList<BuiltGroup> out;
    for (int u : order) {
        if (assigned.contains(u))
            continue;
        assigned.insert(u);
        const Unit& cu = units[u];
        BuiltGroup g;
        g.center = cu.rep;
        for (ItemId id : cu.items)
            g.members << GroupMember{id, DupLevel::Identical, std::nullopt, 0};

        QList<int> joined;
        QList<int> cand = neighbours.value(u);
        std::stable_sort(cand.begin(), cand.end(),
                         [&](int x, int y) { return KeeperRule::compare(factsOf(units[x].rep), factsOf(units[y].rep)) < 0; });
        for (int v : cand) {
            if (assigned.contains(v))
                continue;
            const Unit& vu = units[v];
            const bool blocked = std::any_of(vu.items.begin(), vu.items.end(), [&](ItemId x) {
                return std::any_of(cu.items.begin(), cu.items.end(), [&](ItemId y) { return isForbidden(x, y); });
            });
            if (blocked)
                continue;
            const UnitEdge e = unitEdges.value(key(u, v));
            assigned.insert(v);
            joined << v;
            for (ItemId id : vu.items)
                g.members << GroupMember{id, e.level, e.similarity, e.rotation};
        }
        if (g.members.size() < 2)
            continue;
        g.level = DupLevel::Identical;
        for (const GroupMember& m : g.members)
            g.level = std::max(g.level, m.level);
        // Raison : ce qui départage le centre de la deuxième meilleure copie du groupe.
        QList<CopyFacts> list;
        for (const GroupMember& m : g.members)
            list << factsOf(m.item);
        KeeperRule::sort(list);
        g.reason = KeeperRule::reason(list);
        out << g;
    }
    return out;
}

} // namespace picazoo

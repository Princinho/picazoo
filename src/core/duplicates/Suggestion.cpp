#include "duplicates/Suggestion.h"

#include "index/Database.h"
#include "platform/win/Paths.h"

#include <QHash>
#include <QSet>

namespace picazoo {

const char* const kReclaimableAllSql =
    "UPDATE dup_group SET reclaimable_bytes = ("
    " SELECT COALESCE(SUM(i.size_bytes), 0) FROM dup_member m JOIN item i ON i.id = m.item_id"
    " LEFT JOIN dup_choice c ON c.item_id = m.item_id"
    " WHERE m.group_id = dup_group.id AND COALESCE(c.keep, m.suggested_keep) = 0)";

namespace {

QString idList(const QList<GroupId>& ids)
{
    QString s;
    for (GroupId id : ids) {
        if (!s.isEmpty())
            s += u',';
        s += QString::number(id);
    }
    return s;
}

const CopyFacts* best(const QList<const CopyFacts*>& copies)
{
    const CopyFacts* b = nullptr;
    for (const CopyFacts* c : copies)
        if (!b || KeeperRule::compare(*c, *b) < 0)
            b = c;
    return b;
}

} // namespace

SuggestionOutcome Suggestion::choose(DupLevel level, ItemId center, const QList<CopyFacts>& copies,
                                     const FolderRules& rules)
{
    SuggestionOutcome quality;
    quality.keeper = center;
    if (rules.isEmpty() || copies.isEmpty())
        return quality;

    QList<const CopyFacts*> preferred, neutral, avoided;
    QHash<ItemId, const FolderRule*> ruleOf;
    for (const CopyFacts& c : copies) {
        const FolderRule* r = rules.match(platform::parentPath(c.path));
        ruleOf.insert(c.id, r);
        if (!r)
            neutral << &c;
        else if (r->kind == RuleKind::Prefer)
            preferred << &c;
        else
            avoided << &c;
    }
    if (preferred.isEmpty() && avoided.isEmpty())
        return quality; // aucune règle ne concerne ce groupe

    QList<const CopyFacts*> candidates;
    SuggestionOutcome out;
    if (!preferred.isEmpty()) {
        candidates = preferred;
        out.reason = SuggestionReason::FolderPreferred;
    } else if (!neutral.isEmpty()) {
        candidates = neutral;
        out.reason = SuggestionReason::FolderAvoided;
    } else {
        quality.flags = FlagAllAvoided; // FR-010 : la meilleure copie reste gardée
        return quality;
    }
    const CopyFacts* chosen = best(candidates);

    // Protection de la qualité (clarification FR-006) : hors « Identique », une règle n'écarte
    // jamais une copie RAW ou de plus grande résolution que celle qu'elle désigne.
    if (level != DupLevel::Identical) {
        const QSet<const CopyFacts*> in(candidates.begin(), candidates.end());
        for (const CopyFacts& c : copies) {
            if (in.contains(&c))
                continue;
            if ((c.isRaw && !chosen->isRaw) || c.pixels > chosen->pixels) {
                quality.flags = FlagPreferredWorse;
                return quality;
            }
        }
    }

    out.keeper = chosen->id;
    const FolderRule* decisive = out.reason == SuggestionReason::FolderPreferred ? ruleOf.value(chosen->id)
                                                                                  : ruleOf.value(avoided.first()->id);
    out.detail = decisive ? decisive->path : QString();
    return out;
}

FolderRules Suggestion::loadRules(Database& db)
{
    QList<FolderRule> rules;
    auto q = db.prepare("SELECT id, path, kind, created_at FROM dup_folder_rule");
    while (q.step()) {
        FolderRule r;
        r.id = q.int64(0);
        r.path = q.text(1);
        r.kind = ruleKindFromDb(q.text(2));
        r.createdAt = q.int64(3);
        rules << r;
    }
    return FolderRules(rules);
}

QHash<GroupId, QList<CopyFacts>> Suggestion::loadFacts(Database& db, const QList<GroupId>* only)
{
    QHash<GroupId, QList<CopyFacts>> out;
    auto read = [&](const QString& where) {
        const QString sql = QStringLiteral("SELECT m.group_id, i.id, i.kind, i.width, i.height, i.size_bytes, i.taken_at,"
                                           " i.sort_date, fo.path, i.file_name FROM dup_member m"
                                           " JOIN item i ON i.id = m.item_id JOIN folder fo ON fo.id = i.folder_id") +
                            where;
        auto q = db.prepare(sql.toUtf8().constData());
        while (q.step()) {
            CopyFacts f;
            f.id = q.int64(1);
            f.isRaw = q.text(2) == u"raw";
            f.pixels = qint64(q.isNull(3) ? 0 : q.int32(3)) * (q.isNull(4) ? 0 : q.int32(4));
            f.sizeBytes = q.int64(5);
            f.hasTakenAt = !q.isNull(6);
            f.date = q.int64(7);
            f.path = platform::joinPath(q.text(8), q.text(9));
            out[q.int64(0)] << f;
        }
    };
    if (!only) {
        read({});
        return out;
    }
    for (qsizetype off = 0; off < only->size(); off += 500)
        read(QStringLiteral(" WHERE m.group_id IN (") + idList(only->mid(off, 500)) + u')');
    return out;
}

void Suggestion::apply(Database& db, const QList<GroupId>* only)
{
    const FolderRules rules = loadRules(db);
    const QHash<GroupId, QList<CopyFacts>> facts = loadFacts(db, only);

    auto head = db.prepare("SELECT center_item_id, level FROM dup_group WHERE id = ?");
    auto member = db.prepare("UPDATE dup_member SET suggested_keep = (item_id = ?) WHERE group_id = ?");
    auto group = db.prepare("UPDATE dup_group SET suggestion_reason = ?, suggestion_detail = ?, flags = ? WHERE id = ?");
    for (auto it = facts.constBegin(); it != facts.constEnd(); ++it) {
        head.bind(1, it.key());
        if (!head.step()) {
            head.reset();
            continue;
        }
        const ItemId center = head.int64(0);
        const DupLevel level = dupLevelFromDb(head.text(1));
        head.reset();
        const SuggestionOutcome s = choose(level, center, it.value(), rules);
        member.bind(1, s.keeper).bind(2, it.key());
        member.run();
        member.reset();
        const QString reason = toDb(s.reason);
        if (reason.isEmpty())
            group.bindNull(1);
        else
            group.bind(1, reason);
        if (s.detail.isEmpty())
            group.bindNull(2);
        else
            group.bind(2, s.detail);
        group.bind(3, s.flags).bind(4, it.key());
        group.run();
        group.reset();
    }

    // FR-017 : aucun groupe ne doit se retrouver sans copie gardée.
    db.exec("DELETE FROM dup_choice WHERE item_id IN ("
            " SELECT m.item_id FROM dup_member m WHERE m.group_id IN ("
            "  SELECT g.id FROM dup_group g WHERE NOT EXISTS ("
            "   SELECT 1 FROM dup_member m2 LEFT JOIN dup_choice c ON c.item_id = m2.item_id"
            "   WHERE m2.group_id = g.id AND COALESCE(c.keep, m2.suggested_keep) = 1)))");

    if (!only) {
        db.exec(kReclaimableAllSql);
        return;
    }
    for (qsizetype off = 0; off < only->size(); off += 500)
        db.exec(QString::fromLatin1(kReclaimableAllSql) + QStringLiteral(" WHERE id IN (") + idList(only->mid(off, 500)) + u')');
}

} // namespace picazoo

#include "duplicates/DuplicateStore.h"

#include "duplicates/FolderRules.h"
#include "duplicates/Suggestion.h"
#include "index/Database.h"
#include "platform/win/Paths.h"

#include <QDateTime>
#include <QHash>
#include <QLocale>
#include <QSet>
#include <QVariant>

#include <algorithm>

namespace picazoo {

const char* const kReclaimableSql =
    "UPDATE dup_group SET reclaimable_bytes = ("
    " SELECT COALESCE(SUM(i.size_bytes), 0) FROM dup_member m JOIN item i ON i.id = m.item_id"
    " LEFT JOIN dup_choice c ON c.item_id = m.item_id"
    " WHERE m.group_id = dup_group.id AND COALESCE(c.keep, m.suggested_keep) = 0)"
    " WHERE id = ?";

namespace {

QString levelClause(LevelFilter f, const char* column = "level")
{
    const QString c = QString::fromLatin1(column);
    switch (f) {
    case LevelFilter::Identical: return QStringLiteral(" AND %1 = 'identical'").arg(c);
    case LevelFilter::SameImage: return QStringLiteral(" AND %1 = 'same_image'").arg(c);
    case LevelFilter::Similar: return QStringLiteral(" AND %1 = 'similar'").arg(c);
    case LevelFilter::All: break;
    }
    return {};
}

qint64 nowMs() { return QDateTime::currentMSecsSinceEpoch(); }

QString idList(const QList<qint64>& ids)
{
    QString s;
    for (qint64 id : ids) {
        if (!s.isEmpty())
            s += u',';
        s += QString::number(id);
    }
    return s;
}

QString plural(int n, const char* one, const char* many)
{
    return QLocale(QLocale::French).toString(n) + u' ' + QString::fromUtf8(n > 1 ? many : one);
}

// Requête paramétrée : texte avec des « ? » et valeurs dans l'ordre.
struct Sql {
    QString text;
    QList<QVariant> params;
    void add(const QString& t, const QList<QVariant>& p = {})
    {
        text += t;
        params += p;
    }
    Statement prepare(Database& db) const
    {
        Statement s = db.prepare(text.toUtf8().constData());
        for (int i = 0; i < params.size(); ++i) {
            const QVariant& v = params[i];
            if (v.typeId() == QMetaType::QString)
                s.bind(i + 1, v.toString());
            else
                s.bind(i + 1, v.toLongLong());
        }
        return s;
    }
};

// Condition « le dossier `alias`.path est `path` ou en dessous » (research R5) : la plage
// ]path\, path]] profite de l'index unique de folder.path (insensible à la casse).
void underFolder(Sql& sql, const QString& alias, const QString& path)
{
    const QString p = FolderRules::normalized(path);
    const QString base = p.endsWith(u'\\') ? p.chopped(1) : p;
    sql.add(QStringLiteral("(%1.path = ? OR (%1.path > ? AND %1.path < ?))").arg(alias),
            {p, base + u'\\', base + u']'});
}

void memberExists(Sql& sql, const std::function<void(Sql&)>& cond)
{
    sql.add(QStringLiteral(" AND EXISTS (SELECT 1 FROM dup_member m JOIN item i ON i.id = m.item_id"
                           " JOIN folder fo ON fo.id = i.folder_id WHERE m.group_id = g.id AND "));
    cond(sql);
    sql.add(QStringLiteral(")"));
}

Sql whereClause(const GroupQuery& q)
{
    Sql sql;
    sql.add(QStringLiteral(" FROM dup_group g WHERE g.state = 'to_review'") + levelClause(q.level, "g.level"));
    if (!q.folder.isEmpty())
        memberExists(sql, [&](Sql& s) { underFolder(s, QStringLiteral("fo"), q.folder); });
    if (!q.pairA.isEmpty() && !q.pairB.isEmpty()) {
        memberExists(sql, [&](Sql& s) { underFolder(s, QStringLiteral("fo"), q.pairA); });
        memberExists(sql, [&](Sql& s) { underFolder(s, QStringLiteral("fo"), q.pairB); });
    }
    switch (q.type) {
    case GroupQuery::Type::Photos:
        sql.add(QStringLiteral(" AND NOT EXISTS (SELECT 1 FROM dup_member m JOIN item i ON i.id = m.item_id"
                               " WHERE m.group_id = g.id AND i.kind = 'video')"));
        break;
    case GroupQuery::Type::Videos:
        memberExists(sql, [](Sql& s) { s.add(QStringLiteral("i.kind = 'video'")); });
        break;
    case GroupQuery::Type::RawJpeg:
        memberExists(sql, [](Sql& s) { s.add(QStringLiteral("i.kind = 'raw'")); });
        memberExists(sql, [](Sql& s) { s.add(QStringLiteral("i.kind = 'image'")); });
        break;
    case GroupQuery::Type::Any: break;
    }
    switch (q.state) {
    case GroupQuery::State::Untouched:
        sql.add(QStringLiteral(" AND NOT EXISTS (SELECT 1 FROM dup_member m JOIN dup_choice c ON c.item_id = m.item_id"
                               " WHERE m.group_id = g.id)"));
        break;
    case GroupQuery::State::Modified:
        sql.add(QStringLiteral(" AND EXISTS (SELECT 1 FROM dup_member m JOIN dup_choice c ON c.item_id = m.item_id"
                               " WHERE m.group_id = g.id)"));
        break;
    case GroupQuery::State::Flagged: sql.add(QStringLiteral(" AND g.flags <> 0")); break;
    case GroupQuery::State::Any: break;
    }
    if (q.minBytes > 0)
        memberExists(sql, [&](Sql& s) { s.add(QStringLiteral("i.size_bytes >= ?"), {q.minBytes}); });
    if (q.dateFrom > 0 || q.dateTo > 0) {
        sql.add(QStringLiteral(" AND EXISTS (SELECT 1 FROM dup_member m JOIN item i ON i.id = m.item_id"
                               " LEFT JOIN dup_choice c ON c.item_id = m.item_id"
                               " WHERE m.group_id = g.id AND COALESCE(c.keep, m.suggested_keep) = 1"));
        if (q.dateFrom > 0)
            sql.add(QStringLiteral(" AND i.sort_date >= ?"), {q.dateFrom});
        if (q.dateTo > 0)
            sql.add(QStringLiteral(" AND i.sort_date <= ?"), {q.dateTo});
        sql.add(QStringLiteral(")"));
    }
    return sql;
}

QString orderClause(GroupQuery::Sort s)
{
    switch (s) {
    case GroupQuery::Sort::Copies: return QStringLiteral(" ORDER BY g.member_count DESC, g.reclaimable_bytes DESC, g.id");
    case GroupQuery::Sort::Date:
        return QStringLiteral(" ORDER BY (SELECT i.sort_date FROM dup_member m JOIN item i ON i.id = m.item_id"
                              " WHERE m.group_id = g.id AND m.suggested_keep = 1) DESC, g.id");
    case GroupQuery::Sort::Folder:
        return QStringLiteral(" ORDER BY (SELECT fo.path FROM dup_member m JOIN item i ON i.id = m.item_id"
                              " JOIN folder fo ON fo.id = i.folder_id WHERE m.group_id = g.id AND m.suggested_keep = 1)"
                              " COLLATE NOCASE, g.reclaimable_bytes DESC, g.id");
    case GroupQuery::Sort::Reclaimable: break;
    }
    return QStringLiteral(" ORDER BY g.reclaimable_bytes DESC, g.id");
}

QStringList segments(const QString& path) { return FolderRules::normalized(path).split(u'\\', Qt::SkipEmptyParts); }

// Racines divergentes de deux dossiers qui se terminent par la même suite de segments
// (« Images\2019\Été » / « D:\Sauvegarde\2019\Été » → « Images » / « D:\Sauvegarde »).
std::optional<QPair<QString, QString>> divergentRoots(const QString& a, const QString& b)
{
    const QStringList sa = segments(a), sb = segments(b);
    int k = 0;
    while (k < sa.size() - 1 && k < sb.size() - 1 &&
           sa[sa.size() - 1 - k].compare(sb[sb.size() - 1 - k], Qt::CaseInsensitive) == 0)
        ++k;
    if (k == 0)
        return std::nullopt;
    auto join = [](const QStringList& s, int n) {
        QString out = s.mid(0, n).join(u'\\');
        if (n == 1 && out.endsWith(u':'))
            out += u'\\';
        return out;
    };
    return QPair<QString, QString>(join(sa, int(sa.size()) - k), join(sb, int(sb.size()) - k));
}

QString pairKey(const QString& a, const QString& b) { return a.toLower() + QChar(0) + b.toLower(); }

} // namespace

DuplicateStore::DuplicateStore(const QString& libraryDbPath)
{
    read_ = Database::open(libraryDbPath, true);
    write_ = Database::open(libraryDbPath);
}

DuplicateStore::~DuplicateStore() = default;

void DuplicateStore::changed()
{
    if (onChanged_)
        onChanged_();
}

// ------------------------------------------------------------------ Lectures

QList<GroupId> DuplicateStore::groups(LevelFilter filter) const
{
    GroupQuery q;
    q.level = filter;
    return groups(q);
}

QList<GroupId> DuplicateStore::groups(const GroupQuery& query) const
{
    std::lock_guard lock(read_->mutex());
    Sql sql;
    sql.add(QStringLiteral("SELECT g.id"));
    const Sql where = whereClause(query);
    sql.add(where.text, where.params);
    sql.add(orderClause(query.sort));
    QList<GroupId> out;
    auto q = sql.prepare(*read_);
    while (q.step())
        out << q.int64(0);
    return out;
}

int DuplicateStore::count(const GroupQuery& query) const
{
    std::lock_guard lock(read_->mutex());
    Sql sql;
    sql.add(QStringLiteral("SELECT COUNT(*)"));
    const Sql where = whereClause(query);
    sql.add(where.text, where.params);
    auto q = sql.prepare(*read_);
    return q.step() ? q.int32(0) : 0;
}

std::optional<DuplicateGroup> DuplicateStore::group(GroupId id) const
{
    std::lock_guard lock(read_->mutex());
    auto g = read_->prepare("SELECT id, center_item_id, keeper_reason, level, reclaimable_bytes, state,"
                            " suggestion_reason, suggestion_detail, flags FROM dup_group WHERE id = ?");
    g.bind(1, id);
    if (!g.step())
        return std::nullopt;
    DuplicateGroup out;
    out.id = g.int64(0);
    out.center = g.int64(1);
    out.reason = keeperReasonFromDb(g.text(2));
    out.level = dupLevelFromDb(g.text(3));
    out.reclaimableBytes = g.int64(4);
    out.state = groupStateFromDb(g.text(5));
    out.suggestionReason = g.isNull(6) ? SuggestionReason::Quality : suggestionReasonFromDb(g.text(6));
    out.suggestionDetail = g.isNull(7) ? QString() : g.text(7);
    out.flags = g.int32(8);
    auto m = read_->prepare("SELECT m.item_id, m.level, m.similarity, m.rotation, c.keep, m.suggested_keep FROM dup_member m"
                            " LEFT JOIN dup_choice c ON c.item_id = m.item_id WHERE m.group_id = ?"
                            " ORDER BY m.item_id = ? DESC, m.level, m.similarity DESC, m.item_id");
    m.bind(1, id).bind(2, out.center);
    while (m.step()) {
        DuplicateCopy c;
        c.item = m.int64(0);
        c.isCenter = c.item == out.center;
        c.level = dupLevelFromDb(m.text(1));
        if (!m.isNull(2))
            c.similarity = double(m.int64(2)) / 1e6;
        c.rotation = m.int32(3);
        c.userChoice = !m.isNull(4);
        c.isSuggested = m.int32(5) != 0;
        c.keep = c.userChoice ? m.int32(4) != 0 : c.isSuggested;
        if (c.isSuggested)
            out.suggested = c.item;
        out.copies << c;
    }
    if (!out.suggested)
        out.suggested = out.center;
    return out;
}

std::optional<GroupId> DuplicateStore::groupOf(ItemId item) const
{
    std::lock_guard lock(read_->mutex());
    auto q = read_->prepare("SELECT m.group_id FROM dup_member m JOIN dup_group g ON g.id = m.group_id"
                            " WHERE m.item_id = ? AND g.state = 'to_review'");
    q.bind(1, item);
    if (q.step())
        return q.int64(0);
    return std::nullopt;
}

int DuplicateStore::duplicateCount(ItemId item) const
{
    std::lock_guard lock(read_->mutex());
    auto q = read_->prepare("SELECT g.member_count FROM dup_member m JOIN dup_group g ON g.id = m.group_id"
                            " WHERE m.item_id = ? AND g.state = 'to_review'");
    q.bind(1, item);
    return q.step() ? std::max(0, q.int32(0) - 1) : 0;
}

DuplicateSummary DuplicateStore::summary(LevelFilter filter) const
{
    std::lock_guard lock(read_->mutex());
    const QString sql = QStringLiteral("SELECT COUNT(*), COALESCE(SUM(reclaimable_bytes), 0) FROM dup_group"
                                       " WHERE state = 'to_review'") + levelClause(filter);
    auto q = read_->prepare(sql.toUtf8().constData());
    DuplicateSummary s;
    if (q.step()) {
        s.groups = q.int32(0);
        s.reclaimableBytes = q.int64(1);
    }
    return s;
}

SelectionTotals DuplicateStore::totals(const QList<GroupId>& groups) const
{
    std::lock_guard lock(read_->mutex());
    SelectionTotals t;
    for (qsizetype off = 0; off < groups.size(); off += 500) {
        const QString ids = idList(groups.mid(off, 500));
        const QString sql =
            QStringLiteral("SELECT (SELECT COUNT(*) FROM dup_group WHERE id IN (%1) AND state = 'to_review'),"
                           " COUNT(*), COALESCE(SUM(i.size_bytes), 0) FROM dup_member m JOIN item i ON i.id = m.item_id"
                           " JOIN dup_group g ON g.id = m.group_id LEFT JOIN dup_choice c ON c.item_id = m.item_id"
                           " WHERE m.group_id IN (%1) AND g.state = 'to_review' AND COALESCE(c.keep, m.suggested_keep) = 0")
                .arg(ids);
        auto q = read_->prepare(sql.toUtf8().constData());
        if (q.step()) {
            t.groups += q.int32(0);
            t.files += q.int32(1);
            t.bytes += q.int64(2);
        }
    }
    return t;
}

SelectionTotals DuplicateStore::trashTotals() const
{
    std::lock_guard lock(read_->mutex());
    SelectionTotals t;
    auto q = read_->prepare("SELECT (SELECT COUNT(*) FROM dup_group WHERE state = 'to_review'),"
                            " COUNT(*), COALESCE(SUM(i.size_bytes), 0) FROM dup_member m JOIN item i ON i.id = m.item_id"
                            " JOIN dup_group g ON g.id = m.group_id LEFT JOIN dup_choice c ON c.item_id = m.item_id"
                            " WHERE g.state = 'to_review' AND COALESCE(c.keep, m.suggested_keep) = 0");
    if (q.step()) {
        t.groups = q.int32(0);
        t.files = q.int32(1);
        t.bytes = q.int64(2);
    }
    return t;
}

QList<QPair<QString, int>> DuplicateStore::foldersOf(const QList<GroupId>& groups, int limit) const
{
    std::lock_guard lock(read_->mutex());
    QHash<QString, QPair<QString, QSet<GroupId>>> byKey;
    for (qsizetype off = 0; off < groups.size(); off += 500) {
        const QString sql = QStringLiteral("SELECT m.group_id, fo.path FROM dup_member m JOIN item i ON i.id = m.item_id"
                                           " JOIN folder fo ON fo.id = i.folder_id WHERE m.group_id IN (%1)")
                                .arg(idList(groups.mid(off, 500)));
        auto q = read_->prepare(sql.toUtf8().constData());
        while (q.step()) {
            const QString path = q.text(1);
            auto& e = byKey[path.toLower()];
            e.first = path;
            e.second.insert(q.int64(0));
        }
    }
    QList<QPair<QString, int>> out;
    for (const auto& e : std::as_const(byKey))
        out << qMakePair(e.first, int(e.second.size()));
    std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) {
        return a.second != b.second ? a.second > b.second : a.first.compare(b.first, Qt::CaseInsensitive) < 0;
    });
    if (out.size() > limit)
        out.resize(limit);
    return out;
}

// ------------------------------------------------------- Décisions unitaires

void DuplicateStore::refreshReclaimable(GroupId group)
{
    auto q = write_->prepare(kReclaimableSql);
    q.bind(1, group);
    q.run();
}

bool DuplicateStore::setKeep(GroupId groupId, ItemId item, bool keep)
{
    const auto g = group(groupId);
    if (!g)
        return false;
    const bool member = std::any_of(g->copies.begin(), g->copies.end(), [&](const DuplicateCopy& c) { return c.item == item; });
    if (!member)
        return false;
    if (!keep) {
        const bool otherKept = std::any_of(g->copies.begin(), g->copies.end(),
                                           [&](const DuplicateCopy& c) { return c.item != item && c.keep; });
        if (!otherKept)
            return false; // FR-017 : au moins une copie reste
    }
    {
        std::lock_guard lock(write_->mutex());
        Transaction tx(*write_);
        // Le choix explicite d'une copie fige aussi l'état des autres copies du groupe.
        auto put = write_->prepare("INSERT OR REPLACE INTO dup_choice (item_id, keep, decided_at) VALUES (?, ?, ?)");
        for (const DuplicateCopy& c : g->copies) {
            const bool k = c.item == item ? keep : c.keep;
            put.bind(1, c.item).bind(2, k ? 1 : 0).bind(3, nowMs());
            put.run();
            put.reset();
        }
        refreshReclaimable(groupId);
        tx.commit();
    }
    changed();
    return true;
}

void DuplicateStore::resetChoice(GroupId groupId)
{
    const auto g = group(groupId);
    if (!g)
        return;
    {
        std::lock_guard lock(write_->mutex());
        Transaction tx(*write_);
        auto del = write_->prepare("DELETE FROM dup_choice WHERE item_id = ?");
        for (const DuplicateCopy& c : g->copies) {
            del.bind(1, c.item);
            del.run();
            del.reset();
        }
        refreshReclaimable(groupId);
        tx.commit();
    }
    changed();
}

void DuplicateStore::pairDecision(const QList<ItemId>& a, const QList<ItemId>& b, const char* kind, qint64 batchId)
{
    auto put = write_->prepare("INSERT OR REPLACE INTO dup_decision (item_a, item_b, kind, decided_at, batch_id)"
                               " VALUES (?, ?, ?, ?, ?)");
    for (ItemId x : a)
        for (ItemId y : b) {
            if (x == y)
                continue;
            put.bind(1, std::min(x, y)).bind(2, std::max(x, y)).bind(3, QString::fromLatin1(kind)).bind(4, nowMs());
            if (batchId)
                put.bind(5, batchId);
            else
                put.bindNull(5);
            put.run();
            put.reset();
        }
}

void DuplicateStore::markNotDuplicates(GroupId groupId)
{
    const auto g = group(groupId);
    if (!g)
        return;
    QList<ItemId> ids;
    for (const auto& c : g->copies)
        ids << c.item;
    {
        std::lock_guard lock(write_->mutex());
        Transaction tx(*write_);
        pairDecision(ids, ids, "not_duplicate");
        tx.commit();
    }
    changed();
}

void DuplicateStore::removeFromGroup(GroupId groupId, ItemId item)
{
    const auto g = group(groupId);
    if (!g)
        return;
    QList<ItemId> others;
    for (const auto& c : g->copies)
        if (c.item != item)
            others << c.item;
    {
        std::lock_guard lock(write_->mutex());
        Transaction tx(*write_);
        pairDecision({item}, others, "not_duplicate");
        auto del = write_->prepare("DELETE FROM dup_choice WHERE item_id = ?");
        del.bind(1, item);
        del.run();
        tx.commit();
    }
    changed();
}

void DuplicateStore::keepAll(GroupId groupId)
{
    const auto g = group(groupId);
    if (!g)
        return;
    QList<ItemId> ids;
    for (const auto& c : g->copies)
        ids << c.item;
    {
        std::lock_guard lock(write_->mutex());
        Transaction tx(*write_);
        pairDecision(ids, ids, "keep_all");
        auto upd = write_->prepare("UPDATE dup_group SET state = 'kept_all' WHERE id = ?");
        upd.bind(1, groupId);
        upd.run();
        tx.commit();
    }
    changed();
}

void DuplicateStore::undoDecisions(const QList<ItemId>& items)
{
    {
        std::lock_guard lock(write_->mutex());
        Transaction tx(*write_);
        auto pairs = write_->prepare("DELETE FROM dup_decision WHERE item_a = ?1 OR item_b = ?1");
        auto choice = write_->prepare("DELETE FROM dup_choice WHERE item_id = ?");
        for (ItemId id : items) {
            pairs.bind(1, id);
            pairs.run();
            pairs.reset();
            choice.bind(1, id);
            choice.run();
            choice.reset();
        }
        tx.commit();
    }
    changed();
}

// ------------------------------------------------------------ Actions en lot

BatchResult DuplicateStore::applyBatch(BatchKind kind, const QList<GroupId>& groupIds, const QString& folder)
{
    BatchResult r;
    if (groupIds.isEmpty() || (kind == BatchKind::KeepFolder && folder.isEmpty()))
        return r;
    // Lectures d'abord (connexion de lecture), écritures ensuite dans une seule transaction.
    QList<DuplicateGroup> loaded;
    for (GroupId id : groupIds)
        if (auto g = group(id); g && g->state == GroupState::ToReview)
            loaded << *g;
    if (loaded.isEmpty())
        return r;

    std::lock_guard lock(write_->mutex());
    Transaction tx(*write_);
    auto batch = write_->prepare("INSERT INTO dup_batch (kind, label, detail, group_count, created_at) VALUES (?, '', ?, 0, ?)");
    batch.bind(1, toDb(kind));
    if (folder.isEmpty())
        batch.bindNull(2);
    else
        batch.bind(2, FolderRules::normalized(folder));
    batch.bind(3, nowMs());
    batch.run();
    const qint64 batchId = write_->lastInsertId();

    auto entry = write_->prepare("INSERT OR REPLACE INTO dup_batch_entry (batch_id, item_id, prev_keep, new_keep) VALUES (?, ?, ?, ?)");
    auto record = [&](const DuplicateCopy& c, std::optional<int> newKeep) {
        entry.bind(1, batchId).bind(2, c.item);
        if (c.userChoice)
            entry.bind(3, c.keep ? 1 : 0);
        else
            entry.bindNull(3);
        if (newKeep)
            entry.bind(4, *newKeep);
        else
            entry.bindNull(4);
        entry.run();
        entry.reset();
    };
    auto put = write_->prepare("INSERT OR REPLACE INTO dup_choice (item_id, keep, decided_at) VALUES (?, ?, ?)");
    auto del = write_->prepare("DELETE FROM dup_choice WHERE item_id = ?");

    switch (kind) {
    case BatchKind::Reset:
        for (const DuplicateGroup& g : loaded) {
            bool any = false;
            for (const DuplicateCopy& c : g.copies) {
                if (!c.userChoice)
                    continue;
                record(c, std::nullopt);
                del.bind(1, c.item);
                del.run();
                del.reset();
                any = true;
            }
            if (any) {
                refreshReclaimable(g.id);
                ++r.changed;
            }
        }
        break;
    case BatchKind::KeepFolder: {
        QList<GroupId> ids;
        for (const auto& g : loaded)
            ids << g.id;
        const QHash<GroupId, QList<CopyFacts>> facts = Suggestion::loadFacts(*write_, &ids);
        FolderRule rule;
        rule.path = FolderRules::normalized(folder);
        rule.kind = RuleKind::Prefer;
        const FolderRules rules({rule});
        for (const DuplicateGroup& g : loaded) {
            const SuggestionOutcome s = Suggestion::choose(g.level, g.center, facts.value(g.id), rules);
            if (s.reason != SuggestionReason::FolderPreferred) {
                if (s.flags & FlagPreferredWorse)
                    ++r.flagged;
                else
                    ++r.skipped;
                continue;
            }
            for (const DuplicateCopy& c : g.copies) {
                const int k = c.item == s.keeper ? 1 : 0;
                record(c, k);
                put.bind(1, c.item).bind(2, k).bind(3, nowMs());
                put.run();
                put.reset();
            }
            refreshReclaimable(g.id);
            ++r.changed;
        }
        break;
    }
    case BatchKind::KeepAll: {
        auto upd = write_->prepare("UPDATE dup_group SET state = 'kept_all' WHERE id = ?");
        for (const DuplicateGroup& g : loaded) {
            QList<ItemId> ids;
            for (const auto& c : g.copies)
                ids << c.item;
            pairDecision(ids, ids, "keep_all", batchId);
            upd.bind(1, g.id);
            upd.run();
            upd.reset();
            ++r.changed;
        }
        break;
    }
    case BatchKind::NotDuplicate: {
        // Le groupe disparaît tout de suite ; le prochain regroupement respectera les paires.
        auto drop = write_->prepare("DELETE FROM dup_group WHERE id = ?");
        for (const DuplicateGroup& g : loaded) {
            QList<ItemId> ids;
            for (const auto& c : g.copies)
                ids << c.item;
            pairDecision(ids, ids, "not_duplicate", batchId);
            drop.bind(1, g.id);
            drop.run();
            drop.reset();
            ++r.changed;
        }
        break;
    }
    }

    if (r.changed == 0) {
        // Rien à faire : la transaction est abandonnée, aucun lot n'est inscrit.
        r.message = kind == BatchKind::KeepFolder
                        ? (r.flagged ? plural(r.flagged, "groupe signalé", "groupes signalés") +
                                           QStringLiteral(" : la copie de ce dossier est moins bonne. Rien n'a changé.")
                                     : QStringLiteral("Aucun groupe sélectionné n'a de copie dans ce dossier."))
                        : QStringLiteral("Rien à changer dans les groupes sélectionnés.");
        return r;
    }

    const QString groupsText = plural(r.changed, "groupe", "groupes");
    QString label;
    switch (kind) {
    case BatchKind::Reset:
        label = QStringLiteral("Revenir à la suggestion — ") + groupsText;
        r.message = QStringLiteral("Suggestion rétablie sur ") + groupsText + u'.';
        break;
    case BatchKind::KeepFolder:
        label = QStringLiteral("Garder les copies de ") + FolderRules::normalized(folder) + QStringLiteral(" — ") + groupsText;
        r.message = groupsText + QStringLiteral(" : copies de ") + FolderRules::normalized(folder) + QStringLiteral(" gardées.");
        if (r.flagged)
            r.message += u' ' + plural(r.flagged, "groupe signalé", "groupes signalés") +
                         QStringLiteral(" : la copie de ce dossier est moins bonne.");
        break;
    case BatchKind::KeepAll:
        label = QStringLiteral("Garder toutes — ") + groupsText;
        r.message = plural(r.changed, "groupe marqué", "groupes marqués") + QStringLiteral(" « Garder toutes ».");
        break;
    case BatchKind::NotDuplicate:
        label = QStringLiteral("Ce ne sont pas des doublons — ") + groupsText;
        r.message = plural(r.changed, "groupe marqué", "groupes marqués") + QStringLiteral(" « Ce ne sont pas des doublons ».");
        break;
    }
    auto upd = write_->prepare("UPDATE dup_batch SET label = ?, group_count = ? WHERE id = ?");
    upd.bind(1, label).bind(2, r.changed).bind(3, batchId);
    upd.run();
    tx.commit();
    r.batchId = batchId;
    if (kind == BatchKind::KeepAll || kind == BatchKind::NotDuplicate)
        changed(); // une seule notification pour tout le lot
    return r;
}

UndoResult DuplicateStore::undoBatch(qint64 batchId)
{
    UndoResult r;
    bool decisions = false;
    {
        std::lock_guard lock(write_->mutex());
        Transaction tx(*write_);
        auto b = write_->prepare("SELECT undone_at FROM dup_batch WHERE id = ?");
        b.bind(1, batchId);
        if (!b.step() || !b.isNull(0))
            return r; // inconnu ou déjà annulé
        b.reset();

        // Choix : rétablis seulement s'ils valent encore ce que le lot a écrit.
        QSet<GroupId> changedSince;
        {
            auto e = write_->prepare("SELECT item_id, prev_keep, new_keep FROM dup_batch_entry WHERE batch_id = ?");
            e.bind(1, batchId);
            auto cur = write_->prepare("SELECT keep FROM dup_choice WHERE item_id = ?");
            auto put = write_->prepare("INSERT OR REPLACE INTO dup_choice (item_id, keep, decided_at) VALUES (?, ?, ?)");
            auto del = write_->prepare("DELETE FROM dup_choice WHERE item_id = ?");
            auto groupOfItem = write_->prepare("SELECT group_id FROM dup_member WHERE item_id = ?");
            while (e.step()) {
                const ItemId item = e.int64(0);
                const std::optional<qint64> prev = e.optInt64(1);
                const std::optional<qint64> written = e.optInt64(2);
                cur.bind(1, item);
                const std::optional<qint64> now = cur.step() ? std::optional<qint64>(cur.int64(0)) : std::nullopt;
                cur.reset();
                if (now != written) {
                    groupOfItem.bind(1, item);
                    if (groupOfItem.step())
                        changedSince.insert(groupOfItem.int64(0));
                    groupOfItem.reset();
                    continue;
                }
                if (prev) {
                    put.bind(1, item).bind(2, *prev).bind(3, nowMs());
                    put.run();
                    put.reset();
                } else {
                    del.bind(1, item);
                    del.run();
                    del.reset();
                }
                ++r.restored;
            }
        }
        r.notRestored = int(changedSince.size());

        // Paires écrites par le lot (et pas réécrites depuis).
        {
            auto count = write_->prepare("SELECT COUNT(*) FROM dup_decision WHERE batch_id = ?");
            count.bind(1, batchId);
            decisions = count.step() && count.int64(0) > 0;
        }
        if (decisions) {
            auto d = write_->prepare("DELETE FROM dup_decision WHERE batch_id = ?");
            d.bind(1, batchId);
            d.run();
            // « Garder toutes » : le groupe revient dès qu'il n'a plus de paire keep_all.
            write_->exec("UPDATE dup_group SET state = 'to_review' WHERE state = 'kept_all' AND NOT EXISTS ("
                         " SELECT 1 FROM dup_decision d JOIN dup_member a ON a.item_id = d.item_a"
                         " JOIN dup_member b ON b.item_id = d.item_b"
                         " WHERE d.kind = 'keep_all' AND a.group_id = dup_group.id AND b.group_id = dup_group.id)");
        }
        write_->exec(kReclaimableAllSql);
        auto done = write_->prepare("UPDATE dup_batch SET undone_at = ? WHERE id = ?");
        done.bind(1, nowMs()).bind(2, batchId);
        done.run();
        tx.commit();
    }
    if (decisions)
        changed(); // « Ce ne sont pas des doublons » : les groupes reviennent au regroupement
    return r;
}

QList<BatchInfo> DuplicateStore::batches(int limit) const
{
    std::lock_guard lock(read_->mutex());
    QList<BatchInfo> out;
    auto q = read_->prepare("SELECT id, kind, label, detail, group_count, created_at, undone_at FROM dup_batch"
                            " ORDER BY id DESC LIMIT ?");
    q.bind(1, limit);
    while (q.step()) {
        BatchInfo b;
        b.id = q.int64(0);
        b.kind = batchKindFromDb(q.text(1));
        b.label = q.text(2);
        b.detail = q.isNull(3) ? QString() : q.text(3);
        b.groups = q.int32(4);
        b.createdAt = q.int64(5);
        b.undone = !q.isNull(6);
        out << b;
    }
    return out;
}

// ----------------------------------------------------------- Règles de dossier

QList<FolderRule> DuplicateStore::folderRules() const
{
    std::lock_guard lock(read_->mutex());
    QList<FolderRule> out;
    auto q = read_->prepare("SELECT id, path, kind, created_at FROM dup_folder_rule ORDER BY path COLLATE NOCASE");
    while (q.step()) {
        FolderRule r;
        r.id = q.int64(0);
        r.path = q.text(1);
        r.kind = ruleKindFromDb(q.text(2));
        r.createdAt = q.int64(3);
        out << r;
    }
    for (FolderRule& r : out) {
        Sql found;
        found.add(QStringLiteral("SELECT EXISTS (SELECT 1 FROM folder fo WHERE "));
        underFolder(found, QStringLiteral("fo"), r.path);
        found.add(QStringLiteral(")"));
        auto f = found.prepare(*read_);
        r.found = f.step() && f.int64(0) != 0;
        auto n = read_->prepare("SELECT COUNT(*) FROM dup_group WHERE state = 'to_review'"
                                " AND suggestion_detail = ? COLLATE NOCASE");
        n.bind(1, r.path);
        r.groups = n.step() ? n.int32(0) : 0;
    }
    return out;
}

qint64 DuplicateStore::setFolderRule(const QString& path, RuleKind kind)
{
    const QString p = FolderRules::normalized(path);
    if (p.isEmpty())
        return 0;
    std::lock_guard lock(write_->mutex());
    Transaction tx(*write_);
    auto put = write_->prepare("INSERT INTO dup_folder_rule (path, kind, created_at) VALUES (?, ?, ?)"
                               " ON CONFLICT(path) DO UPDATE SET kind = excluded.kind");
    put.bind(1, p).bind(2, toDb(kind)).bind(3, nowMs());
    put.run();
    auto id = write_->prepare("SELECT id FROM dup_folder_rule WHERE path = ?");
    id.bind(1, p);
    const qint64 ruleId = id.step() ? id.int64(0) : 0;
    id.reset();
    Suggestion::apply(*write_);
    tx.commit();
    return ruleId;
}

void DuplicateStore::removeFolderRule(qint64 ruleId)
{
    std::lock_guard lock(write_->mutex());
    Transaction tx(*write_);
    auto del = write_->prepare("DELETE FROM dup_folder_rule WHERE id = ?");
    del.bind(1, ruleId);
    del.run();
    Suggestion::apply(*write_);
    tx.commit();
}

void DuplicateStore::relinkFolderRule(qint64 ruleId, const QString& path)
{
    const QString p = FolderRules::normalized(path);
    if (p.isEmpty())
        return;
    std::lock_guard lock(write_->mutex());
    Transaction tx(*write_);
    // Une autre règle sur ce dossier serait remplacée par celle-ci.
    auto clash = write_->prepare("DELETE FROM dup_folder_rule WHERE path = ? AND id <> ?");
    clash.bind(1, p).bind(2, ruleId);
    clash.run();
    auto upd = write_->prepare("UPDATE dup_folder_rule SET path = ? WHERE id = ?");
    upd.bind(1, p).bind(2, ruleId);
    upd.run();
    Suggestion::apply(*write_);
    tx.commit();
}

// ------------------------------------------------------- Paires de dossiers

QList<FolderPair> DuplicateStore::folderPairs(int limit) const
{
    struct Copy {
        QString folder;
        qint64 bytes;
    };
    QHash<GroupId, QList<Copy>> byGroup;
    {
        std::lock_guard lock(read_->mutex());
        auto q = read_->prepare("SELECT m.group_id, fo.path, i.size_bytes FROM dup_member m"
                                " JOIN dup_group g ON g.id = m.group_id JOIN item i ON i.id = m.item_id"
                                " JOIN folder fo ON fo.id = i.folder_id WHERE g.state = 'to_review'");
        while (q.step())
            byGroup[q.int64(0)] << Copy{q.text(1), q.int64(2)};
    }
    struct Acc {
        QString a, b;
        int groups = 0;
        qint64 bytes = 0;
        QSet<QString> leaves; // pour une racine : sous-paires réunies
    };
    QHash<QString, Acc> leaves, roots;
    for (auto it = byGroup.constBegin(); it != byGroup.constEnd(); ++it) {
        // Volume de chaque dossier dans ce groupe.
        QHash<QString, QPair<QString, qint64>> perFolder;
        for (const Copy& c : it.value()) {
            auto& f = perFolder[c.folder.toLower()];
            f.first = c.folder;
            f.second += c.bytes;
        }
        QList<QPair<QString, qint64>> folders = perFolder.values();
        std::sort(folders.begin(), folders.end(),
                  [](const auto& x, const auto& y) { return x.first.compare(y.first, Qt::CaseInsensitive) < 0; });
        QHash<QString, qint64> rootBytes; // racine -> octets (le maximum des sous-paires du groupe)
        QHash<QString, QPair<QString, QString>> rootNames;
        QMultiHash<QString, QString> rootLeaf;
        for (int x = 0; x < folders.size(); ++x)
            for (int y = x + 1; y < folders.size(); ++y) {
                const QString& a = folders[x].first;
                const QString& b = folders[y].first;
                const qint64 bytes = std::min(folders[x].second, folders[y].second);
                const QString key = pairKey(a, b);
                Acc& acc = leaves[key];
                acc.a = a;
                acc.b = b;
                acc.groups += 1;
                acc.bytes += bytes;
                if (const auto r = divergentRoots(a, b)) {
                    const QString rk = pairKey(r->first, r->second);
                    rootBytes[rk] = std::max(rootBytes.value(rk), bytes);
                    rootNames[rk] = *r;
                    rootLeaf.insert(rk, key);
                }
            }
        for (auto r = rootBytes.constBegin(); r != rootBytes.constEnd(); ++r) {
            Acc& acc = roots[r.key()];
            acc.a = rootNames.value(r.key()).first;
            acc.b = rootNames.value(r.key()).second;
            acc.groups += 1;
            acc.bytes += r.value();
            for (const QString& leaf : rootLeaf.values(r.key()))
                acc.leaves.insert(leaf);
        }
    }
    QList<FolderPair> out;
    QSet<QString> covered;
    for (const Acc& acc : std::as_const(roots)) {
        if (acc.leaves.size() < 2)
            continue; // une seule sous-paire : on la montre telle quelle
        out << FolderPair{acc.a, acc.b, acc.groups, acc.bytes, int(acc.leaves.size())};
        covered.unite(acc.leaves);
    }
    for (auto it = leaves.constBegin(); it != leaves.constEnd(); ++it)
        if (!covered.contains(it.key()))
            out << FolderPair{it->a, it->b, it->groups, it->bytes, 1};
    std::sort(out.begin(), out.end(), [](const FolderPair& x, const FolderPair& y) {
        return x.bytes != y.bytes ? x.bytes > y.bytes : x.groups > y.groups;
    });
    if (out.size() > limit)
        out.resize(limit);
    return out;
}

QList<FolderStat> DuplicateStore::internalDuplicates(int limit) const
{
    QHash<QString, FolderStat> stats;
    {
        std::lock_guard lock(read_->mutex());
        auto q = read_->prepare("SELECT m.group_id, fo.path, COUNT(*), SUM(i.size_bytes), MAX(i.size_bytes) FROM dup_member m"
                                " JOIN dup_group g ON g.id = m.group_id JOIN item i ON i.id = m.item_id"
                                " JOIN folder fo ON fo.id = i.folder_id WHERE g.state = 'to_review'"
                                " GROUP BY m.group_id, fo.id HAVING COUNT(*) > 1");
        while (q.step()) {
            FolderStat& s = stats[q.text(1).toLower()];
            s.folder = q.text(1);
            s.groups += 1;
            s.bytes += q.int64(3) - q.int64(4);
        }
    }
    QList<FolderStat> out = stats.values();
    std::sort(out.begin(), out.end(), [](const FolderStat& a, const FolderStat& b) { return a.bytes > b.bytes; });
    if (out.size() > limit)
        out.resize(limit);
    return out;
}

// ----------------------------------------------------- Écartés de la revue

QList<SetAsideEntry> DuplicateStore::setAside() const
{
    std::lock_guard lock(read_->mutex());
    QList<SetAsideEntry> out;
    {
        auto g = read_->prepare("SELECT id FROM dup_group WHERE state = 'kept_all'");
        auto m = read_->prepare("SELECT item_id FROM dup_member WHERE group_id = ?");
        QList<GroupId> ids;
        while (g.step())
            ids << g.int64(0);
        for (GroupId id : ids) {
            SetAsideEntry e;
            e.kind = SetAsideEntry::Kind::KeptAll;
            e.group = id;
            m.bind(1, id);
            while (m.step())
                e.items << m.int64(0);
            m.reset();
            const QString sql = QStringLiteral("SELECT MAX(decided_at) FROM dup_decision WHERE kind = 'keep_all'"
                                               " AND item_a IN (%1) AND item_b IN (%1)").arg(idList(e.items));
            auto d = read_->prepare(sql.toUtf8().constData());
            e.decidedAt = d.step() && !d.isNull(0) ? d.int64(0) : 0;
            out << e;
        }
    }
    {
        // « Ce ne sont pas des doublons » : composantes reliées par les paires.
        QHash<ItemId, ItemId> parent;
        std::function<ItemId(ItemId)> find = [&](ItemId x) {
            ItemId p = parent.value(x, x);
            if (p == x)
                return x;
            const ItemId root = find(p);
            parent[x] = root;
            return root;
        };
        QHash<ItemId, qint64> when;
        QList<QPair<ItemId, ItemId>> pairs;
        auto q = read_->prepare("SELECT item_a, item_b, decided_at FROM dup_decision WHERE kind = 'not_duplicate'");
        while (q.step()) {
            const ItemId a = q.int64(0), b = q.int64(1);
            pairs << qMakePair(a, b);
            const ItemId ra = find(a), rb = find(b);
            if (ra != rb)
                parent[ra] = rb;
            when[a] = std::max(when.value(a), q.int64(2));
            when[b] = std::max(when.value(b), q.int64(2));
        }
        QHash<ItemId, SetAsideEntry> comps;
        for (auto it = when.constBegin(); it != when.constEnd(); ++it) {
            SetAsideEntry& e = comps[find(it.key())];
            e.kind = SetAsideEntry::Kind::NotDuplicate;
            e.items << it.key();
            e.decidedAt = std::max(e.decidedAt, it.value());
        }
        for (SetAsideEntry& e : comps) {
            std::sort(e.items.begin(), e.items.end());
            out << e;
        }
    }
    std::sort(out.begin(), out.end(), [](const SetAsideEntry& a, const SetAsideEntry& b) {
        return a.decidedAt != b.decidedAt ? a.decidedAt > b.decidedAt : a.items.value(0) < b.items.value(0);
    });
    return out;
}

void DuplicateStore::restore(const QList<SetAsideEntry>& entries)
{
    if (entries.isEmpty())
        return;
    {
        std::lock_guard lock(write_->mutex());
        Transaction tx(*write_);
        auto toReview = write_->prepare("UPDATE dup_group SET state = 'to_review' WHERE id = ?");
        for (const SetAsideEntry& e : entries) {
            if (e.items.isEmpty())
                continue;
            const QString ids = idList(e.items);
            const char* kind = e.kind == SetAsideEntry::Kind::KeptAll ? "keep_all" : "not_duplicate";
            write_->exec(QStringLiteral("DELETE FROM dup_decision WHERE kind = '%1' AND item_a IN (%2) AND item_b IN (%2)")
                             .arg(QString::fromLatin1(kind), ids));
            if (e.kind == SetAsideEntry::Kind::KeptAll && e.group) {
                toReview.bind(1, e.group);
                toReview.run();
                toReview.reset();
            }
        }
        tx.commit();
    }
    changed();
}

} // namespace picazoo

#include "duplicates/DuplicateStore.h"

#include "index/Database.h"

#include <QDateTime>

#include <algorithm>

namespace picazoo {

const char* const kReclaimableSql =
    "UPDATE dup_group SET reclaimable_bytes = ("
    " SELECT COALESCE(SUM(i.size_bytes), 0) FROM dup_member m JOIN item i ON i.id = m.item_id"
    " LEFT JOIN dup_choice c ON c.item_id = m.item_id"
    " WHERE m.group_id = dup_group.id AND COALESCE(c.keep, m.item_id = dup_group.center_item_id) = 0)"
    " WHERE id = ?";

namespace {

QString levelClause(LevelFilter f)
{
    switch (f) {
    case LevelFilter::Identical: return QStringLiteral(" AND level = 'identical'");
    case LevelFilter::SameImage: return QStringLiteral(" AND level = 'same_image'");
    case LevelFilter::Similar: return QStringLiteral(" AND level = 'similar'");
    case LevelFilter::All: break;
    }
    return {};
}

qint64 nowMs() { return QDateTime::currentMSecsSinceEpoch(); }

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

QList<GroupId> DuplicateStore::groups(LevelFilter filter) const
{
    std::lock_guard lock(read_->mutex());
    QList<GroupId> out;
    const QString sql = QStringLiteral("SELECT id FROM dup_group WHERE state = 'to_review'") + levelClause(filter) +
                        QStringLiteral(" ORDER BY reclaimable_bytes DESC, id");
    auto q = read_->prepare(sql.toUtf8().constData());
    while (q.step())
        out << q.int64(0);
    return out;
}

std::optional<DuplicateGroup> DuplicateStore::group(GroupId id) const
{
    std::lock_guard lock(read_->mutex());
    auto g = read_->prepare("SELECT id, center_item_id, keeper_reason, level, reclaimable_bytes, state"
                            " FROM dup_group WHERE id = ?");
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
    auto m = read_->prepare("SELECT m.item_id, m.level, m.similarity, m.rotation, c.keep FROM dup_member m"
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
        c.keep = c.userChoice ? m.int32(4) != 0 : c.isCenter;
        out.copies << c;
    }
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

void DuplicateStore::pairDecision(const QList<ItemId>& a, const QList<ItemId>& b, const char* kind)
{
    std::lock_guard lock(write_->mutex());
    Transaction tx(*write_);
    auto put = write_->prepare("INSERT OR REPLACE INTO dup_decision (item_a, item_b, kind, decided_at) VALUES (?, ?, ?, ?)");
    for (ItemId x : a)
        for (ItemId y : b) {
            if (x == y)
                continue;
            put.bind(1, std::min(x, y)).bind(2, std::max(x, y)).bind(3, QString::fromLatin1(kind)).bind(4, nowMs());
            put.run();
            put.reset();
        }
    tx.commit();
}

void DuplicateStore::markNotDuplicates(GroupId groupId)
{
    const auto g = group(groupId);
    if (!g)
        return;
    QList<ItemId> ids;
    for (const auto& c : g->copies)
        ids << c.item;
    pairDecision(ids, ids, "not_duplicate");
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
    pairDecision({item}, others, "not_duplicate");
    {
        std::lock_guard lock(write_->mutex());
        auto del = write_->prepare("DELETE FROM dup_choice WHERE item_id = ?");
        del.bind(1, item);
        del.run();
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
    pairDecision(ids, ids, "keep_all");
    {
        std::lock_guard lock(write_->mutex());
        auto upd = write_->prepare("UPDATE dup_group SET state = 'kept_all' WHERE id = ?");
        upd.bind(1, groupId);
        upd.run();
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

} // namespace picazoo

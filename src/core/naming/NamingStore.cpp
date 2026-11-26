#include "naming/NamingStore.h"

#include "index/Database.h"
#include "naming/NamingDb.h"
#include "naming/NamingDetector.h"

#include <QDateTime>
#include <QSet>

namespace picazoo {

namespace {

// Sépare la racine d'origine du numéro d'un groupe issu d'une scission : ce caractère de
// contrôle ne peut pas figurer dans un nom de fichier Windows.
constexpr QChar kSplitMark(0x1F);

qint64 nowMs() { return QDateTime::currentMSecsSinceEpoch(); }

struct GroupHead {
    NamingGroupId id = 0;
    QString rootKey;
    QString name;
};

std::optional<GroupHead> head(Database& db, NamingGroupId id)
{
    auto q = db.prepare("SELECT root_key, display_name FROM naming_group WHERE id = ?");
    q.bind(1, id);
    if (!q.step())
        return std::nullopt;
    return GroupHead{id, q.text(0), q.text(1)};
}

// Racine détectée aujourd'hui pour cet élément ; vide si aucune. std::nullopt : élément absent.
std::optional<QString> rootOf(Database& db, ItemId item)
{
    auto q = db.prepare("SELECT file_name FROM item WHERE id = ?");
    q.bind(1, item);
    if (!q.step())
        return std::nullopt;
    const auto detected = detectRoot(baseNameOf(q.text(0)));
    return detected ? detected->normalizedKey : QString();
}

struct MemberRoot {
    ItemId item;
    QString root;
};

QList<MemberRoot> memberRoots(Database& db, NamingGroupId group)
{
    QList<MemberRoot> out;
    auto q = db.prepare("SELECT i.id, i.file_name FROM naming_member m JOIN item i ON i.id = m.item_id"
                        " WHERE m.group_id = ?");
    q.bind(1, group);
    while (q.step()) {
        const auto detected = detectRoot(baseNameOf(q.text(1)));
        out << MemberRoot{q.int64(0), detected ? detected->normalizedKey : QString()};
    }
    return out;
}

void confirmGroup(Database& db, NamingGroupId id)
{
    auto q = db.prepare("UPDATE naming_group SET state = 'confirmed', updated_at = ? WHERE id = ?");
    q.bind(1, nowMs()).bind(2, id);
    q.run();
}

void exclude(Database& db, ItemId item, const QString& root, const QString& rejectedRoot)
{
    auto q = db.prepare("INSERT OR REPLACE INTO naming_exclusion (item_id, root_key, rejected_root) VALUES (?, ?, ?)");
    q.bind(1, item).bind(2, root);
    if (rejectedRoot.isEmpty())
        q.bindNull(3);
    else
        q.bind(3, rejectedRoot);
    q.run();
}

void removeMember(Database& db, ItemId item)
{
    auto q = db.prepare("DELETE FROM naming_member WHERE item_id = ?");
    q.bind(1, item);
    q.run();
}

void writeDecision(Database& db, const QString& root, const char* kind, const QString& target, const QString& label)
{
    auto q = db.prepare("INSERT OR REPLACE INTO naming_decision (root_key, kind, target_root_key, label, decided_at)"
                        " VALUES (?, ?, ?, ?, ?)");
    q.bind(1, root).bind(2, QString::fromLatin1(kind));
    if (target.isEmpty())
        q.bindNull(3);
    else
        q.bind(3, target);
    q.bind(4, label).bind(5, nowMs());
    q.run();
}

QString likePattern(QString text)
{
    text.replace(u'\\', QStringLiteral("\\\\"));
    text.replace(u'%', QStringLiteral("\\%"));
    text.replace(u'_', QStringLiteral("\\_"));
    return u'%' + text + u'%';
}

} // namespace

NamingStore::NamingStore(const QString& libraryDbPath)
    : read_(Database::open(libraryDbPath, true)), write_(Database::open(libraryDbPath))
{
}

NamingStore::~NamingStore() = default;

void NamingStore::changed(const QStringList& keys)
{
    if (onChanged_)
        onChanged_(keys);
}

// ------------------------------------------------------------------ Lectures

QList<NamingGroupId> NamingStore::groups(ConfidenceFilter filter) const
{
    QList<NamingGroupId> out;
    for (const auto& row : rows(filter))
        out << row.id;
    return out;
}

QList<NamingGroupRow> NamingStore::rows(ConfidenceFilter filter) const
{
    std::lock_guard lock(read_->mutex());
    QString where;
    if (filter != ConfidenceFilter::All) {
        const Confidence c = filter == ConfidenceFilter::RegularSeries ? Confidence::RegularSeries
                             : filter == ConfidenceFilter::SameRoot    ? Confidence::SameRoot
                                                                       : Confidence::GenericRoot;
        where = QStringLiteral(" WHERE g.confidence = '%1'").arg(toDb(c));
    }
    // Un élément a pu disparaître depuis le dernier repérage : le nombre vient des membres.
    const QString sql = QStringLiteral(
                            "SELECT g.id, g.display_name, g.confidence, g.state, COUNT(*), SUM(i.id), SUM(i.version),"
                            " SUM(i.thumb_state = 'done'), SUM(w.state != 'available'), group_concat(i.file_name, '/')"
                            " FROM naming_group g JOIN naming_member m ON m.group_id = g.id"
                            " JOIN item i ON i.id = m.item_id JOIN folder f ON f.id = i.folder_id"
                            " JOIN watched_root w ON w.id = f.root_id") +
                        where +
                        QStringLiteral(" GROUP BY g.id HAVING COUNT(*) >= 2"
                                       " ORDER BY g.display_name COLLATE NOCASE, g.id");
    auto q = read_->prepare(sql.toUtf8().constData());
    QList<NamingGroupRow> out;
    while (q.step()) {
        QStringList parts;
        for (int col = 1; col <= 9; ++col)
            parts << q.text(col);
        out << NamingGroupRow{q.int64(0), q.text(1), q.int32(4), parts.join(u'|'), confidenceFromDb(q.text(2))};
    }
    return out;
}

std::optional<NamingGroup> NamingStore::group(NamingGroupId id) const
{
    std::lock_guard lock(read_->mutex());
    auto q = read_->prepare("SELECT root_key, display_name, confidence, state, renamed FROM naming_group WHERE id = ?");
    q.bind(1, id);
    if (!q.step())
        return std::nullopt;
    NamingGroup g;
    g.id = id;
    g.rootKey = q.text(0);
    g.displayName = q.text(1);
    g.confidence = confidenceFromDb(q.text(2));
    g.state = namingGroupStateFromDb(q.text(3));
    g.renamed = q.int32(4) != 0;

    auto m = read_->prepare("SELECT m.item_id, m.detected_root, m.manual, i.file_name, f.path, i.kind, i.thumb_state,"
                            " i.version, w.state FROM naming_member m JOIN item i ON i.id = m.item_id"
                            " JOIN folder f ON f.id = i.folder_id JOIN watched_root w ON w.id = f.root_id"
                            " WHERE m.group_id = ? ORDER BY i.file_name COLLATE NOCASE, i.id");
    m.bind(1, id);
    while (m.step()) {
        NamingMember member;
        member.item = m.int64(0);
        member.detectedRoot = m.text(1);
        member.manual = m.int32(2) != 0;
        member.fileName = m.text(3);
        member.folderPath = m.text(4);
        member.kind = kindFromDb(m.text(5));
        member.thumbState = thumbStateFromDb(m.text(6));
        member.version = m.int32(7);
        member.available = rootStateFromDb(m.text(8)) == RootState::Available;
        g.members << member;
    }
    return g;
}

std::optional<NamingGroupId> NamingStore::groupOf(ItemId item) const
{
    std::lock_guard lock(read_->mutex());
    auto q = read_->prepare("SELECT m.group_id FROM naming_member m WHERE m.item_id = ?"
                            " AND (SELECT COUNT(*) FROM naming_member o WHERE o.group_id = m.group_id) >= 2");
    q.bind(1, item);
    if (!q.step())
        return std::nullopt;
    return q.int64(0);
}

std::optional<NamingGroupRow> NamingStore::rowOf(ItemId item) const
{
    std::lock_guard lock(read_->mutex());
    auto q = read_->prepare("SELECT g.id, g.display_name, COUNT(*) FROM naming_member m"
                            " JOIN naming_group g ON g.id = m.group_id"
                            " JOIN naming_member o ON o.group_id = g.id"
                            " WHERE m.item_id = ? GROUP BY g.id HAVING COUNT(*) >= 2");
    q.bind(1, item);
    if (!q.step())
        return std::nullopt;
    return NamingGroupRow{q.int64(0), q.text(1), q.int32(2), {}};
}

NamingSummary NamingStore::summary() const
{
    std::lock_guard lock(read_->mutex());
    NamingSummary s;
    auto q = read_->prepare("SELECT g.confidence, COUNT(*) FROM naming_group g WHERE"
                            " (SELECT COUNT(*) FROM naming_member m WHERE m.group_id = g.id) >= 2"
                            " GROUP BY g.confidence");
    while (q.step()) {
        const int n = q.int32(1);
        switch (confidenceFromDb(q.text(0))) {
        case Confidence::RegularSeries: s.regularSeries = n; break;
        case Confidence::SameRoot: s.sameRoot = n; break;
        case Confidence::GenericRoot: s.genericRoot = n; break;
        }
        s.groupCount += n;
    }
    return s;
}

QList<NamingDecision> NamingStore::decisions() const
{
    std::lock_guard lock(read_->mutex());
    QList<NamingDecision> out;
    const NamingRules rules = NamingRules::load(*read_);
    auto name = read_->prepare("SELECT display_name FROM naming_group WHERE root_key = ?");
    auto q = read_->prepare("SELECT root_key, kind, label FROM naming_decision ORDER BY decided_at DESC, root_key");
    while (q.step()) {
        NamingDecision d;
        d.rootKey = q.text(0);
        d.kind = q.text(1) == u"rejected" ? NamingDecision::Kind::Rejected : NamingDecision::Kind::MergedInto;
        d.label = q.text(2);
        if (d.kind == NamingDecision::Kind::MergedInto) {
            name.reset();
            name.bind(1, rules.resolve(d.rootKey));
            if (name.step())
                d.targetName = name.text(0);
            name.reset();
        }
        out << d;
    }
    return out;
}

QList<NamingItem> NamingStore::searchItems(const QString& text, NamingGroupId excludeGroup, int limit) const
{
    std::lock_guard lock(read_->mutex());
    auto q = read_->prepare("SELECT i.id, i.file_name, f.path, i.thumb_state, i.version FROM item i"
                            " JOIN folder f ON f.id = i.folder_id WHERE i.file_name LIKE ? ESCAPE '\\'"
                            " AND NOT EXISTS (SELECT 1 FROM naming_member m WHERE m.item_id = i.id AND m.group_id = ?)"
                            " ORDER BY i.file_name COLLATE NOCASE, i.id LIMIT ?");
    q.bind(1, likePattern(text.trimmed())).bind(2, excludeGroup).bind(3, limit);
    QList<NamingItem> out;
    while (q.step())
        out << NamingItem{q.int64(0), q.text(1), q.text(2), thumbStateFromDb(q.text(3)), q.int32(4)};
    return out;
}

// ------------------------------------------------------------------ Décisions

bool NamingStore::addToGroup(NamingGroupId group, ItemId item)
{
    QStringList keys;
    {
        Transaction tx(*write_);
        const auto target = head(*write_, group);
        const auto root = rootOf(*write_, item);
        if (!target || !root)
            return false;
        std::optional<NamingGroupId> previous;
        {
            auto q = write_->prepare("SELECT group_id FROM naming_member WHERE item_id = ?");
            q.bind(1, item);
            if (q.step())
                previous = q.int64(0);
        }
        if (previous == group)
            return false;
        removeMember(*write_, item); // un élément appartient à un seul groupe de nommage
        if (previous) {
            if (const auto p = head(*write_, *previous))
                keys << p->rootKey;
            refreshNamingGroup(*write_, *previous);
        }
        auto forget = write_->prepare("DELETE FROM naming_exclusion WHERE item_id = ?");
        forget.bind(1, item);
        forget.run();
        auto insert = write_->prepare("INSERT INTO naming_member (group_id, item_id, detected_root, manual)"
                                      " VALUES (?, ?, ?, 1)");
        insert.bind(1, group).bind(2, item).bind(3, *root);
        insert.run();
        confirmGroup(*write_, group);
        refreshNamingGroup(*write_, group);
        keys << target->rootKey;
        if (!root->isEmpty())
            keys << *root;
        tx.commit();
    }
    changed(keys);
    return true;
}

bool NamingStore::removeFromGroup(NamingGroupId group, ItemId item)
{
    QStringList keys;
    {
        Transaction tx(*write_);
        const auto source = head(*write_, group);
        const auto root = rootOf(*write_, item);
        if (!source || !root)
            return false;
        auto del = write_->prepare("DELETE FROM naming_member WHERE group_id = ? AND item_id = ?");
        del.bind(1, group).bind(2, item);
        del.run();
        if (write_->changes() == 0)
            return false;
        exclude(*write_, item, source->rootKey, {}); // ce groupe-ci seulement
        confirmGroup(*write_, group);
        refreshNamingGroup(*write_, group);
        keys << source->rootKey;
        if (!root->isEmpty())
            keys << *root;
        tx.commit();
    }
    changed(keys);
    return true;
}

NamingGroupId NamingStore::mergeGroups(NamingGroupId into, NamingGroupId from)
{
    QStringList keys;
    {
        Transaction tx(*write_);
        const auto target = head(*write_, into);
        const auto source = head(*write_, from);
        if (into == from || !target || !source)
            return 0;
        auto move = write_->prepare("UPDATE naming_member SET group_id = ?, manual = 1 WHERE group_id = ?");
        move.bind(1, into).bind(2, from);
        move.run();
        // Les racines déjà fusionnées dans la source la suivent par NamingRules::resolve : la chaîne
        // est gardée pour pouvoir annuler chaque fusion séparément.
        auto clear = write_->prepare("DELETE FROM naming_decision WHERE root_key = ?");
        clear.bind(1, target->rootKey);
        clear.run();
        // Un groupe issu d'une scission n'a pas de racine propre : rien à retenir pour lui.
        if (!source->rootKey.contains(kSplitMark))
            writeDecision(*write_, source->rootKey, "merged_into", target->rootKey, source->name);
        deleteNamingGroup(*write_, from);
        confirmGroup(*write_, into);
        refreshNamingGroup(*write_, into);
        keys << target->rootKey << source->rootKey;
        tx.commit();
    }
    changed(keys);
    return into;
}

NamingGroupId NamingStore::splitGroup(NamingGroupId group, const QList<ItemId>& toSplitOff, const QString& newName)
{
    QStringList keys;
    NamingGroupId created = 0;
    {
        Transaction tx(*write_);
        const auto source = head(*write_, group);
        const QString name = newName.trimmed();
        if (!source || name.isEmpty())
            return 0;
        const QSet<ItemId> wanted(toSplitOff.begin(), toSplitOff.end());
        QList<MemberRoot> moving;
        int staying = 0;
        for (const MemberRoot& m : memberRoots(*write_, group)) {
            if (wanted.contains(m.item))
                moving << m;
            else
                ++staying;
        }
        if (moving.size() < 2 || staying < 2)
            return 0;

        const QString base = source->rootKey.section(kSplitMark, 0, 0);
        QString key;
        auto taken = write_->prepare("SELECT 1 FROM naming_group WHERE root_key = ?1"
                                     " UNION ALL SELECT 1 FROM naming_decision WHERE root_key = ?1");
        for (int n = 2;; ++n) {
            key = base + kSplitMark + QString::number(n);
            taken.reset();
            taken.bind(1, key);
            if (!taken.step())
                break;
        }
        taken.reset();

        auto insert = write_->prepare("INSERT INTO naming_group (root_key, display_name, confidence, member_count,"
                                      " state, renamed, updated_at) VALUES (?, ?, 'same_root', ?, 'confirmed', 1, ?)");
        insert.bind(1, key).bind(2, name).bind(3, qint64(moving.size())).bind(4, nowMs());
        insert.run();
        created = write_->lastInsertId();
        auto move = write_->prepare("UPDATE naming_member SET group_id = ?, manual = 1 WHERE item_id = ?");
        for (const MemberRoot& m : std::as_const(moving)) {
            move.bind(1, created).bind(2, m.item);
            move.run();
        }
        confirmGroup(*write_, group);
        refreshNamingGroup(*write_, group);
        refreshNamingGroup(*write_, created);
        keys << source->rootKey << key;
        tx.commit();
    }
    changed(keys);
    return created;
}

bool NamingStore::rename(NamingGroupId group, const QString& displayName)
{
    QStringList keys;
    {
        Transaction tx(*write_);
        const auto g = head(*write_, group);
        const QString name = displayName.trimmed();
        if (!g || name.isEmpty())
            return false;
        auto q = write_->prepare("UPDATE naming_group SET display_name = ?, renamed = 1 WHERE id = ?");
        q.bind(1, name).bind(2, group);
        q.run();
        confirmGroup(*write_, group);
        keys << g->rootKey;
        tx.commit();
    }
    changed(keys);
    return true;
}

bool NamingStore::confirm(NamingGroupId group)
{
    QStringList keys;
    {
        Transaction tx(*write_);
        const auto g = head(*write_, group);
        if (!g)
            return false;
        confirmGroup(*write_, group);
        keys << g->rootKey;
        tx.commit();
    }
    changed(keys);
    return true;
}

bool NamingStore::markNotAGroup(NamingGroupId group)
{
    QStringList keys;
    {
        Transaction tx(*write_);
        const auto g = head(*write_, group);
        if (!g)
            return false;
        const NamingRules rules = NamingRules::load(*write_);
        // Groupe issu d'une scission : ses éléments ne retournent pas dans le groupe dont ils
        // ont été détachés. Les éléments ajoutés à la main depuis une autre racine retrouvent
        // simplement la leur.
        const QString base = g->rootKey.section(kSplitMark, 0, 0);
        const QString origin = base != g->rootKey ? rules.resolve(base) : QString();
        for (const MemberRoot& m : memberRoots(*write_, group)) {
            if (m.root.isEmpty())
                continue;
            if (!origin.isEmpty() && rules.resolve(m.root) == origin)
                exclude(*write_, m.item, origin, g->rootKey);
            keys << m.root;
        }
        writeDecision(*write_, g->rootKey, "rejected", {}, g->name);
        deleteNamingGroup(*write_, group);
        keys << g->rootKey;
        tx.commit();
    }
    changed(keys);
    return true;
}

bool NamingStore::undoDecision(const QString& rootKey)
{
    QStringList keys{rootKey};
    {
        Transaction tx(*write_);
        QString kind, target;
        {
            auto q = write_->prepare("SELECT kind, target_root_key FROM naming_decision WHERE root_key = ?");
            q.bind(1, rootKey);
            if (!q.step())
                return false;
            kind = q.text(0);
            target = q.text(1);
        }
        if (kind == u"merged_into") {
            // Les éléments de cette racine quittent le groupe qui les avait absorbés ; le
            // repérage les regroupera de nouveau entre eux.
            const NamingRules rules = NamingRules::load(*write_);
            const QString resolved = rules.resolve(target);
            auto throughUndone = [&](QString root) {
                QSet<QString> seen;
                while (!root.isEmpty() && !seen.contains(root)) {
                    if (root == rootKey)
                        return true;
                    seen.insert(root);
                    root = rules.merged.value(root);
                }
                return false;
            };
            std::optional<NamingGroupId> absorbing;
            {
                auto q = write_->prepare("SELECT id FROM naming_group WHERE root_key = ?");
                q.bind(1, resolved);
                if (q.step())
                    absorbing = q.int64(0);
            }
            if (absorbing) {
                for (const MemberRoot& m : memberRoots(*write_, *absorbing)) {
                    if (!m.root.isEmpty() && throughUndone(m.root)) {
                        removeMember(*write_, m.item);
                        keys << m.root;
                    }
                }
                refreshNamingGroup(*write_, *absorbing);
            }
            keys << resolved;
        } else {
            {
                auto q = write_->prepare("SELECT root_key FROM naming_exclusion WHERE rejected_root = ?");
                q.bind(1, rootKey);
                while (q.step())
                    keys << q.text(0);
            }
            auto lift = write_->prepare("DELETE FROM naming_exclusion WHERE rejected_root = ?");
            lift.bind(1, rootKey);
            lift.run();
        }
        auto del = write_->prepare("DELETE FROM naming_decision WHERE root_key = ?");
        del.bind(1, rootKey);
        del.run();
        tx.commit();
    }
    changed(keys);
    return true;
}

} // namespace picazoo

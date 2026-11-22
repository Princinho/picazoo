#include "index/LibraryDb.h"

#include "index/Database.h"
#include "media/Formats.h"
#include "platform/win/Paths.h"

#include <QStringList>

#include <algorithm>

namespace picazoo {

namespace {

QString idList(const QList<qint64>& ids)
{
    QStringList parts;
    parts.reserve(ids.size());
    for (qint64 id : ids)
        parts << QString::number(id);
    return parts.join(u',');
}

template <typename Fn> void forChunks(const QList<qint64>& ids, qsizetype chunk, Fn fn)
{
    for (qsizetype i = 0; i < ids.size(); i += chunk)
        fn(ids.mid(i, chunk));
}

QByteArray identityKey(qint64 volume, const QByteArray& fileId)
{
    if (fileId.isEmpty())
        return {};
    return QByteArray::number(volume) + ':' + fileId;
}

const char* kItemColumns =
    "SELECT i.id, i.folder_id, i.file_name, f.path, i.kind, i.format, i.size_bytes, i.mtime, i.taken_at,"
    " i.sort_date, i.width, i.height, i.orientation, i.camera, i.duration_ms, i.meta_state, i.thumb_state,"
    " i.version FROM item i JOIN folder f ON f.id = i.folder_id";

Item readItem(const Statement& q)
{
    Item it;
    it.id = q.int64(0);
    it.folderId = q.int64(1);
    it.fileName = q.text(2);
    it.folderPath = q.text(3);
    it.kind = kindFromDb(q.text(4));
    it.format = formatFromName(q.text(5));
    it.sizeBytes = q.int64(6);
    it.mtime = q.int64(7);
    it.takenAt = q.optInt64(8);
    it.sortDate = q.int64(9);
    it.width = q.int32(10);
    it.height = q.int32(11);
    it.orientation = q.int32(12);
    it.camera = q.text(13);
    it.durationMs = q.optInt64(14);
    it.metaState = metaStateFromDb(q.text(15));
    it.thumbState = thumbStateFromDb(q.text(16));
    it.version = q.int32(17);
    return it;
}

void refreshFoldersNoTx(Database& db, const QSet<FolderId>& folders)
{
    auto upd = db.prepare("UPDATE folder SET"
                          " item_count = (SELECT COUNT(*) FROM item WHERE folder_id = ?1),"
                          " date = (SELECT MIN(sort_date) FROM item WHERE folder_id = ?1)"
                          " WHERE id = ?1");
    auto del = db.prepare("DELETE FROM folder WHERE id = ? AND item_count = 0");
    for (FolderId f : folders) {
        upd.bind(1, f);
        upd.run();
        del.bind(1, f);
        del.run();
    }
}

} // namespace

// ------------------------------------------------------------------ Racines

QList<WatchedRoot> LibraryDb::roots(Database& read)
{
    std::lock_guard lock(read.mutex());
    QList<WatchedRoot> out;
    QHash<RootId, qsizetype> index;
    auto q = read.prepare("SELECT id, path, state, last_scan_at FROM watched_root ORDER BY path COLLATE NOCASE");
    while (q.step()) {
        WatchedRoot r;
        r.id = q.int64(0);
        r.path = q.text(1);
        r.state = rootStateFromDb(q.text(2));
        r.lastScanAt = q.optInt64(3);
        index.insert(r.id, out.size());
        out << r;
    }
    auto ex = read.prepare("SELECT root_id, path FROM excluded_path ORDER BY path COLLATE NOCASE");
    while (ex.step()) {
        const auto it = index.constFind(ex.int64(0));
        if (it != index.constEnd())
            out[*it].excluded << ex.text(1);
    }
    return out;
}

RootId LibraryDb::addRoot(const QString& path, qint64 now)
{
    Transaction tx(db_);
    auto find = db_.prepare("SELECT id FROM watched_root WHERE path = ?");
    find.bind(1, path);
    if (find.step())
        return find.int64(0);
    auto ins = db_.prepare("INSERT INTO watched_root (path, state, added_at) VALUES (?, 'available', ?)");
    ins.bind(1, path).bind(2, now);
    ins.run();
    const RootId id = db_.lastInsertId();
    tx.commit();
    return id;
}

QList<ItemId> LibraryDb::removeRoot(RootId id)
{
    Transaction tx(db_);
    QList<ItemId> ids;
    auto q = db_.prepare("SELECT i.id FROM item i JOIN folder f ON f.id = i.folder_id WHERE f.root_id = ?");
    q.bind(1, id);
    while (q.step())
        ids << q.int64(0);
    auto del = db_.prepare("DELETE FROM watched_root WHERE id = ?");
    del.bind(1, id);
    del.run();
    tx.commit();
    return ids;
}

void LibraryDb::setRootState(RootId id, RootState state)
{
    std::lock_guard lock(db_.mutex());
    auto q = db_.prepare("UPDATE watched_root SET state = ? WHERE id = ?");
    q.bind(1, toDb(state)).bind(2, id);
    q.run();
}

void LibraryDb::setRootScanned(RootId id, qint64 when)
{
    std::lock_guard lock(db_.mutex());
    auto q = db_.prepare("UPDATE watched_root SET last_scan_at = ?, state = 'available' WHERE id = ?");
    q.bind(1, when).bind(2, id);
    q.run();
}

void LibraryDb::addExcluded(RootId id, const QString& path)
{
    std::lock_guard lock(db_.mutex());
    auto q = db_.prepare("INSERT OR IGNORE INTO excluded_path (root_id, path) VALUES (?, ?)");
    q.bind(1, id).bind(2, path);
    q.run();
}

void LibraryDb::removeExcluded(RootId id, const QString& path)
{
    std::lock_guard lock(db_.mutex());
    auto q = db_.prepare("DELETE FROM excluded_path WHERE root_id = ? AND path = ?");
    q.bind(1, id).bind(2, path);
    q.run();
}

QList<ItemId> LibraryDb::removeItemsUnder(const QString& path, QSet<FolderId>* touched)
{
    Transaction tx(db_);
    QList<FolderId> folders;
    auto f = db_.prepare("SELECT id, path FROM folder");
    while (f.step()) {
        if (platform::isInside(f.text(1), path))
            folders << f.int64(0);
    }
    QList<ItemId> ids;
    forChunks(folders, 500, [&](const QList<qint64>& chunk) {
        auto q = db_.prepare(("SELECT id FROM item WHERE folder_id IN (" + idList(chunk) + ")").toUtf8().constData());
        while (q.step())
            ids << q.int64(0);
        db_.exec("DELETE FROM folder WHERE id IN (" + idList(chunk) + ")");
    });
    tx.commit();
    if (touched)
        touched->unite(QSet<FolderId>(folders.begin(), folders.end()));
    return ids;
}

// ------------------------------------------------------------------ Analyse

SyncResult LibraryDb::syncFolder(const DiscoveredFolder& folder, qint64 stamp, const IdentityFn& identity)
{
    struct Existing {
        ItemId id;
        qint64 size;
        qint64 mtime;
    };
    SyncResult result;
    std::optional<FolderId> folderId;
    RootId currentRoot = 0;
    QHash<QString, Existing> existing;
    {
        std::lock_guard lock(db_.mutex());
        auto f = db_.prepare("SELECT id, root_id FROM folder WHERE path = ?");
        f.bind(1, folder.path);
        if (f.step()) {
            folderId = f.int64(0);
            currentRoot = f.int64(1);
            auto q = db_.prepare("SELECT id, file_name, size_bytes, mtime FROM item WHERE folder_id = ?");
            q.bind(1, *folderId);
            while (q.step())
                existing.insert(q.text(1).toLower(), Existing{q.int64(0), q.int64(2), q.int64(3)});
        }
    }

    // Identités des nouveaux fichiers, calculées hors transaction (E/S).
    QHash<QString, platform::FileId> ids;
    for (const auto& file : folder.files) {
        if (!existing.contains(file.name.toLower()) && identity) {
            if (auto id = identity(platform::joinPath(folder.path, file.name)))
                ids.insert(file.name, *id);
        }
    }

    Transaction tx(db_);
    if (!folderId) {
        auto ins = db_.prepare("INSERT INTO folder (root_id, path, parent_path, name) VALUES (?, ?, ?, ?)");
        ins.bind(1, folder.rootId).bind(2, folder.path).bind(3, platform::parentPath(folder.path));
        ins.bind(4, platform::lastSegment(folder.path));
        ins.run();
        folderId = db_.lastInsertId();
    } else if (currentRoot != folder.rootId) {
        auto upd = db_.prepare("UPDATE folder SET root_id = ? WHERE id = ?");
        upd.bind(1, folder.rootId).bind(2, *folderId);
        upd.run();
    }
    result.folderId = *folderId;

    auto seen = db_.prepare("UPDATE item SET last_seen_at = ?, file_name = ? WHERE id = ?");
    auto changed = db_.prepare("UPDATE item SET size_bytes = ?, mtime = ?, version = version + 1,"
                               " meta_state = 'pending', thumb_state = 'pending', quick_hash = NULL,"
                               " taken_at = NULL, sort_date = ?, last_seen_at = ?, file_name = ? WHERE id = ?");
    auto insert = db_.prepare("INSERT INTO item (folder_id, file_name, kind, format, size_bytes, mtime,"
                              " volume_serial, file_id, sort_date, last_seen_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    for (const auto& file : folder.files) {
        const auto it = existing.constFind(file.name.toLower());
        if (it != existing.constEnd()) {
            if (it->size == file.size && it->mtime == file.mtime) {
                seen.bind(1, stamp).bind(2, file.name).bind(3, it->id);
                seen.run();
            } else {
                changed.bind(1, file.size).bind(2, file.mtime).bind(3, file.mtime).bind(4, stamp);
                changed.bind(5, file.name).bind(6, it->id);
                changed.run();
                result.changed << it->id;
            }
            continue;
        }
        insert.bind(1, *folderId).bind(2, file.name).bind(3, toDb(kindOf(file.format)));
        insert.bind(4, formatName(file.format)).bind(5, file.size).bind(6, file.mtime);
        const auto id = ids.constFind(file.name);
        if (id != ids.constEnd() && !id->id.isEmpty()) {
            insert.bind(7, qint64(id->volumeSerial)).bind(8, id->id);
        } else {
            insert.bindNull(7).bindNull(8);
        }
        insert.bind(9, file.mtime).bind(10, stamp);
        insert.run();
        result.created << db_.lastInsertId();
    }
    refreshFoldersNoTx(db_, {*folderId});
    tx.commit();
    return result;
}

ScanOutcome LibraryDb::finishScan(const QList<RootId>& roots, const QStringList& skippedPaths, qint64 stamp,
                                  const QList<ItemId>& createdThisScan, const QuickHashFn& quickHash,
                                  const QList<FolderId>& folderScope)
{
    ScanOutcome out;
    if (roots.isEmpty() && folderScope.isEmpty())
        return out;

    struct Row {
        ItemId id;
        FolderId folder;
        QString path;
        qint64 volume;
        QByteArray fileId;
        std::optional<qint64> quickHash;
        qint64 size;
        qint64 mtime;
        QString fileName;
    };
    auto readRows = [&](const QString& where, auto bindFn) {
        QList<Row> rows;
        const QString sql = "SELECT i.id, i.folder_id, f.path, i.file_name, i.volume_serial, i.file_id,"
                            " i.quick_hash, i.size_bytes, i.mtime FROM item i JOIN folder f ON f.id = i.folder_id"
                            " WHERE " + where;
        auto q = db_.prepare(sql.toUtf8().constData());
        bindFn(q);
        while (q.step()) {
            Row r{q.int64(0), q.int64(1), platform::joinPath(q.text(2), q.text(3)), q.int64(4), q.blob(5),
                  q.optInt64(6), q.int64(7), q.int64(8), q.text(3)};
            rows << r;
        }
        return rows;
    };

    Transaction tx(db_);
    QString scope = folderScope.isEmpty() ? QStringLiteral("f.root_id IN (%1)").arg(idList(roots))
                                          : QStringLiteral("i.folder_id IN (%1)").arg(idList(folderScope));
    QList<Row> missing = readRows(scope + QStringLiteral(" AND i.last_seen_at < ?"),
                                  [&](Statement& q) { q.bind(1, stamp); });
    missing.erase(std::remove_if(missing.begin(), missing.end(),
                                 [&](const Row& r) {
                                     return std::any_of(skippedPaths.begin(), skippedPaths.end(), [&](const QString& s) {
                                         return platform::isInside(r.path, s);
                                     });
                                 }),
                  missing.end());
    if (missing.isEmpty()) {
        tx.commit();
        return out;
    }

    // Nouveaux éléments de ce passage, candidats à un rapprochement (renommage / déplacement).
    QList<Row> created;
    forChunks(createdThisScan, 500, [&](const QList<qint64>& chunk) {
        created += readRows(QStringLiteral("i.id IN (%1)").arg(idList(chunk)), [](Statement&) {});
    });
    QHash<QByteArray, qsizetype> byIdentity;
    QMultiHash<qint64, qsizetype> bySize;
    for (qsizetype i = 0; i < created.size(); ++i) {
        const QByteArray key = identityKey(created[i].volume, created[i].fileId);
        if (!key.isEmpty())
            byIdentity.insert(key, i);
        bySize.insert(created[i].size, i);
    }
    QSet<qsizetype> consumed;
    QHash<qsizetype, std::optional<quint64>> hashCache;

    auto delItem = db_.prepare("DELETE FROM item WHERE id = ?");
    auto move = db_.prepare("UPDATE item SET folder_id = ?, file_name = ?, size_bytes = ?, mtime = ?,"
                            " volume_serial = ?, file_id = ?, last_seen_at = ? WHERE id = ?");
    auto bump = db_.prepare("UPDATE item SET version = version + 1, meta_state = 'pending',"
                            " thumb_state = 'pending', taken_at = NULL, sort_date = mtime WHERE id = ?");

    for (const Row& m : missing) {
        std::optional<qsizetype> match;
        const QByteArray key = identityKey(m.volume, m.fileId);
        if (!key.isEmpty()) {
            const auto it = byIdentity.constFind(key);
            if (it != byIdentity.constEnd() && !consumed.contains(*it))
                match = *it;
        }
        if (!match && m.quickHash) {
            for (auto it = bySize.constFind(m.size); it != bySize.constEnd() && it.key() == m.size; ++it) {
                if (consumed.contains(*it))
                    continue;
                if (!hashCache.contains(*it))
                    hashCache.insert(*it, quickHash ? quickHash(created[*it].path, created[*it].size) : std::nullopt);
                const auto h = hashCache.value(*it);
                if (h && qint64(*h) == *m.quickHash) {
                    match = *it;
                    break;
                }
            }
        }
        if (match) {
            const Row& n = created[*match];
            consumed.insert(*match);
            delItem.bind(1, n.id);
            delItem.run();
            move.bind(1, n.folder).bind(2, n.fileName).bind(3, n.size).bind(4, n.mtime);
            if (n.fileId.isEmpty())
                move.bindNull(5).bindNull(6);
            else
                move.bind(5, n.volume).bind(6, n.fileId);
            move.bind(7, stamp).bind(8, m.id);
            move.run();
            if (n.size != m.size || n.mtime != m.mtime) {
                bump.bind(1, m.id);
                bump.run();
            }
            out.mergedAway << n.id;
            out.moved << m.id;
            out.touchedFolders << m.folder << n.folder;
        } else {
            delItem.bind(1, m.id);
            delItem.run();
            out.removed << m.id;
            out.touchedFolders << m.folder;
        }
    }
    refreshFoldersNoTx(db_, out.touchedFolders);
    tx.commit();
    return out;
}

void LibraryDb::refreshFolders(const QSet<FolderId>& folders)
{
    if (folders.isEmpty())
        return;
    Transaction tx(db_);
    refreshFoldersNoTx(db_, folders);
    tx.commit();
}

// ----------------------------------------------------------------- Pipeline

QSet<FolderId> LibraryDb::applyUpdates(const QList<ItemUpdate>& updates)
{
    QSet<FolderId> touched;
    if (updates.isEmpty())
        return touched;
    Transaction tx(db_);
    auto meta = db_.prepare("UPDATE item SET taken_at = ?, sort_date = COALESCE(?, mtime), width = ?, height = ?,"
                            " orientation = ?, camera = ?, meta_state = 'done' WHERE id = ? AND version = ?");
    auto metaFail = db_.prepare("UPDATE item SET meta_state = 'failed' WHERE id = ? AND version = ?");
    auto thumb = db_.prepare("UPDATE item SET thumb_state = ? WHERE id = ? AND version = ?");
    auto hash = db_.prepare("UPDATE item SET quick_hash = ? WHERE id = ? AND version = ?");
    auto duration = db_.prepare("UPDATE item SET duration_ms = ? WHERE id = ? AND version = ?");
    auto folderOf = db_.prepare("SELECT folder_id FROM item WHERE id = ?");
    for (const auto& u : updates) {
        if (u.meta) {
            const Metadata& m = *u.meta;
            meta.bind(1, m.takenAt).bind(2, m.takenAt).bind(3, m.size.width()).bind(4, m.size.height());
            meta.bind(5, m.orientation);
            if (m.camera.isEmpty())
                meta.bindNull(6);
            else
                meta.bind(6, m.camera);
            meta.bind(7, u.id).bind(8, u.version);
            meta.run();
            folderOf.bind(1, u.id);
            if (folderOf.step())
                touched << folderOf.int64(0);
            folderOf.reset();
        } else if (u.metaFailed) {
            metaFail.bind(1, u.id).bind(2, u.version);
            metaFail.run();
        }
        if (u.thumbState) {
            thumb.bind(1, toDb(*u.thumbState)).bind(2, u.id).bind(3, u.version);
            thumb.run();
        }
        if (u.quickHash) {
            hash.bind(1, qint64(*u.quickHash)).bind(2, u.id).bind(3, u.version);
            hash.run();
        }
        if (u.durationMs) {
            duration.bind(1, *u.durationMs).bind(2, u.id).bind(3, u.version);
            duration.run();
        }
    }
    refreshFoldersNoTx(db_, touched);
    tx.commit();
    return touched;
}

QList<ItemId> LibraryDb::pendingWork(bool includeVideos) const
{
    std::lock_guard lock(db_.mutex());
    QList<ItemId> ids;
    const char* sql = includeVideos
                          ? "SELECT id FROM item WHERE meta_state = 'pending' OR thumb_state IN ('pending', 'provisional')"
                          : "SELECT id FROM item WHERE kind != 'video' AND"
                            " (meta_state = 'pending' OR thumb_state IN ('pending', 'provisional'))";
    auto q = db_.prepare(sql);
    while (q.step())
        ids << q.int64(0);
    return ids;
}

// ----------------------------------------------------------------- Lectures

QList<Folder> LibraryDb::folders(Database& read, FolderSort sort)
{
    std::lock_guard lock(read.mutex());
    const char* order = sort == FolderSort::NameAsc  ? " ORDER BY name COLLATE NOCASE, path COLLATE NOCASE"
                        : sort == FolderSort::DateAsc ? " ORDER BY date IS NULL, date ASC, name COLLATE NOCASE"
                                                      : " ORDER BY date IS NULL, date DESC, name COLLATE NOCASE";
    auto q = read.prepare((QByteArray("SELECT id, root_id, path, parent_path, name, item_count, date FROM folder"
                                      " WHERE item_count > 0") + order).constData());
    QList<Folder> out;
    while (q.step()) {
        Folder f;
        f.id = q.int64(0);
        f.rootId = q.int64(1);
        f.path = q.text(2);
        f.parentPath = q.text(3);
        f.name = q.text(4);
        f.itemCount = q.int32(5);
        f.date = q.optInt64(6);
        out << f;
    }
    return out;
}

QList<ItemSummary> LibraryDb::itemSummaries(Database& read, FolderId folder, ItemSort sort)
{
    std::lock_guard lock(read.mutex());
    const char* sql = sort == ItemSort::NameAsc
                          ? "SELECT id, folder_id, file_name, kind, thumb_state, version, duration_ms FROM item"
                            " WHERE folder_id = ? ORDER BY file_name COLLATE NOCASE"
                          : "SELECT id, folder_id, file_name, kind, thumb_state, version, duration_ms FROM item"
                            " WHERE folder_id = ? ORDER BY sort_date, file_name COLLATE NOCASE";
    auto q = read.prepare(sql);
    q.bind(1, folder);
    QList<ItemSummary> out;
    while (q.step()) {
        ItemSummary s;
        s.id = q.int64(0);
        s.folderId = q.int64(1);
        s.fileName = q.text(2);
        s.kind = kindFromDb(q.text(3));
        s.thumbState = thumbStateFromDb(q.text(4));
        s.version = q.int32(5);
        s.durationMs = q.optInt64(6);
        out << s;
    }
    return out;
}

QHash<FolderId, QList<ItemSummary>> LibraryDb::allItemSummaries(Database& read, ItemSort sort)
{
    std::lock_guard lock(read.mutex());
    const char* sql = sort == ItemSort::NameAsc
                          ? "SELECT id, folder_id, file_name, kind, thumb_state, version, duration_ms FROM item"
                            " ORDER BY folder_id, file_name COLLATE NOCASE"
                          : "SELECT id, folder_id, file_name, kind, thumb_state, version, duration_ms FROM item"
                            " ORDER BY folder_id, sort_date, file_name COLLATE NOCASE";
    auto q = read.prepare(sql);
    QHash<FolderId, QList<ItemSummary>> out;
    while (q.step()) {
        ItemSummary s;
        s.id = q.int64(0);
        s.folderId = q.int64(1);
        s.fileName = q.text(2);
        s.kind = kindFromDb(q.text(3));
        s.thumbState = thumbStateFromDb(q.text(4));
        s.version = q.int32(5);
        s.durationMs = q.optInt64(6);
        out[s.folderId] << s;
    }
    return out;
}

std::optional<Item> LibraryDb::item(Database& read, ItemId id)
{
    std::lock_guard lock(read.mutex());
    auto q = read.prepare((QByteArray(kItemColumns) + " WHERE i.id = ?").constData());
    q.bind(1, id);
    if (!q.step())
        return std::nullopt;
    return readItem(q);
}

std::optional<FolderId> LibraryDb::folderIdForPath(Database& read, const QString& path)
{
    std::lock_guard lock(read.mutex());
    auto q = read.prepare("SELECT id FROM folder WHERE path = ?");
    q.bind(1, path);
    if (!q.step())
        return std::nullopt;
    return q.int64(0);
}

QList<QPair<FolderId, QString>> LibraryDb::folderPaths() const
{
    std::lock_guard lock(db_.mutex());
    QList<QPair<FolderId, QString>> out;
    auto q = db_.prepare("SELECT id, path FROM folder");
    while (q.step())
        out.append({q.int64(0), q.text(1)});
    return out;
}

qint64 LibraryDb::itemCount(Database& read)
{
    std::lock_guard lock(read.mutex());
    auto q = read.prepare("SELECT COUNT(*) FROM item");
    return q.step() ? q.int64(0) : 0;
}

} // namespace picazoo

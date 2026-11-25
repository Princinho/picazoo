#include "fileops/FileTrash.h"

#include "index/Database.h"
#include "platform/win/DirectoryWalker.h"
#include "platform/win/Paths.h"
#include "platform/win/RecycleBin.h"

#include <windows.h>

#include <QDateTime>

namespace picazoo {

namespace {

qint64 nowMs() { return QDateTime::currentMSecsSinceEpoch(); }

bool stat(const QString& path, qint64* size, qint64* mtime)
{
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(platform::toWin32(path).c_str(), GetFileExInfoStandard, &data))
        return false;
    if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        return false;
    *size = (qint64(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
    *mtime = platform::fileTimeToEpochMs(data.ftLastWriteTime.dwLowDateTime, data.ftLastWriteTime.dwHighDateTime);
    return true;
}

} // namespace

QString toDb(OperationSource s)
{
    switch (s) {
    case OperationSource::Duplicates: return QStringLiteral("duplicates");
    case OperationSource::Library: return QStringLiteral("library");
    case OperationSource::Viewer: return QStringLiteral("viewer");
    }
    return QStringLiteral("duplicates");
}

OperationSource operationSourceFromDb(const QString& s)
{
    if (s == u"library")
        return OperationSource::Library;
    if (s == u"viewer")
        return OperationSource::Viewer;
    return OperationSource::Duplicates;
}

FileTrash::FileTrash(Database& db) : db_(db) {}

// Chemin long (« EGNAKO~1 » -> nom complet) : la corbeille de Windows note le chemin long, et
// la restauration le retrouve par comparaison de chemins.
static QString longPath(const QString& path)
{
    const std::wstring w = platform::toWin32(path);
    const DWORD n = GetLongPathNameW(w.c_str(), nullptr, 0);
    if (n == 0)
        return path;
    std::wstring out(n, L'\0');
    const DWORD m = GetLongPathNameW(w.c_str(), out.data(), n);
    if (m == 0 || m >= n)
        return path;
    out.resize(m);
    QString r = QString::fromStdWString(out);
    if (r.startsWith(QStringLiteral("\\\\?\\")))
        r = r.mid(4);
    return r;
}

std::optional<LooseTrashEntry> FileTrash::describe(const QString& rawPath, std::optional<ItemId> item)
{
    const QString path = longPath(rawPath);
    LooseTrashEntry e;
    if (!stat(path, &e.bytes, &e.mtime))
        return std::nullopt;
    e.item = item;
    e.path = path;
    return e;
}

QString FileTrash::recycle(const QString& path, QString* message)
{
    switch (platform::moveToRecycleBin(path, message)) {
    case platform::RecycleResult::Recycled: return QStringLiteral("trashed");
    case platform::RecycleResult::NoRecycleBin: return QStringLiteral("skipped_no_recycle_bin");
    case platform::RecycleResult::Locked: return QStringLiteral("skipped_locked");
    case platform::RecycleResult::Missing: return QStringLiteral("skipped_changed");
    case platform::RecycleResult::Failed: return QStringLiteral("failed");
    }
    return QStringLiteral("failed");
}

void FileTrash::setEntry(qint64 entryId, const QString& result, const QString& message)
{
    std::lock_guard lock(db_.mutex());
    auto q = db_.prepare("UPDATE file_operation_entry SET result = ?, message = ? WHERE id = ?");
    q.bind(1, result);
    if (message.isEmpty())
        q.bindNull(2);
    else
        q.bind(2, message);
    q.bind(3, entryId);
    q.run();
}

OperationId FileTrash::trash(const QList<LooseTrashEntry>& entries, OperationSource source)
{
    if (entries.isEmpty())
        return 0;
    OperationId id = 0;
    QList<qint64> entryIds;
    {
        // Journal d'abord : l'opération et chaque fichier sont écrits avant d'agir.
        std::lock_guard lock(db_.mutex());
        Transaction tx(db_);
        qint64 bytes = 0;
        for (const auto& e : entries)
            bytes += e.bytes;
        auto op = db_.prepare("INSERT INTO file_operation (kind, created_at, state, file_count, bytes, source)"
                              " VALUES ('trash', ?, 'planned', ?, ?, ?)");
        op.bind(1, nowMs()).bind(2, int(entries.size())).bind(3, bytes).bind(4, toDb(source));
        op.run();
        id = db_.lastInsertId();
        auto entry = db_.prepare("INSERT INTO file_operation_entry (operation_id, item_id, original_path, size_bytes,"
                                 " mtime, keeper_path, result) VALUES (?, ?, ?, ?, ?, NULL, 'pending')");
        for (const auto& e : entries) {
            entry.bind(1, id).bind(2, e.item).bind(3, e.path).bind(4, e.bytes).bind(5, e.mtime);
            entry.run();
            entryIds << db_.lastInsertId();
        }
        auto run = db_.prepare("UPDATE file_operation SET state = 'running' WHERE id = ?");
        run.bind(1, id);
        run.run();
        tx.commit();
    }
    int ok = 0;
    for (qsizetype i = 0; i < entries.size(); ++i) {
        const LooseTrashEntry& e = entries[i];
        qint64 size = 0, mtime = 0;
        QString result, message;
        if (!stat(e.path, &size, &mtime) || size != e.bytes || mtime != e.mtime) {
            result = QStringLiteral("skipped_changed");
            message = QStringLiteral("Le fichier a changé ou a disparu depuis la demande.");
        } else {
            result = recycle(e.path, &message);
        }
        if (result == u"trashed")
            ++ok;
        setEntry(entryIds[i], result, message);
    }
    {
        std::lock_guard lock(db_.mutex());
        auto q = db_.prepare("UPDATE file_operation SET state = ?, finished_at = ? WHERE id = ?");
        q.bind(1, ok == entries.size() ? QStringLiteral("done") : ok > 0 ? QStringLiteral("partial") : QStringLiteral("failed"))
            .bind(2, nowMs())
            .bind(3, id);
        q.run();
    }
    return id;
}

OperationId FileTrash::restore(OperationId original, int* restoredOut)
{
    struct Row {
        qint64 id;
        QString path;
        qint64 bytes;
    };
    QList<Row> rows;
    qint64 since = 0;
    QString source;
    {
        std::lock_guard lock(db_.mutex());
        auto o = db_.prepare("SELECT created_at, source FROM file_operation WHERE id = ? AND kind = 'trash'");
        o.bind(1, original);
        if (!o.step())
            return 0;
        since = o.int64(0);
        source = o.text(1);
        auto e = db_.prepare("SELECT id, original_path, size_bytes FROM file_operation_entry"
                             " WHERE operation_id = ? AND result = 'trashed' ORDER BY id");
        e.bind(1, original);
        while (e.step())
            rows << Row{e.int64(0), e.text(1), e.int64(2)};
    }
    if (rows.isEmpty())
        return 0;
    OperationId id = 0;
    {
        std::lock_guard lock(db_.mutex());
        qint64 bytes = 0;
        for (const Row& r : rows)
            bytes += r.bytes;
        auto q = db_.prepare("INSERT INTO file_operation (kind, undoes_id, created_at, state, file_count, bytes, source)"
                             " VALUES ('restore', ?, ?, 'running', ?, ?, ?)");
        q.bind(1, original).bind(2, nowMs()).bind(3, int(rows.size())).bind(4, bytes).bind(5, source);
        q.run();
        id = db_.lastInsertId();
    }
    int restored = 0;
    for (const Row& r : rows) {
        QString why, result;
        switch (platform::restoreFromRecycleBin(r.path, since, &why)) {
        case platform::RestoreResult::Restored:
            result = QStringLiteral("restored");
            ++restored;
            break;
        case platform::RestoreResult::NotInRecycleBin: result = QStringLiteral("restore_missing"); break;
        case platform::RestoreResult::Conflict: result = QStringLiteral("restore_conflict"); break;
        case platform::RestoreResult::Failed: result = QStringLiteral("failed"); break;
        }
        setEntry(r.id, result, why);
    }
    {
        std::lock_guard lock(db_.mutex());
        auto u = db_.prepare("UPDATE file_operation SET state = ? WHERE id = ?");
        u.bind(1, restored == rows.size() ? QStringLiteral("undone") : QStringLiteral("partially_undone")).bind(2, original);
        u.run();
        auto f = db_.prepare("UPDATE file_operation SET state = ?, finished_at = ? WHERE id = ?");
        f.bind(1, restored == rows.size() ? QStringLiteral("done") : restored > 0 ? QStringLiteral("partial") : QStringLiteral("failed"))
            .bind(2, nowMs())
            .bind(3, id);
        f.run();
    }
    if (restoredOut)
        *restoredOut = restored;
    return id;
}

} // namespace picazoo

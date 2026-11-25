#include "fileops/TrashService.h"

#include "duplicates/DuplicateEngine.h"
#include "duplicates/DuplicateStore.h"
#include "index/Database.h"
#include "library/Library.h"
#include "platform/win/DirectoryWalker.h"
#include "platform/win/FileIdentity.h"
#include "platform/win/Paths.h"
#include "platform/win/RecycleBin.h"
#include "platform/win/ThrottledReader.h"

#include <windows.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QHash>
#include <QMetaObject>
#include <QThread>

namespace picazoo {

namespace {

qint64 nowMs() { return QDateTime::currentMSecsSinceEpoch(); }

struct IndexedFile {
    bool found = false;
    QString path;
    qint64 size = 0;
    qint64 mtime = 0;
    std::optional<qint64> volume;
    QByteArray fileId;
    QByteArray contentHash;
};

IndexedFile indexedFile(Database& db, ItemId id)
{
    std::lock_guard lock(db.mutex());
    IndexedFile f;
    auto q = db.prepare("SELECT fo.path, i.file_name, i.size_bytes, i.mtime, i.volume_serial, i.file_id, fp.content_hash"
                        " FROM item i JOIN folder fo ON fo.id = i.folder_id"
                        " LEFT JOIN item_fingerprint fp ON fp.item_id = i.id AND fp.version = i.version"
                        " AND fp.content_state = 'done' WHERE i.id = ?");
    q.bind(1, id);
    if (!q.step())
        return f;
    f.found = true;
    f.path = platform::joinPath(q.text(0), q.text(1));
    f.size = q.int64(2);
    f.mtime = q.int64(3);
    f.volume = q.optInt64(4);
    if (!q.isNull(5))
        f.fileId = q.blob(5);
    if (!q.isNull(6))
        f.contentHash = q.blob(6);
    return f;
}

// Le fichier est-il toujours celui que l'analyse a vu (FR-019) ?
bool unchangedOnDisk(const IndexedFile& f)
{
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(platform::toWin32(f.path).c_str(), GetFileExInfoStandard, &data))
        return false;
    const qint64 size = (qint64(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
    const qint64 mtime = platform::fileTimeToEpochMs(data.ftLastWriteTime.dwLowDateTime, data.ftLastWriteTime.dwHighDateTime);
    if (size != f.size || mtime != f.mtime)
        return false;
    if (!f.fileId.isEmpty()) {
        const auto id = platform::fileIdentity(f.path);
        if (!id || id->id != f.fileId || (f.volume && qint64(id->volumeSerial) != *f.volume))
            return false;
    }
    return true;
}

bool exists(const QString& path) { return GetFileAttributesW(platform::toWin32(path).c_str()) != INVALID_FILE_ATTRIBUTES; }

} // namespace

QString toDb(OperationState s)
{
    switch (s) {
    case OperationState::Planned: return QStringLiteral("planned");
    case OperationState::Running: return QStringLiteral("running");
    case OperationState::Done: return QStringLiteral("done");
    case OperationState::Partial: return QStringLiteral("partial");
    case OperationState::Failed: return QStringLiteral("failed");
    case OperationState::Undone: return QStringLiteral("undone");
    case OperationState::PartiallyUndone: return QStringLiteral("partially_undone");
    }
    return QStringLiteral("failed");
}

OperationState operationStateFromDb(const QString& s)
{
    if (s == u"planned")
        return OperationState::Planned;
    if (s == u"running")
        return OperationState::Running;
    if (s == u"done")
        return OperationState::Done;
    if (s == u"partial")
        return OperationState::Partial;
    if (s == u"undone")
        return OperationState::Undone;
    if (s == u"partially_undone")
        return OperationState::PartiallyUndone;
    return OperationState::Failed;
}

QString resultLabel(const QString& r)
{
    static const QHash<QString, QString> labels{
        {QStringLiteral("pending"), QStringLiteral("En attente")},
        {QStringLiteral("trashed"), QStringLiteral("Mis à la corbeille")},
        {QStringLiteral("skipped_changed"),
         QStringLiteral("Ce fichier a changé depuis l'analyse. Il n'a pas été touché ; son groupe va être recalculé.")},
        {QStringLiteral("skipped_no_recycle_bin"),
         QStringLiteral("Ce disque n'a pas de corbeille : Picazoo ne supprime pas ce fichier.")},
        {QStringLiteral("skipped_locked"),
         QStringLiteral("Ce fichier est utilisé par un autre programme ou protégé en écriture. Il n'a pas été touché.")},
        {QStringLiteral("skipped_unavailable"), QStringLiteral("Disque ou dossier indisponible. Il n'a pas été touché.")},
        {QStringLiteral("failed"), QStringLiteral("Échec : le fichier n'a pas été touché.")},
        {QStringLiteral("restored"), QStringLiteral("Restauré à son emplacement d'origine")},
        {QStringLiteral("restore_missing"),
         QStringLiteral("Ce fichier n'est plus dans la corbeille : il ne peut pas être restauré.")},
        {QStringLiteral("restore_conflict"),
         QStringLiteral("Un fichier du même nom existe déjà à cet emplacement : celui-ci n'a pas été restauré.")},
    };
    return labels.value(r, r);
}

TrashService::TrashService(Library& library, DuplicateEngine& engine, QObject* parent)
    : QObject(parent), lib_(library), engine_(engine)
{
    db_ = Database::open(QDir(lib_.dataDir()).filePath(QStringLiteral("library.db")));
    recoverInterrupted();
}

TrashService::~TrashService() { join(); }

void TrashService::join()
{
    if (worker_.joinable())
        worker_.join();
}

// --------------------------------------------------------------------- Plan

TrashPlan TrashService::plan(const QList<GroupId>& groups) const
{
    TrashPlan p;
    DuplicateStore& store = engine_.store();
    QHash<QString, bool> volumes; // racine du volume -> corbeille disponible
    for (GroupId gid : groups) {
        const auto g = store.group(gid);
        if (!g)
            continue;
        QList<DuplicateCopy> kept, out;
        for (const DuplicateCopy& c : g->copies)
            (c.keep ? kept : out) << c;
        if (out.isEmpty())
            continue;
        if (kept.isEmpty()) {
            p.refused << gid;
            continue;
        }
        // Copie gardée de référence (006) : la copie suggérée si elle est gardée, sinon le centre
        // s'il est gardé, sinon la première gardée.
        const DuplicateCopy keeper = [&] {
            for (const DuplicateCopy& c : kept)
                if (c.isSuggested)
                    return c;
            for (const DuplicateCopy& c : kept)
                if (c.isCenter)
                    return c;
            return kept.first();
        }();
        const bool examined =
            std::any_of(g->copies.begin(), g->copies.end(), [](const DuplicateCopy& c) { return c.userChoice; });
        const auto keeperItem = lib_.item(keeper.item);
        if (!keeperItem || !exists(keeperItem->path())) {
            p.refused << gid; // la copie gardée doit être présente
            continue;
        }
        for (const DuplicateCopy& c : out) {
            const auto item = lib_.item(c.item);
            if (!item)
                continue;
            const QString path = item->path();
            if (!exists(path)) {
                p.blocked << BlockedEntry{c.item, gid, path, BlockReason::Unavailable};
                continue;
            }
            const QString vol = path.left(3).toUpper();
            auto v = volumes.find(vol);
            if (v == volumes.end())
                v = volumes.insert(vol, platform::volumeHasRecycleBin(path));
            if (!*v) {
                p.blocked << BlockedEntry{c.item, gid, path, BlockReason::NoRecycleBin};
                continue;
            }
            TrashPlanEntry e;
            e.item = c.item;
            e.group = gid;
            e.path = path;
            e.bytes = item->sizeBytes;
            e.keeper = keeper.item;
            e.keeperPath = keeperItem->path();
            e.identical = c.level == DupLevel::Identical && keeper.level == DupLevel::Identical;
            e.level = g->level;
            e.examined = examined;
            p.toTrash << e;
            p.bytes += e.bytes;
        }
    }
    return p;
}

TrashPlan TrashService::planAllIdentical() const
{
    return plan(engine_.store().groups(LevelFilter::Identical));
}

TrashPlan TrashService::planItems(const QList<ItemId>& items) const
{
    TrashPlan p;
    QHash<QString, bool> volumes;
    for (ItemId id : items) {
        const auto item = lib_.item(id);
        if (!item)
            continue;
        const QString path = item->path();
        if (!exists(path)) {
            p.blocked << BlockedEntry{id, 0, path, BlockReason::Unavailable};
            continue;
        }
        const QString vol = path.left(3).toUpper();
        auto v = volumes.find(vol);
        if (v == volumes.end())
            v = volumes.insert(vol, platform::volumeHasRecycleBin(path));
        if (!*v) {
            p.blocked << BlockedEntry{id, 0, path, BlockReason::NoRecycleBin};
            continue;
        }
        TrashPlanEntry e;
        e.item = id;
        e.path = path;
        e.bytes = item->sizeBytes;
        p.toTrash << e;
        p.bytes += e.bytes;
    }
    return p;
}

TrashPlan TrashPlan::filtered(const std::function<bool(const TrashPlanEntry&)>& keep) const
{
    TrashPlan p;
    p.blocked = blocked;
    p.refused = refused;
    for (const TrashPlanEntry& e : toTrash)
        if (keep(e)) {
            p.toTrash << e;
            p.bytes += e.bytes;
        }
    return p;
}

void TrashService::requestStop()
{
    if (busy_.load())
        stopRequested_ = true;
}

// ---------------------------------------------------------------- Exécution

OperationId TrashService::execute(const TrashPlan& confirmed, OperationSource source)
{
    if (busy_.load() || confirmed.toTrash.isEmpty())
        return 0;
    // FR-017, revérifié ici indépendamment de l'interface : chaque groupe garde une copie.
    // (Les entrées sans groupe viennent de la bibliothèque ou de la visionneuse, 007.)
    QHash<GroupId, int> trashedPerGroup;
    for (const auto& e : confirmed.toTrash)
        if (e.group != 0)
            ++trashedPerGroup[e.group];
    for (auto it = trashedPerGroup.constBegin(); it != trashedPerGroup.constEnd(); ++it) {
        const auto g = engine_.store().group(it.key());
        if (!g || it.value() >= g->copies.size())
            return 0;
    }
    join();
    OperationId id = 0;
    TrashPlan plan = confirmed;
    {
        // Journal d'abord : l'opération et chaque fichier sont écrits avant d'agir.
        std::lock_guard lock(dbMutex_);
        std::lock_guard dbLock(db_->mutex());
        Transaction tx(*db_);
        auto op = db_->prepare("INSERT INTO file_operation (kind, created_at, state, file_count, bytes, source)"
                               " VALUES ('trash', ?, 'planned', ?, ?, ?)");
        op.bind(1, nowMs()).bind(2, int(plan.toTrash.size())).bind(3, plan.bytes).bind(4, toDb(source));
        op.run();
        id = db_->lastInsertId();
        auto entry = db_->prepare("INSERT INTO file_operation_entry (operation_id, item_id, original_path, size_bytes,"
                                  " mtime, content_hash, keeper_path, result) VALUES (?, ?, ?, ?, ?, ?, ?, 'pending')");
        for (const auto& e : plan.toTrash) {
            const IndexedFile f = indexedFile(*db_, e.item);
            entry.bind(1, id).bind(2, e.item).bind(3, e.path).bind(4, e.bytes).bind(5, f.mtime);
            if (f.contentHash.isEmpty())
                entry.bindNull(6);
            else
                entry.bind(6, f.contentHash);
            if (e.keeperPath.isEmpty())
                entry.bindNull(7);
            else
                entry.bind(7, e.keeperPath);
            entry.run();
        }
        tx.commit();
    }
    stopRequested_ = false;
    busy_ = true;
    worker_ = std::thread([this, id, plan = std::move(plan)] { runTrash(id, plan); });
    return id;
}

void TrashService::setEntry(qint64 entryId, const QString& result, const QString& message)
{
    std::lock_guard lock(dbMutex_);
    std::lock_guard dbLock(db_->mutex());
    auto q = db_->prepare("UPDATE file_operation_entry SET result = ?, message = ? WHERE id = ?");
    q.bind(1, result);
    if (message.isEmpty())
        q.bindNull(2);
    else
        q.bind(2, message);
    q.bind(3, entryId);
    q.run();
}

void TrashService::finish(OperationId id, OperationState state)
{
    {
        std::lock_guard lock(dbMutex_);
        std::lock_guard dbLock(db_->mutex());
        auto q = db_->prepare("UPDATE file_operation SET state = ?, finished_at = ? WHERE id = ?");
        q.bind(1, toDb(state)).bind(2, nowMs()).bind(3, id);
        q.run();
    }
    // Le fil est rejoint par la prochaine opération, waitForIdle() ou le destructeur.
    QMetaObject::invokeMethod(this, [this, id, state] { emit operationFinished(id, int(state)); },
                              Qt::QueuedConnection);
}

void TrashService::runTrash(OperationId id, TrashPlan plan)
{
    {
        std::lock_guard lock(dbMutex_);
        std::lock_guard dbLock(db_->mutex());
        auto q = db_->prepare("UPDATE file_operation SET state = 'running' WHERE id = ?");
        q.bind(1, id);
        q.run();
    }
    QHash<ItemId, qint64> entryIds;
    {
        std::lock_guard lock(dbMutex_);
        std::lock_guard dbLock(db_->mutex());
        auto q = db_->prepare("SELECT id, item_id FROM file_operation_entry WHERE operation_id = ?");
        q.bind(1, id);
        while (q.step())
            entryIds.insert(q.int64(1), q.int64(0));
    }
    QList<ItemId> trashed;
    int done = 0, ok = 0;
    bool changed = false;
    bool stopped = false;
    for (const TrashPlanEntry& e : plan.toTrash) {
        // 006 (research R8) : arrêt entre deux fichiers ; les suivants restent « pending ».
        if (stopRequested_.load()) {
            stopped = true;
            break;
        }
        const qint64 entryId = entryIds.value(e.item);
        QString result, message;
        const IndexedFile victim = indexedFile(*db_, e.item);
        bool verified = victim.found && platform::samePath(victim.path, e.path) && unchangedOnDisk(victim);
        if (e.keeper != 0) {
            // Doublons : la copie gardée doit elle aussi être intacte.
            const IndexedFile keeper = indexedFile(*db_, e.keeper);
            verified = verified && keeper.found && unchangedOnDisk(keeper);
        }
        if (verified && e.identical && e.keeper != 0) {
            const IndexedFile keeper = indexedFile(*db_, e.keeper);
            // « Identique » : on refait l'empreinte des deux fichiers juste avant d'agir.
            const auto hv = platform::hashFileContent(victim.path);
            const auto hk = platform::hashFileContent(keeper.path);
            verified = hv.status == platform::ReadStatus::Ok && hk.status == platform::ReadStatus::Ok &&
                       hv.hash == hk.hash;
        }
        if (!verified) {
            result = QStringLiteral("skipped_changed");
            changed = true;
        } else {
            QString why;
            result = FileTrash::recycle(e.path, &why);
            if (result == u"trashed") {
                trashed << e.item;
                ++ok;
            } else if (result == u"skipped_changed") {
                changed = true;
            }
            message = why;
        }
        setEntry(entryId, result, message);
        ++done;
        QMetaObject::invokeMethod(this, [this, id, done, total = int(plan.toTrash.size())] {
            emit operationProgress(id, done, total);
        }, Qt::QueuedConnection);
    }
    // FR-023 : la bibliothèque suit immédiatement. La recherche des doublons repart une fois
    // les éléments retirés de l'index, pour que groupes et comptes reflètent ce qui reste.
    lib_.forgetItems(trashed, engine_.forgetWhenRemoved(trashed));
    if (changed)
        engine_.rescanAll();
    engine_.requestRegroup();
    const OperationState state = ok == int(plan.toTrash.size()) ? OperationState::Done
                                 : ok > 0                       ? OperationState::Partial
                                                                : OperationState::Failed;
    if (stopped) {
        std::lock_guard lock(dbMutex_);
        std::lock_guard dbLock(db_->mutex());
        auto q = db_->prepare("UPDATE file_operation SET stopped = 1 WHERE id = ?");
        q.bind(1, id);
        q.run();
    }
    stopRequested_ = false;
    busy_ = false;
    finish(id, state);
}

// ---------------------------------------------------------------- Annulation

OperationId TrashService::undo(OperationId original)
{
    if (busy_.load())
        return 0;
    const auto op = operation(original);
    if (!op || op->kind != u"trash")
        return 0;
    int count = 0;
    qint64 bytes = 0;
    for (const auto& e : op->entries)
        if (e.result == u"trashed") {
            ++count;
            bytes += e.sizeBytes;
        }
    if (count == 0)
        return 0;
    join();
    OperationId id = 0;
    {
        std::lock_guard lock(dbMutex_);
        std::lock_guard dbLock(db_->mutex());
        auto q = db_->prepare("INSERT INTO file_operation (kind, undoes_id, created_at, state, file_count, bytes, source)"
                              " VALUES ('restore', ?, ?, 'planned', ?, ?, ?)");
        q.bind(1, original).bind(2, nowMs()).bind(3, count).bind(4, bytes).bind(5, toDb(op->source));
        q.run();
        id = db_->lastInsertId();
    }
    busy_ = true;
    worker_ = std::thread([this, id, original, since = op->createdAt] { runRestore(id, original, since); });
    return id;
}

void TrashService::runRestore(OperationId id, OperationId original, qint64 since)
{
    const auto op = operation(original);
    int restored = 0, attempted = 0;
    if (op) {
        for (const OperationEntry& e : op->entries) {
            if (e.result != u"trashed")
                continue;
            ++attempted;
            QString why;
            QString result;
            switch (platform::restoreFromRecycleBin(e.path, since, &why)) {
            case platform::RestoreResult::Restored:
                result = QStringLiteral("restored");
                ++restored;
                break;
            case platform::RestoreResult::NotInRecycleBin: result = QStringLiteral("restore_missing"); break;
            case platform::RestoreResult::Conflict: result = QStringLiteral("restore_conflict"); break;
            case platform::RestoreResult::Failed: result = QStringLiteral("failed"); break;
            }
            setEntry(e.id, result, why);
            QMetaObject::invokeMethod(this, [this, id, attempted, total = op->fileCount] {
                emit operationProgress(id, attempted, total);
            }, Qt::QueuedConnection);
        }
    }
    {
        std::lock_guard lock(dbMutex_);
        std::lock_guard dbLock(db_->mutex());
        auto q = db_->prepare("UPDATE file_operation SET state = ? WHERE id = ?");
        q.bind(1, toDb(restored == attempted ? OperationState::Undone : OperationState::PartiallyUndone)).bind(2, original);
        q.run();
    }
    if (restored > 0)
        lib_.startScan(); // les fichiers restaurés réapparaissent dans la bibliothèque
    busy_ = false;
    finish(id, restored == attempted ? OperationState::Done
                                     : restored > 0 ? OperationState::Partial : OperationState::Failed);
}

// -------------------------------------------------------------------- Journal

QList<Operation> TrashService::journal() const
{
    std::lock_guard lock(dbMutex_);
    std::lock_guard dbLock(db_->mutex());
    QList<Operation> out;
    auto q = db_->prepare("SELECT id, kind, undoes_id, created_at, finished_at, state, file_count, bytes, stopped,"
                          " source FROM file_operation ORDER BY id DESC");
    while (q.step()) {
        Operation o;
        o.id = q.int64(0);
        o.kind = q.text(1);
        o.undoes = q.optInt64(2);
        o.createdAt = q.int64(3);
        o.finishedAt = q.optInt64(4);
        o.state = operationStateFromDb(q.text(5));
        o.fileCount = q.int32(6);
        o.bytes = q.int64(7);
        o.stopped = q.int32(8) != 0;
        o.source = operationSourceFromDb(q.text(9));
        out << o;
    }
    return out;
}

std::optional<Operation> TrashService::operation(OperationId id) const
{
    std::lock_guard lock(dbMutex_);
    std::lock_guard dbLock(db_->mutex());
    auto q = db_->prepare("SELECT id, kind, undoes_id, created_at, finished_at, state, file_count, bytes, stopped,"
                          " source FROM file_operation WHERE id = ?");
    q.bind(1, id);
    if (!q.step())
        return std::nullopt;
    Operation o;
    o.id = q.int64(0);
    o.kind = q.text(1);
    o.undoes = q.optInt64(2);
    o.createdAt = q.int64(3);
    o.finishedAt = q.optInt64(4);
    o.state = operationStateFromDb(q.text(5));
    o.fileCount = q.int32(6);
    o.bytes = q.int64(7);
    o.stopped = q.int32(8) != 0;
    o.source = operationSourceFromDb(q.text(9));
    // Pour une restauration, le détail est celui de l'opération annulée.
    auto e = db_->prepare("SELECT id, item_id, original_path, size_bytes, keeper_path, result, message"
                          " FROM file_operation_entry WHERE operation_id = ? ORDER BY id");
    e.bind(1, o.undoes.value_or(o.id));
    while (e.step()) {
        OperationEntry x;
        x.id = e.int64(0);
        x.item = e.optInt64(1);
        x.path = e.text(2);
        x.sizeBytes = e.int64(3);
        x.keeperPath = e.isNull(4) ? QString() : e.text(4);
        x.result = e.text(5);
        x.message = e.isNull(6) ? QString() : e.text(6);
        o.entries << x;
    }
    return o;
}

void TrashService::recoverInterrupted()
{
    // Opération interrompue (arrêt brutal) : on constate sur le disque ce qui a été fait.
    QList<OperationId> open;
    {
        std::lock_guard dbLock(db_->mutex());
        // 007 : une opération de moins d'une minute peut être en cours dans une visionneuse autonome
        // (même base, autre processus) : on ne la « répare » pas.
        auto q = db_->prepare("SELECT id FROM file_operation WHERE state IN ('planned', 'running') AND kind = 'trash'"
                              " AND created_at < ?");
        q.bind(1, nowMs() - 60'000);
        while (q.step())
            open << q.int64(0);
    }
    for (OperationId id : open) {
        const auto op = operation(id);
        if (!op)
            continue;
        for (const OperationEntry& e : op->entries) {
            if (e.result != u"pending")
                continue;
            if (exists(e.path)) {
                setEntry(e.id, QStringLiteral("failed"), QStringLiteral("Opération interrompue : fichier non touché."));
                continue;
            }
            bool inBin = false;
            for (const auto& r : platform::recycledEntries(e.path))
                inBin |= platform::samePath(r.originalPath, e.path) && r.deletedAtMs >= op->createdAt - 5000;
            setEntry(e.id, inBin ? QStringLiteral("trashed") : QStringLiteral("failed"),
                     inBin ? QString() : QStringLiteral("Opération interrompue."));
        }
        std::lock_guard dbLock(db_->mutex());
        auto u = db_->prepare("UPDATE file_operation SET state = 'partial', finished_at = ? WHERE id = ?");
        u.bind(1, nowMs()).bind(2, id);
        u.run();
    }
}

bool TrashService::waitForIdle(int timeoutMs)
{
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        if (!busy_.load()) {
            QCoreApplication::processEvents();
            join();
            return true;
        }
        QThread::msleep(20);
    }
    return false;
}

} // namespace picazoo

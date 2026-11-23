#include "controllers/TrashController.h"

#include "controllers/AppController.h"
#include "duplicates/DuplicateEngine.h"
#include "duplicates/DuplicateStore.h"
#include "library/Library.h"
#include "platform/win/Paths.h"

#include <QDateTime>
#include <QLocale>

using namespace picazoo;

namespace {

QString files(int n)
{
    return QLocale(QLocale::French).toString(n) + (n > 1 ? QStringLiteral(" fichiers") : QStringLiteral(" fichier"));
}

QString stateLabel(OperationState s)
{
    switch (s) {
    case OperationState::Planned:
    case OperationState::Running: return QStringLiteral("En cours");
    case OperationState::Done: return QStringLiteral("Fait");
    case OperationState::Partial: return QStringLiteral("Fait en partie");
    case OperationState::Failed: return QStringLiteral("Échec");
    case OperationState::Undone: return QStringLiteral("Annulé");
    case OperationState::PartiallyUndone: return QStringLiteral("Annulé en partie");
    }
    return {};
}

QString blockLabel(BlockReason r)
{
    switch (r) {
    case BlockReason::NoRecycleBin: return QStringLiteral("Ce disque n'a pas de corbeille");
    case BlockReason::Unavailable: return QStringLiteral("Disque ou fichier indisponible");
    case BlockReason::Locked: return QStringLiteral("Fichier utilisé ou protégé en écriture");
    }
    return {};
}

} // namespace

TrashController::TrashController(Library& library, DuplicateEngine& engine, TrashService& service, QObject* parent)
    : QObject(parent), lib_(library), engine_(engine), service_(service)
{
    connect(&service_, &TrashService::operationProgress, this, [this](qint64, int done, int total) {
        progressText_ = QStringLiteral("%1 / %2").arg(done).arg(total);
        emit progressChanged();
    });
    connect(&service_, &TrashService::operationFinished, this, [this](qint64 id, int state) {
        progressText_.clear();
        emit progressChanged();
        emit busyChanged();
        emit journalChanged();
        const auto op = service_.operation(id);
        if (!op)
            return;
        int ok = 0, total = 0;
        for (const auto& e : op->entries) {
            if (op->kind == u"trash") {
                ++total;
                ok += e.result == u"trashed" || e.result == u"restored" ? 1 : 0;
            } else if (e.result == u"restored" || e.result.startsWith(u"restore") || e.result == u"failed") {
                ++total;
                ok += e.result == u"restored" ? 1 : 0;
            }
        }
        QString msg;
        if (op->kind == u"trash")
            msg = ok == total ? files(ok) + QStringLiteral(" mis à la corbeille.")
                              : QStringLiteral("%1 sur %2 mis à la corbeille ; les autres n'ont pas été touchés "
                                               "(voir le journal des opérations).").arg(ok).arg(total);
        else
            msg = OperationState(state) == OperationState::Done
                      ? files(ok) + QStringLiteral(" restaurés à leur emplacement d'origine.")
                      : QStringLiteral("%1 sur %2 restaurés (voir le journal des opérations).").arg(ok).arg(total);
        emit finished(msg);
    });
}

QVariantMap TrashController::describe(const TrashPlan& plan) const
{
    QVariantMap m;
    QVariantList groups;
    QHash<GroupId, int> rowOf;
    for (const TrashPlanEntry& e : plan.toTrash) {
        if (!rowOf.contains(e.group)) {
            rowOf.insert(e.group, int(groups.size()));
            QVariantMap g;
            g.insert(QStringLiteral("keeperName"), platform::lastSegment(e.keeperPath));
            g.insert(QStringLiteral("keeperPath"), e.keeperPath);
            g.insert(QStringLiteral("files"), QVariantList());
            groups << g;
        }
        QVariantMap g = groups[rowOf.value(e.group)].toMap();
        QVariantList list = g.value(QStringLiteral("files")).toList();
        QVariantMap f;
        f.insert(QStringLiteral("name"), platform::lastSegment(e.path));
        f.insert(QStringLiteral("folder"), platform::parentPath(e.path));
        f.insert(QStringLiteral("size"), AppController::formatBytes(e.bytes));
        list << f;
        g.insert(QStringLiteral("files"), list);
        groups[rowOf.value(e.group)] = g;
    }
    QVariantList blocked;
    for (const BlockedEntry& b : plan.blocked) {
        QVariantMap x;
        x.insert(QStringLiteral("name"), platform::lastSegment(b.path));
        x.insert(QStringLiteral("path"), b.path);
        x.insert(QStringLiteral("item"), b.item);
        x.insert(QStringLiteral("reason"), blockLabel(b.reason));
        blocked << x;
    }
    m.insert(QStringLiteral("groups"), groups);
    m.insert(QStringLiteral("blocked"), blocked);
    m.insert(QStringLiteral("refusedCount"), int(plan.refused.size()));
    m.insert(QStringLiteral("count"), int(plan.toTrash.size()));
    m.insert(QStringLiteral("countText"), files(int(plan.toTrash.size())));
    m.insert(QStringLiteral("bytesText"), AppController::formatBytes(plan.bytes));
    return m;
}

QVariantMap TrashController::prepare(const QVariantList& groupIds)
{
    QList<GroupId> ids;
    for (const QVariant& v : groupIds)
        ids << v.toLongLong();
    if (ids.isEmpty())
        ids = engine_.store().groups();
    lastPlan_ = service_.plan(ids);
    return describe(*lastPlan_);
}

QVariantMap TrashController::prepareAllIdentical()
{
    lastPlan_ = service_.planAllIdentical();
    return describe(*lastPlan_);
}

bool TrashController::confirm()
{
    if (!lastPlan_)
        return false;
    const TrashPlan plan = *lastPlan_;
    lastPlan_.reset();
    const bool ok = service_.execute(plan) != 0;
    emit busyChanged();
    emit journalChanged();
    return ok;
}

QVariantList TrashController::journal() const
{
    QVariantList out;
    const QLocale fr(QLocale::French);
    for (const Operation& o : service_.journal()) {
        QVariantMap m;
        m.insert(QStringLiteral("id"), o.id);
        m.insert(QStringLiteral("kind"), o.kind);
        m.insert(QStringLiteral("date"), fr.toString(QDateTime::fromMSecsSinceEpoch(o.createdAt), QStringLiteral("d MMMM yyyy 'à' HH:mm")));
        m.insert(QStringLiteral("title"), o.kind == u"trash"
                                              ? files(o.fileCount) + QStringLiteral(" mis à la corbeille · ") + AppController::formatBytes(o.bytes)
                                              : QStringLiteral("Restauration de ") + files(o.fileCount));
        m.insert(QStringLiteral("state"), stateLabel(o.state));
        m.insert(QStringLiteral("undoable"), o.kind == u"trash" &&
                                                 (o.state == OperationState::Done || o.state == OperationState::Partial ||
                                                  o.state == OperationState::PartiallyUndone));
        out << m;
    }
    return out;
}

QVariantMap TrashController::operationDetails(qint64 operationId) const
{
    QVariantMap m;
    const auto op = service_.operation(operationId);
    if (!op)
        return m;
    QVariantList entries;
    for (const OperationEntry& e : op->entries) {
        QVariantMap x;
        x.insert(QStringLiteral("name"), platform::lastSegment(e.path));
        x.insert(QStringLiteral("path"), e.path);
        x.insert(QStringLiteral("size"), AppController::formatBytes(e.sizeBytes));
        x.insert(QStringLiteral("result"), e.result);
        x.insert(QStringLiteral("resultText"), resultLabel(e.result));
        x.insert(QStringLiteral("message"), e.message);
        entries << x;
    }
    m.insert(QStringLiteral("entries"), entries);
    m.insert(QStringLiteral("state"), stateLabel(op->state));
    return m;
}

bool TrashController::undo(qint64 operationId)
{
    const bool ok = service_.undo(operationId) != 0;
    emit busyChanged();
    emit journalChanged();
    return ok;
}

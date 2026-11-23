#include "controllers/TrashController.h"

#include "controllers/AppController.h"
#include "duplicates/DuplicateEngine.h"
#include "duplicates/DuplicateStore.h"
#include "library/Library.h"
#include "platform/win/Paths.h"

#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>
#include <QHash>
#include <QLocale>

#include <algorithm>

using namespace picazoo;

namespace {

QString files(int n)
{
    return QLocale(QLocale::French).toString(n) + (n > 1 ? QStringLiteral(" fichiers") : QStringLiteral(" fichier"));
}

QString groupsText(int n)
{
    return QLocale(QLocale::French).toString(n) + (n > 1 ? QStringLiteral(" groupes") : QStringLiteral(" groupe"));
}

QString stateLabel(const Operation& o)
{
    if (o.stopped && o.state != OperationState::Undone && o.state != OperationState::PartiallyUndone)
        return QStringLiteral("Arrêtée");
    switch (o.state) {
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

// Niveau d'une entrée dans le récapitulatif (FR-018) : les « Très ressemblante » dont aucun
// choix n'a été fait à la main sont à part.
QString levelKey(const TrashPlanEntry& e)
{
    switch (e.level) {
    case DupLevel::Identical: return QStringLiteral("identical");
    case DupLevel::SameImage: return QStringLiteral("same_image");
    case DupLevel::Similar: return e.examined ? QStringLiteral("similar") : QStringLiteral("similar_unchecked");
    }
    return QStringLiteral("identical");
}

constexpr int kFilesPerFolder = 200;

} // namespace

TrashController::TrashController(Library& library, DuplicateEngine& engine, TrashService& service, QObject* parent)
    : QObject(parent), lib_(library), engine_(engine), service_(service)
{
    connect(&service_, &TrashService::operationProgress, this, [this](qint64, int done, int total) {
        const QLocale fr(QLocale::French);
        progressText_ = fr.toString(done) + QStringLiteral(" / ") + fr.toString(total);
        progressValue_ = total > 0 ? double(done) / total : 0;
        emit progressChanged();
    });
    connect(&service_, &TrashService::operationFinished, this, [this](qint64 id, int state) {
        progressText_.clear();
        progressValue_ = 0;
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
        if (op->kind == u"trash" && op->stopped)
            msg = QStringLiteral("Arrêtée : ") + files(ok) +
                  QStringLiteral(" mis à la corbeille. Les autres n'ont pas été touchés (voir le journal des opérations).");
        else if (op->kind == u"trash")
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

TrashController::~TrashController()
{
    ++generation_;
    joinPlanner();
}

void TrashController::joinPlanner()
{
    if (planner_.joinable())
        planner_.join();
}

// ------------------------------------------------------------ Récapitulatif

void TrashController::prepare(const QVariantList& groupIds)
{
    QList<GroupId> ids;
    for (const QVariant& v : groupIds)
        if (v.toLongLong() > 0)
            ids << v.toLongLong();
    joinPlanner();
    const int gen = ++generation_;
    lastPlan_.reset();
    folders_.clear();
    described_.clear();
    preparing_ = true;
    emit planChanged();
    // research R7 : le plan vérifie les fichiers et la corbeille des volumes, hors du fil de l'interface.
    planner_ = std::thread([this, ids, gen] {
        QElapsedTimer timer;
        timer.start();
        TrashPlan p = service_.plan(ids);
        const qint64 ms = timer.elapsed();
        QMetaObject::invokeMethod(this, [this, p = std::move(p), gen, ms, n = ids.size()] {
            if (gen != generation_.load())
                return; // un récapitulatif plus récent a été demandé
            if (n > 100)
                qInfo() << "Doublons : récapitulatif de" << n << "groupes préparé en" << ms << "ms";
            lastPlan_ = p;
            excluded_ = {QStringLiteral("similar_unchecked")};
            preparing_ = false;
            described_ = describe();
            emit planChanged();
        }, Qt::QueuedConnection);
    });
}

TrashPlan TrashController::included() const
{
    if (!lastPlan_)
        return {};
    return lastPlan_->filtered([this](const TrashPlanEntry& e) { return !excluded_.contains(levelKey(e)); });
}

QVariantMap TrashController::describe() const
{
    QVariantMap m;
    if (!lastPlan_)
        return m;
    const TrashPlan inc = included();

    // Par niveau.
    struct Level {
        const char* key;
        const char* label;
        const char* badge;
    };
    static const Level levels[] = {
        {"identical", "Identique", "identical"},
        {"same_image", "Même image", "same_image"},
        {"similar", "Très ressemblante (vérifiées)", "similar"},
        {"similar_unchecked", "Très ressemblante non vérifiées", "similar"},
    };
    QVariantList levelList;
    for (const Level& l : levels) {
        const QString key = QString::fromLatin1(l.key);
        QSet<GroupId> groups;
        int n = 0;
        qint64 bytes = 0;
        for (const TrashPlanEntry& e : lastPlan_->toTrash)
            if (levelKey(e) == key) {
                groups.insert(e.group);
                ++n;
                bytes += e.bytes;
            }
        if (n == 0)
            continue;
        const bool on = !excluded_.contains(key);
        QString note = on ? QStringLiteral("Inclus") : QStringLiteral("Non inclus");
        if (key == u"similar_unchecked")
            note = on ? QStringLiteral("Inclus : vérifiez ces groupes") : QStringLiteral("Non vérifiées : non incluses");
        levelList << QVariantMap{{QStringLiteral("key"), key},
                                 {QStringLiteral("label"), QString::fromUtf8(l.label)},
                                 {QStringLiteral("badge"), QString::fromLatin1(l.badge)},
                                 {QStringLiteral("stats"), groupsText(int(groups.size())) + QStringLiteral(" · ") + files(n) +
                                                               QStringLiteral(" · ") + AppController::formatBytes(bytes)},
                                 {QStringLiteral("included"), on},
                                 {QStringLiteral("note"), note}};
    }

    // Par dossier d'origine (inclus seulement), du plus lourd au plus léger.
    QHash<QString, int> rowOf;
    auto& folders = folders_;
    folders.clear();
    for (const TrashPlanEntry& e : inc.toTrash) {
        const QString dir = platform::parentPath(e.path);
        const QString k = dir.toLower();
        auto it = rowOf.find(k);
        if (it == rowOf.end()) {
            it = rowOf.insert(k, int(folders.size()));
            folders << qMakePair(dir, QList<TrashPlanEntry>());
        }
        folders[*it].second << e;
    }
    auto bytesOf = [](const QList<TrashPlanEntry>& l) {
        qint64 b = 0;
        for (const auto& e : l)
            b += e.bytes;
        return b;
    };
    std::sort(folders.begin(), folders.end(), [&](const auto& a, const auto& b) { return bytesOf(a.second) > bytesOf(b.second); });
    QVariantList folderList;
    for (int i = 0; i < folders.size(); ++i) {
        const int n = int(folders[i].second.size());
        folderList << QVariantMap{{QStringLiteral("index"), i},
                                  {QStringLiteral("path"), folders[i].first},
                                  {QStringLiteral("stats"), files(n) + QStringLiteral(" · ") + AppController::formatBytes(bytesOf(folders[i].second))},
                                  {QStringLiteral("moreText"),
                                   n <= kFilesPerFolder           ? QString()
                                   : n - kFilesPerFolder == 1     ? QStringLiteral("… 1 autre fichier")
                                                                  : QStringLiteral("… %1 autres fichiers")
                                                                        .arg(QLocale(QLocale::French).toString(n - kFilesPerFolder))}};
    }

    QVariantList blocked;
    for (const BlockedEntry& b : inc.blocked) {
        blocked << QVariantMap{{QStringLiteral("name"), platform::lastSegment(b.path)},
                               {QStringLiteral("path"), b.path},
                               {QStringLiteral("item"), b.item},
                               {QStringLiteral("reason"), blockLabel(b.reason)}};
    }
    QSet<GroupId> groups;
    for (const TrashPlanEntry& e : inc.toTrash)
        groups.insert(e.group);
    m.insert(QStringLiteral("levels"), levelList);
    m.insert(QStringLiteral("folders"), folderList);
    m.insert(QStringLiteral("blocked"), blocked);
    m.insert(QStringLiteral("refusedCount"), int(inc.refused.size()));
    m.insert(QStringLiteral("count"), int(inc.toTrash.size()));
    m.insert(QStringLiteral("countText"), files(int(inc.toTrash.size())));
    m.insert(QStringLiteral("bytesText"), AppController::formatBytes(inc.bytes));
    m.insert(QStringLiteral("groupsText"), groupsText(int(groups.size())));
    return m;
}

QVariantList TrashController::folderFiles(int folderIndex) const
{
    QVariantList out;
    if (folderIndex < 0 || folderIndex >= folders_.size())
        return out;
    const auto& entries = folders_[folderIndex].second;
    for (int i = 0; i < entries.size() && i < kFilesPerFolder; ++i) {
        const TrashPlanEntry& e = entries[i];
        out << QVariantMap{{QStringLiteral("name"), platform::lastSegment(e.path)},
                           {QStringLiteral("size"), AppController::formatBytes(e.bytes)},
                           {QStringLiteral("keeper"), e.keeperPath}};
    }
    return out;
}

void TrashController::setLevelIncluded(const QString& key, bool on)
{
    if (on)
        excluded_.remove(key);
    else
        excluded_.insert(key);
    described_ = describe();
    emit planChanged();
}

bool TrashController::confirm()
{
    if (!lastPlan_ || preparing_)
        return false;
    const TrashPlan plan = included();
    lastPlan_.reset();
    described_.clear();
    folders_.clear();
    emit planChanged();
    const bool ok = service_.execute(plan) != 0;
    emit busyChanged();
    emit journalChanged();
    return ok;
}

void TrashController::cancelPlan()
{
    ++generation_;
    lastPlan_.reset();
    described_.clear();
    folders_.clear();
    preparing_ = false;
    emit planChanged();
}

void TrashController::stop() { service_.requestStop(); }

// ------------------------------------------------------------------ Journal

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
        m.insert(QStringLiteral("state"), stateLabel(o));
        m.insert(QStringLiteral("stopped"), o.stopped);
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
    const bool finished = op->state != OperationState::Planned && op->state != OperationState::Running;
    QVariantList entries;
    for (const OperationEntry& e : op->entries) {
        QVariantMap x;
        x.insert(QStringLiteral("name"), platform::lastSegment(e.path));
        x.insert(QStringLiteral("path"), e.path);
        x.insert(QStringLiteral("size"), AppController::formatBytes(e.sizeBytes));
        x.insert(QStringLiteral("result"), e.result);
        x.insert(QStringLiteral("resultText"), e.result == u"pending" && finished
                                                   ? QStringLiteral("Non traité : opération arrêtée")
                                                   : resultLabel(e.result));
        x.insert(QStringLiteral("message"), e.message);
        entries << x;
    }
    m.insert(QStringLiteral("entries"), entries);
    m.insert(QStringLiteral("state"), stateLabel(*op));
    return m;
}

bool TrashController::undo(qint64 operationId)
{
    const bool ok = service_.undo(operationId) != 0;
    emit busyChanged();
    emit journalChanged();
    return ok;
}

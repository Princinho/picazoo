#include "controllers/DuplicatesController.h"

#include "controllers/AppController.h"
#include "duplicates/DuplicateEngine.h"
#include "duplicates/DuplicateStore.h"
#include "library/Library.h"
#include "models/DuplicateGroupModel.h"

#include <QLocale>

using namespace picazoo;

namespace {

LevelFilter filterFrom(const QString& s)
{
    if (s == u"identical")
        return LevelFilter::Identical;
    if (s == u"same_image")
        return LevelFilter::SameImage;
    if (s == u"similar")
        return LevelFilter::Similar;
    return LevelFilter::All;
}

QString count(int n, const char* one, const char* many)
{
    return QLocale(QLocale::French).toString(n) + u' ' + QString::fromUtf8(n > 1 ? many : one);
}

} // namespace

DuplicatesController::DuplicatesController(Library& library, DuplicateEngine& engine, QObject* parent)
    : QObject(parent), lib_(library), engine_(engine)
{
    model_ = new DuplicateGroupModel(lib_, engine_.store(), this);
    filter_ = lib_.setting(QStringLiteral("duplicates_level_filter")).toString();
    model_->setFilter(filterFrom(filter_));
    connect(&engine_, &DuplicateEngine::groupsChanged, this, &DuplicatesController::refresh);
    connect(&engine_, &DuplicateEngine::progressChanged, this, &DuplicatesController::progressChanged);
    // Le contenu des copies (nom, dossier) suit la bibliothèque.
    connect(&lib_, &Library::foldersChanged, this, [this] { model_->reload(); });
    refresh();
}

QObject* DuplicatesController::groups() const { return model_; }

void DuplicatesController::refresh()
{
    model_->reload();
    const DuplicateSummary s = engine_.store().summary();
    groupCount_ = s.groups;
    reclaimable_ = s.reclaimableBytes;
    identicalCount_ = engine_.store().summary(LevelFilter::Identical).groups;
    int n = 0;
    for (GroupId id : engine_.store().groups())
        if (const auto g = engine_.store().group(id))
            for (const auto& c : g->copies)
                n += c.keep ? 0 : 1;
    trashCount_ = n;
    emit summaryChanged();
}

QString DuplicatesController::summaryText() const
{
    if (groupCount_ == 0)
        return QStringLiteral("Aucun doublon");
    return count(groupCount_, "groupe", "groupes") + QStringLiteral(" · ") + AppController::formatBytes(reclaimable_);
}

QString DuplicatesController::trashText() const
{
    return count(trashCount_, "fichier à la corbeille", "fichiers à la corbeille") + QStringLiteral(" · ") +
           AppController::formatBytes(reclaimable_);
}

void DuplicatesController::setFilter(const QString& f)
{
    if (f == filter_)
        return;
    filter_ = f;
    lib_.setSetting(QStringLiteral("duplicates_level_filter"), f);
    model_->setFilter(filterFrom(f));
    emit filterChanged();
}

bool DuplicatesController::active() const
{
    const auto p = engine_.progress();
    return p.phase == DuplicateProgress::Phase::Content || p.phase == DuplicateProgress::Phase::Visual ||
           p.phase == DuplicateProgress::Phase::Grouping;
}

bool DuplicatesController::waiting() const
{
    const auto p = engine_.progress();
    return p.phase == DuplicateProgress::Phase::Waiting && p.reason != u"library_busy" && p.reason != u"viewer";
}

bool DuplicatesController::paused() const { return engine_.isPaused(); }

bool DuplicatesController::autoSearch() const { return engine_.isEnabled(); }

void DuplicatesController::setAutoSearch(bool on)
{
    engine_.setEnabled(on);
    emit progressChanged();
}

QString DuplicatesController::progressText() const
{
    const auto p = engine_.progress();
    switch (p.phase) {
    case DuplicateProgress::Phase::Content:
    case DuplicateProgress::Phase::Visual:
        if (p.total > 0)
            return QStringLiteral("Doublons : %1 / %2").arg(QLocale(QLocale::French).toString(p.done),
                                                            QLocale(QLocale::French).toString(p.total));
        return QStringLiteral("Doublons : analyse…");
    case DuplicateProgress::Phase::Grouping: return QStringLiteral("Doublons : regroupement…");
    case DuplicateProgress::Phase::Waiting:
        if (p.reason == u"on_battery")
            return QStringLiteral("Doublons : en pause sur batterie");
        if (p.reason == u"user_paused")
            return QStringLiteral("Doublons : en pause");
        if (p.reason == u"disabled")
            return QStringLiteral("Doublons : recherche automatique désactivée");
        if (p.reason == u"library_busy")
            return QStringLiteral("Doublons : en attente de la fin de l'analyse de la bibliothèque");
        return QStringLiteral("Doublons : en attente");
    case DuplicateProgress::Phase::Idle: break;
    }
    return {};
}

double DuplicatesController::progressValue() const
{
    const auto p = engine_.progress();
    if (p.total <= 0)
        return -1;
    return double(p.done) / double(p.total);
}

bool DuplicatesController::setKeep(qint64 groupId, qint64 itemId, bool keep)
{
    const bool ok = engine_.store().setKeep(groupId, itemId, keep);
    model_->refreshGroup(groupId);
    refresh();
    return ok;
}

void DuplicatesController::keepOnly(qint64 groupId, qint64 itemId)
{
    const auto g = engine_.store().group(groupId);
    if (!g)
        return;
    engine_.store().setKeep(groupId, itemId, true);
    for (const auto& c : g->copies)
        if (c.item != itemId)
            engine_.store().setKeep(groupId, c.item, false);
    model_->refreshGroup(groupId);
    refresh();
}

void DuplicatesController::resetChoice(qint64 groupId)
{
    engine_.store().resetChoice(groupId);
    model_->refreshGroup(groupId);
    refresh();
}

void DuplicatesController::markNotDuplicates(qint64 groupId) { engine_.store().markNotDuplicates(groupId); }

void DuplicatesController::removeFromGroup(qint64 groupId, qint64 itemId) { engine_.store().removeFromGroup(groupId, itemId); }

void DuplicatesController::keepAll(qint64 groupId)
{
    engine_.store().keepAll(groupId);
    refresh();
}

void DuplicatesController::togglePause()
{
    if (engine_.isPaused())
        engine_.resume();
    else
        engine_.pause();
    emit progressChanged();
}

void DuplicatesController::rescan()
{
    if (!engine_.isEnabled())
        engine_.setEnabled(true);
    if (engine_.isPaused())
        engine_.resume();
    engine_.rescanAll();
    emit progressChanged();
}

void DuplicatesController::hold() { engine_.hold(); }

void DuplicatesController::release() { engine_.release(); }

qint64 DuplicatesController::groupOf(qint64 itemId) const { return engine_.store().groupOf(itemId).value_or(0); }

int DuplicatesController::duplicateCount(qint64 itemId) const { return engine_.store().duplicateCount(itemId); }

#include "controllers/NamingController.h"

#include "index/Database.h"
#include "library/Library.h"
#include "models/NamingGroupModel.h"
#include "naming/NamingEngine.h"
#include "naming/NamingStore.h"

#include <QDebug>
#include <QLocale>

using namespace picazoo;

namespace {

ConfidenceFilter filterFrom(const QString& s)
{
    if (s == u"regular_series")
        return ConfidenceFilter::RegularSeries;
    if (s == u"same_root")
        return ConfidenceFilter::SameRoot;
    if (s == u"generic_root")
        return ConfidenceFilter::GenericRoot;
    return ConfidenceFilter::All;
}

} // namespace

NamingController::NamingController(Library& library, NamingEngine& engine, QObject* parent)
    : QObject(parent), lib_(library), engine_(engine), model_(new NamingGroupModel(engine.store(), this))
{
    connect(&engine_, &NamingEngine::groupsChanged, this, &NamingController::refresh);
    // Un dossier surveillé devenu indisponible se voit sur les vignettes de ses éléments.
    connect(&lib_, &Library::rootsChanged, this, &NamingController::rootsChanged);
    // Vignettes calculées après le repérage : seules les cartes affichées les relisent.
    connect(&lib_, &Library::itemUpdated, this, [this](qint64 id) {
        if (model_->showsItem(id))
            emit itemUpdated(id);
    });
    for (const WatchedRoot& r : lib_.roots())
        rootStates_.insert(r.id, int(r.state));
    refresh();
}

void NamingController::rootsChanged()
{
    QHash<qint64, int> states;
    for (const WatchedRoot& r : lib_.roots())
        states.insert(r.id, int(r.state));
    if (states == rootStates_)
        return; // fin d'analyse sans changement de disponibilité
    rootStates_ = states;
    refresh();
}

// Le stockage peut être occupé ou plein : la correction est abandonnée, pas l'application.
template <typename Fn> bool NamingController::attempt(Fn fn)
{
    try {
        return fn();
    } catch (const DbError& e) {
        qWarning() << "Images liées :" << e.what();
        emit failed(QStringLiteral("Picazoo n'a pas pu enregistrer cette correction (bibliothèque occupée ou "
                                   "disque plein). Réessayez dans un instant. Vos photos ne sont pas concernées."));
        return false;
    }
}

QObject* NamingController::groups() const { return model_; }

void NamingController::refresh()
{
    NamingStore& store = engine_.store();
    const NamingSummary s = store.summary();
    counts_ = {
        {QStringLiteral("all"), s.groupCount},
        {QStringLiteral("regular_series"), s.regularSeries},
        {QStringLiteral("same_root"), s.sameRoot},
        {QStringLiteral("generic_root"), s.genericRoot},
    };
    decisions_.clear();
    for (const NamingDecision& d : store.decisions()) {
        const QString text = d.kind == NamingDecision::Kind::Rejected
                                 ? QStringLiteral("« %1 » : ce n'est pas un groupe").arg(d.label)
                                 : QStringLiteral("« %1 » : fusionné dans « %2 »")
                                       .arg(d.label, d.targetName.isEmpty() ? QStringLiteral("un groupe disparu")
                                                                            : d.targetName);
        decisions_ << QVariantMap{{QStringLiteral("key"), d.rootKey}, {QStringLiteral("text"), text}};
    }
    // Le filtre « Racines génériques » disparaît avec le dernier groupe de ce niveau.
    if (filter_ == u"generic_root" && s.genericRoot == 0 && !includeGeneric())
        setFilter(QStringLiteral("all"));
    rows_ = store.rows();
    model_->reload(rows_);
    ++revision_;
    emit changed();
}

QString NamingController::summaryText() const
{
    const int n = groupCount();
    if (n == 0)
        return QStringLiteral("Aucun groupe");
    return QLocale(QLocale::French).toString(n) + (n > 1 ? QStringLiteral(" groupes") : QStringLiteral(" groupe"));
}

void NamingController::setFilter(const QString& filter)
{
    if (filter == filter_)
        return;
    filter_ = filter;
    model_->setFilter(filterFrom(filter_));
    emit filterChanged();
}

bool NamingController::includeGeneric() const
{
    return lib_.setting(QStringLiteral("naming_include_generic_roots")).toInt() != 0;
}

void NamingController::setIncludeGeneric(bool on)
{
    if (on == includeGeneric())
        return;
    lib_.setSetting(QStringLiteral("naming_include_generic_roots"), on ? 1 : 0);
    engine_.rescanAll();
    emit includeGenericChanged();
}

QVariantMap NamingController::groupOf(qint64 itemId) const
{
    const auto group = engine_.store().rowOf(itemId);
    if (!group)
        return {};
    return {
        {QStringLiteral("id"), group->id},
        {QStringLiteral("name"), group->displayName},
        {QStringLiteral("count"), group->memberCount},
    };
}

QString NamingController::thumbUrl(qint64 itemId) const
{
    const auto item = lib_.item(itemId);
    if (!item)
        return {};
    return QStringLiteral("image://thumb/%1/%2/%3").arg(itemId).arg(item->version).arg(toDb(item->thumbState));
}

void NamingController::rescan() { engine_.rescanAll(); }

bool NamingController::rename(qint64 groupId, const QString& name)
{
    return attempt([&] { return engine_.store().rename(groupId, name); });
}

bool NamingController::confirmGroup(qint64 groupId)
{
    return attempt([&] { return engine_.store().confirm(groupId); });
}

bool NamingController::removeItem(qint64 groupId, qint64 itemId)
{
    return attempt([&] { return engine_.store().removeFromGroup(groupId, itemId); });
}

bool NamingController::addItems(qint64 groupId, const QVariantList& itemIds)
{
    return attempt([&] {
        bool any = false;
        for (const QVariant& id : itemIds)
            any |= engine_.store().addToGroup(groupId, id.toLongLong());
        return any;
    });
}

bool NamingController::merge(qint64 intoGroupId, qint64 fromGroupId, const QString& name)
{
    return attempt([&] {
        return engine_.store().mergeGroups(intoGroupId, fromGroupId) != 0 && engine_.store().rename(intoGroupId, name);
    });
}

bool NamingController::split(qint64 groupId, const QVariantList& itemIds, const QString& name)
{
    QList<ItemId> ids;
    for (const QVariant& id : itemIds)
        ids << id.toLongLong();
    return attempt([&] { return engine_.store().splitGroup(groupId, ids, name) != 0; });
}

bool NamingController::notAGroup(qint64 groupId)
{
    return attempt([&] { return engine_.store().markNotAGroup(groupId); });
}

bool NamingController::undoDecision(const QString& rootKey)
{
    return attempt([&] { return engine_.store().undoDecision(rootKey); });
}

QVariantList NamingController::searchItems(qint64 groupId, const QString& text) const
{
    QVariantList out;
    for (const NamingItem& i : engine_.store().searchItems(text, groupId)) {
        out << QVariantMap{
            {QStringLiteral("id"), i.item},
            {QStringLiteral("name"), i.fileName},
            {QStringLiteral("folder"), i.folderPath},
            {QStringLiteral("thumb"), QStringLiteral("image://thumb/%1/%2/%3")
                                          .arg(i.item)
                                          .arg(i.version)
                                          .arg(toDb(i.thumbState))},
        };
    }
    return out;
}

QVariantList NamingController::otherGroups(qint64 groupId, const QString& text) const
{
    QVariantList out;
    const QString wanted = text.trimmed();
    for (const NamingGroupRow& g : rows_) { // lignes déjà lues : pas de requête par frappe
        if (g.id == groupId || !g.displayName.contains(wanted, Qt::CaseInsensitive))
            continue;
        out << QVariantMap{{QStringLiteral("id"), g.id},
                           {QStringLiteral("name"), g.displayName},
                           {QStringLiteral("count"), g.memberCount}};
        if (out.size() >= 100)
            break; // la recherche par nom affine
    }
    return out;
}

#include "controllers/DuplicatesController.h"

#include "controllers/AppController.h"
#include "duplicates/DuplicateEngine.h"
#include "duplicates/DuplicateStore.h"
#include "duplicates/FolderRules.h"
#include "library/Library.h"
#include "models/DuplicateGroupModel.h"
#include "platform/win/Paths.h"

#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QLocale>
#include <QTimeZone>

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

QString shortPath(const QString& path)
{
    // Les deux derniers segments suffisent à reconnaître un dossier dans un menu.
    const QString parent = platform::parentPath(path);
    const QString last = platform::lastSegment(path);
    const QString before = platform::lastSegment(parent);
    return before.isEmpty() || before == parent ? path : before + u'\\' + last;
}

QString frDate(qint64 ms)
{
    if (ms <= 0)
        return {};
    return QLocale(QLocale::French).toString(QDateTime::fromMSecsSinceEpoch(ms), QStringLiteral("d MMM yyyy, HH 'h' mm"));
}

} // namespace

DuplicatesController::DuplicatesController(Library& library, DuplicateEngine& engine, QObject* parent)
    : QObject(parent), lib_(library), engine_(engine)
{
    model_ = new DuplicateGroupModel(lib_, engine_.store(), this);
    auto setting = [&](const char* key, const QString& fallback) {
        const QString v = lib_.setting(QString::fromLatin1(key)).toString();
        return v.isEmpty() ? fallback : v;
    };
    filter_ = setting("duplicates_level_filter", QStringLiteral("all"));
    type_ = setting("duplicates_type_filter", QStringLiteral("any"));
    state_ = setting("duplicates_state_filter", QStringLiteral("any"));
    sort_ = setting("duplicates_sort", QStringLiteral("reclaimable"));
    minMb_ = lib_.setting(QStringLiteral("duplicates_min_mb")).toInt();
    yearFrom_ = lib_.setting(QStringLiteral("duplicates_year_from")).toInt();
    yearTo_ = lib_.setting(QStringLiteral("duplicates_year_to")).toInt();
    view_ = lib_.setting(QStringLiteral("duplicates_view")).toString();
    applyQuery();
    if (view_.isEmpty()) // R10 : vue compacte d'office au-delà de 20 groupes, tant que rien n'est choisi
        view_ = engine_.store().summary().groups > 20 ? QStringLiteral("compact") : QStringLiteral("detailed");

    connect(&engine_, &DuplicateEngine::groupsChanged, this, [this] {
        refresh();
        if (foldersActive_)
            foldersTimer_.start();
        if (!setAside_.isEmpty() || !batches_.isEmpty())
            refreshSetAside();
    });
    connect(&engine_, &DuplicateEngine::progressChanged, this, &DuplicatesController::progressChanged);
    // Le contenu des copies (nom, dossier) suit la bibliothèque.
    connect(&lib_, &Library::foldersChanged, this, [this] { model_->reload(); });
    connect(model_, &DuplicateGroupModel::selectionChanged, this, &DuplicatesController::updateSelectionTotals);
    connect(model_, &DuplicateGroupModel::selectionDropped, this, [this](int n) {
        if (filtering_)
            emit notice(count(n, "groupe retiré", "groupes retirés") + QStringLiteral(" de la sélection : masqué") +
                            (n > 1 ? QStringLiteral("s") : QString()) + QStringLiteral(" par le filtre."),
                        0);
    });
    foldersTimer_.setSingleShot(true);
    foldersTimer_.setInterval(500); // research R6 : recalcul différé
    connect(&foldersTimer_, &QTimer::timeout, this, &DuplicatesController::refreshFolders);
    refresh();
}

QObject* DuplicatesController::groups() const { return model_; }

void DuplicatesController::refresh()
{
    model_->reload();
    DuplicateStore& store = engine_.store();
    const DuplicateSummary s = store.summary();
    groupCount_ = s.groups;
    reclaimable_ = s.reclaimableBytes;
    identicalCount_ = store.summary(LevelFilter::Identical).groups;
    const SelectionTotals t = store.trashTotals(); // une requête (la 002 relisait chaque groupe)
    trashCount_ = t.files;
    trashBytes_ = t.bytes;
    levelCounts_ = {
        {QStringLiteral("all"), groupCount_},
        {QStringLiteral("identical"), identicalCount_},
        {QStringLiteral("same_image"), store.summary(LevelFilter::SameImage).groups},
        {QStringLiteral("similar"), store.summary(LevelFilter::Similar).groups},
    };
    setAsideCount_ = int(store.setAside().size());
    updateSelectionTotals();
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
           AppController::formatBytes(trashBytes_);
}

// ------------------------------------------------------------------- Filtres

GroupQuery DuplicatesController::buildQuery() const
{
    GroupQuery q;
    q.level = filterFrom(filter_);
    q.folder = folder_;
    q.pairA = pairA_;
    q.pairB = pairB_;
    q.type = type_ == u"photos"     ? GroupQuery::Type::Photos
             : type_ == u"videos"   ? GroupQuery::Type::Videos
             : type_ == u"raw_jpeg" ? GroupQuery::Type::RawJpeg
                                    : GroupQuery::Type::Any;
    q.state = state_ == u"untouched"  ? GroupQuery::State::Untouched
              : state_ == u"modified" ? GroupQuery::State::Modified
              : state_ == u"flagged"  ? GroupQuery::State::Flagged
                                      : GroupQuery::State::Any;
    q.minBytes = qint64(minMb_) * 1000 * 1000;
    if (yearFrom_ > 0)
        q.dateFrom = QDateTime(QDate(yearFrom_, 1, 1), QTime(0, 0)).toMSecsSinceEpoch();
    if (yearTo_ > 0)
        q.dateTo = QDateTime(QDate(yearTo_, 12, 31), QTime(23, 59, 59, 999)).toMSecsSinceEpoch();
    q.sort = sort_ == u"copies"   ? GroupQuery::Sort::Copies
             : sort_ == u"date"   ? GroupQuery::Sort::Date
             : sort_ == u"folder" ? GroupQuery::Sort::Folder
                                  : GroupQuery::Sort::Reclaimable;
    return q;
}

void DuplicatesController::applyQuery()
{
    filtering_ = true;
    model_->setQuery(buildQuery());
    filtering_ = false;
    emit filterChanged();
    emit summaryChanged();
}

void DuplicatesController::setFilter(const QString& f)
{
    if (f == filter_)
        return;
    filter_ = f;
    lib_.setSetting(QStringLiteral("duplicates_level_filter"), f);
    applyQuery();
}

void DuplicatesController::setFolderFilter(const QString& path)
{
    const QString p = path.isEmpty() ? QString() : FolderRules::normalized(QDir::toNativeSeparators(path));
    if (p == folder_)
        return;
    folder_ = p;
    applyQuery();
}

void DuplicatesController::setPairFilter(const QString& a, const QString& b)
{
    pairA_ = a;
    pairB_ = b;
    applyQuery();
}

void DuplicatesController::setTypeFilter(const QString& t)
{
    if (t == type_)
        return;
    type_ = t;
    lib_.setSetting(QStringLiteral("duplicates_type_filter"), t);
    applyQuery();
}

void DuplicatesController::setStateFilter(const QString& s)
{
    if (s == state_)
        return;
    state_ = s;
    lib_.setSetting(QStringLiteral("duplicates_state_filter"), s);
    applyQuery();
}

void DuplicatesController::setMinMegabytes(int mb)
{
    if (mb == minMb_)
        return;
    minMb_ = std::max(0, mb);
    lib_.setSetting(QStringLiteral("duplicates_min_mb"), minMb_);
    applyQuery();
}

void DuplicatesController::setYears(int from, int to)
{
    if (from > 0 && to > 0 && from > to)
        std::swap(from, to);
    yearFrom_ = std::max(0, from);
    yearTo_ = std::max(0, to);
    lib_.setSetting(QStringLiteral("duplicates_year_from"), yearFrom_);
    lib_.setSetting(QStringLiteral("duplicates_year_to"), yearTo_);
    applyQuery();
}

void DuplicatesController::setSort(const QString& s)
{
    if (s == sort_)
        return;
    sort_ = s;
    lib_.setSetting(QStringLiteral("duplicates_sort"), s);
    applyQuery();
}

QString DuplicatesController::sortLabel() const
{
    if (sort_ == u"copies")
        return QStringLiteral("Nombre de copies");
    if (sort_ == u"date")
        return QStringLiteral("Date");
    if (sort_ == u"folder")
        return QStringLiteral("Dossier");
    return QStringLiteral("Place récupérable");
}

void DuplicatesController::setView(const QString& v)
{
    if (v == view_)
        return;
    view_ = v;
    lib_.setSetting(QStringLiteral("duplicates_view"), v);
    emit viewChanged();
}

void DuplicatesController::clearFilter(const QString& key)
{
    const bool all = key == u"all";
    if (all || key == u"folder")
        folder_.clear();
    if (all || key == u"pair") {
        pairA_.clear();
        pairB_.clear();
    }
    if (all || key == u"type") {
        type_ = QStringLiteral("any");
        lib_.setSetting(QStringLiteral("duplicates_type_filter"), type_);
    }
    if (all || key == u"state") {
        state_ = QStringLiteral("any");
        lib_.setSetting(QStringLiteral("duplicates_state_filter"), state_);
    }
    if (all || key == u"size") {
        minMb_ = 0;
        lib_.setSetting(QStringLiteral("duplicates_min_mb"), 0);
    }
    if (all || key == u"dates") {
        yearFrom_ = yearTo_ = 0;
        lib_.setSetting(QStringLiteral("duplicates_year_from"), 0);
        lib_.setSetting(QStringLiteral("duplicates_year_to"), 0);
    }
    applyQuery();
}

void DuplicatesController::showFolder(const QString& path)
{
    folder_.clear();
    pairA_.clear();
    pairB_.clear();
    type_ = state_ = QStringLiteral("any");
    minMb_ = yearFrom_ = yearTo_ = 0;
    filter_ = QStringLiteral("all");
    folder_ = FolderRules::normalized(QDir::toNativeSeparators(path));
    applyQuery();
}

QVariantList DuplicatesController::chips() const
{
    QVariantList out;
    auto chip = [&](const char* key, const QString& label) {
        out << QVariantMap{{QStringLiteral("key"), QString::fromLatin1(key)}, {QStringLiteral("label"), label}};
    };
    if (!folder_.isEmpty())
        chip("folder", QStringLiteral("Dossier : ") + folder_);
    if (!pairA_.isEmpty())
        chip("pair", QStringLiteral("Paire : ") + pairA_ + QStringLiteral(" ↔ ") + pairB_);
    if (type_ == u"photos")
        chip("type", QStringLiteral("Type : photos"));
    else if (type_ == u"videos")
        chip("type", QStringLiteral("Type : vidéos"));
    else if (type_ == u"raw_jpeg")
        chip("type", QStringLiteral("Type : RAW + JPEG"));
    if (state_ == u"untouched")
        chip("state", QStringLiteral("État : suggestion intacte"));
    else if (state_ == u"modified")
        chip("state", QStringLiteral("État : choix modifié"));
    else if (state_ == u"flagged")
        chip("state", QStringLiteral("État : signalés"));
    if (minMb_ > 0)
        chip("size", QStringLiteral("Taille ≥ ") + AppController::formatBytes(qint64(minMb_) * 1000 * 1000));
    if (yearFrom_ > 0 || yearTo_ > 0) {
        QString d;
        if (yearFrom_ > 0 && yearTo_ > 0)
            d = yearFrom_ == yearTo_ ? QString::number(yearFrom_) : QStringLiteral("%1 – %2").arg(yearFrom_).arg(yearTo_);
        else if (yearFrom_ > 0)
            d = QStringLiteral("depuis ") + QString::number(yearFrom_);
        else
            d = QStringLiteral("jusqu'à ") + QString::number(yearTo_);
        chip("dates", QStringLiteral("Dates : ") + d);
    }
    return out;
}

int DuplicatesController::shownCount() const { return model_->count(); }

QString DuplicatesController::shownText() const
{
    const int shown = model_->count();
    if (shown == groupCount_)
        return count(shown, "groupe affiché", "groupes affichés");
    return count(shown, "groupe affiché", "groupes affichés") + QStringLiteral(" sur ") + QLocale(QLocale::French).toString(groupCount_);
}

// ------------------------------------------------------------------ Analyse

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

// -------------------------------------------------------- Décisions unitaires

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

// ------------------------------------------------------------------ Sélection

int DuplicatesController::selectedCount() const { return model_->selectedCount(); }

QString DuplicatesController::selectionText() const
{
    return count(model_->selectedCount(), "groupe sélectionné", "groupes sélectionnés");
}

void DuplicatesController::updateSelectionTotals()
{
    QElapsedTimer timer;
    timer.start();
    const SelectionTotals t = engine_.store().totals(model_->selectedIds());
    selectionDetail_ = count(t.files, "fichier à la corbeille", "fichiers à la corbeille") + QStringLiteral(" · ") +
                       AppController::formatBytes(t.bytes) + QStringLiteral(" libérés");
    if (model_->selectedCount() > 1000)
        qInfo() << "Doublons : totaux de" << model_->selectedCount() << "groupes en" << timer.elapsed() << "ms";
    emit selectionChanged();
}

QVariantList DuplicatesController::selectedGroupIds() const
{
    QVariantList out;
    for (GroupId id : model_->selectedIds())
        out << id;
    return out;
}

QVariantList DuplicatesController::selectionFolders() const
{
    QVariantList out;
    for (const auto& f : engine_.store().foldersOf(model_->selectedIds()))
        out << QVariantMap{{QStringLiteral("path"), f.first},
                           {QStringLiteral("label"), shortPath(f.first)},
                           {QStringLiteral("count"), f.second}};
    return out;
}

void DuplicatesController::afterBatch(const BatchResult& r)
{
    if (r.batchId)
        model_->clearSelection();
    refresh();
    if (foldersActive_)
        refreshFolders();
    if (!batches_.isEmpty() || !setAside_.isEmpty())
        refreshSetAside();
    if (!r.message.isEmpty())
        emit notice(r.message, r.batchId);
}

void DuplicatesController::batchReset() { afterBatch(engine_.store().applyBatch(BatchKind::Reset, model_->selectedIds())); }

void DuplicatesController::batchKeepAll() { afterBatch(engine_.store().applyBatch(BatchKind::KeepAll, model_->selectedIds())); }

void DuplicatesController::batchNotDuplicates()
{
    afterBatch(engine_.store().applyBatch(BatchKind::NotDuplicate, model_->selectedIds()));
}

void DuplicatesController::batchKeepFolder(const QString& folder)
{
    afterBatch(engine_.store().applyBatch(BatchKind::KeepFolder, model_->selectedIds(), folder));
}

void DuplicatesController::keepPairSide(const QString& a, const QString& b, bool keepA)
{
    GroupQuery q;
    q.pairA = a;
    q.pairB = b;
    afterBatch(engine_.store().applyBatch(BatchKind::KeepFolder, engine_.store().groups(q), keepA ? a : b));
}

bool DuplicatesController::undoBatch(qint64 batchId)
{
    const UndoResult u = engine_.store().undoBatch(batchId);
    refresh();
    if (foldersActive_)
        refreshFolders();
    refreshSetAside();
    QString msg = QStringLiteral("Action annulée.");
    if (u.notRestored > 0)
        msg += u' ' + count(u.notRestored, "groupe modifié", "groupes modifiés") +
               QStringLiteral(" depuis : non rétabli") + (u.notRestored > 1 ? QStringLiteral("s.") : QStringLiteral("."));
    emit notice(msg, 0);
    return true;
}

// ----------------------------------------------------------- Règles de dossier

void DuplicatesController::addRule(const QString& path, const QString& kind)
{
    const QString p = FolderRules::normalized(QDir::toNativeSeparators(path));
    if (p.isEmpty())
        return;
    const RuleKind k = kind == u"avoid" ? RuleKind::Avoid : RuleKind::Prefer;
    engine_.store().setFolderRule(p, k);
    refresh();
    refreshFolders();
    emit notice(QStringLiteral("Règle créée : toujours ") + (k == RuleKind::Avoid ? QStringLiteral("écarter « ") : QStringLiteral("préférer « ")) +
                    p + QStringLiteral(" ». Les suggestions sont mises à jour."),
                0);
}

void DuplicatesController::changeRule(qint64 id, const QString& kind)
{
    for (const FolderRule& r : engine_.store().folderRules())
        if (r.id == id) {
            engine_.store().setFolderRule(r.path, kind == u"avoid" ? RuleKind::Avoid : RuleKind::Prefer);
            break;
        }
    refresh();
    refreshFolders();
}

void DuplicatesController::relinkRule(qint64 id, const QString& path)
{
    engine_.store().relinkFolderRule(id, QDir::toNativeSeparators(path));
    refresh();
    refreshFolders();
}

void DuplicatesController::removeRule(qint64 id)
{
    engine_.store().removeFolderRule(id);
    refresh();
    refreshFolders();
    emit notice(QStringLiteral("Règle supprimée. Les suggestions sont mises à jour."), 0);
}

void DuplicatesController::setFoldersActive(bool on)
{
    if (on == foldersActive_)
        return;
    foldersActive_ = on;
    if (on)
        refreshFolders();
    else
        emit foldersChanged();
}

void DuplicatesController::refreshFolders()
{
    QElapsedTimer timer;
    timer.start();
    DuplicateStore& store = engine_.store();
    pairs_.clear();
    for (const FolderPair& p : store.folderPairs()) {
        QString stats = count(p.groups, "groupe", "groupes") + QStringLiteral(" · ") + AppController::formatBytes(p.bytes);
        if (p.subPairs > 1)
            stats += QStringLiteral(" · ") + count(p.subPairs, "sous-dossier", "sous-dossiers");
        pairs_ << QVariantMap{{QStringLiteral("a"), p.a},
                              {QStringLiteral("b"), p.b},
                              {QStringLiteral("aLabel"), shortPath(p.a)},
                              {QStringLiteral("bLabel"), shortPath(p.b)},
                              {QStringLiteral("stats"), stats},
                              {QStringLiteral("active"), platform::samePath(p.a, pairA_) && platform::samePath(p.b, pairB_)}};
    }
    internals_.clear();
    for (const FolderStat& s : store.internalDuplicates())
        internals_ << QVariantMap{{QStringLiteral("path"), s.folder},
                                  {QStringLiteral("label"), shortPath(s.folder)},
                                  {QStringLiteral("stats"), count(s.groups, "groupe", "groupes") + QStringLiteral(" · ") +
                                                                AppController::formatBytes(s.bytes)}};
    rules_.clear();
    for (const FolderRule& r : store.folderRules())
        rules_ << QVariantMap{
            {QStringLiteral("id"), r.id},
            {QStringLiteral("path"), r.path},
            {QStringLiteral("kind"), r.kind == RuleKind::Avoid ? QStringLiteral("avoid") : QStringLiteral("prefer")},
            {QStringLiteral("kindLabel"), r.kind == RuleKind::Avoid ? QStringLiteral("Écarter") : QStringLiteral("Préférer")},
            {QStringLiteral("found"), r.found},
            {QStringLiteral("note"), r.found ? count(r.groups, "groupe orienté", "groupes orientés")
                                             : QStringLiteral("Dossier introuvable : règle ignorée")}};
    if (timer.elapsed() > 200)
        qInfo() << "Doublons : dossiers calculés en" << timer.elapsed() << "ms";
    emit foldersChanged();
}

// ----------------------------------------------------- Écartés de la revue

void DuplicatesController::refreshSetAside()
{
    DuplicateStore& store = engine_.store();
    setAsideEntries_ = store.setAside();
    setAside_.clear();
    for (int i = 0; i < setAsideEntries_.size(); ++i) {
        const SetAsideEntry& e = setAsideEntries_[i];
        QVariantMap m;
        m.insert(QStringLiteral("index"), i);
        m.insert(QStringLiteral("decision"), e.kind == SetAsideEntry::Kind::KeptAll ? QStringLiteral("Garder toutes")
                                                                                   : QStringLiteral("Pas des doublons"));
        m.insert(QStringLiteral("date"), frDate(e.decidedAt));
        QStringList names, folders;
        QVariantList thumbs;
        for (ItemId id : e.items) {
            const QVariantMap info = AppController::describe(lib_, id);
            if (info.isEmpty())
                continue;
            names << info.value(QStringLiteral("name")).toString();
            const QString folder = shortPath(platform::parentPath(info.value(QStringLiteral("path")).toString()));
            if (!folders.contains(folder))
                folders << folder;
            if (thumbs.size() < 4)
                thumbs << QVariantMap{{QStringLiteral("id"), id}, {QStringLiteral("version"), info.value(QStringLiteral("version"))}};
        }
        m.insert(QStringLiteral("name"), names.value(0) + (names.size() > 1 ? QStringLiteral(" (+%1)").arg(names.size() - 1) : QString()));
        m.insert(QStringLiteral("folders"), folders.join(QStringLiteral(" · ")));
        m.insert(QStringLiteral("thumbs"), thumbs);
        setAside_ << m;
    }
    batches_.clear();
    for (const BatchInfo& b : store.batches()) {
        if (b.kind == BatchKind::Reset)
            continue; // « Revenir à la suggestion » ne sort aucun groupe de la revue
        batches_ << QVariantMap{{QStringLiteral("id"), b.id},
                                {QStringLiteral("title"), frDate(b.createdAt) + QStringLiteral(" — ") + b.label},
                                {QStringLiteral("undone"), b.undone}};
    }
    emit setAsideChanged();
}

void DuplicatesController::restoreSetAside(const QVariantList& indexes)
{
    QList<SetAsideEntry> entries;
    for (const QVariant& v : indexes) {
        const int i = v.toInt();
        if (i >= 0 && i < setAsideEntries_.size())
            entries << setAsideEntries_[i];
    }
    if (entries.isEmpty())
        return;
    engine_.store().restore(entries);
    refresh();
    refreshSetAside();
    emit notice(count(int(entries.size()), "groupe remis", "groupes remis") +
                    QStringLiteral(" dans la revue (après la mise à jour des groupes)."),
                0);
}

QString DuplicatesController::urlToPath(const QUrl& url) const { return QDir::toNativeSeparators(url.toLocalFile()); }

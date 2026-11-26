#include "naming/NamingEngine.h"

#include "index/Database.h"
#include "jobs/ThumbnailPipeline.h"
#include "library/Library.h"
#include "naming/NamingDb.h"
#include "naming/NamingDetector.h"
#include "naming/NamingStore.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QHash>
#include <QSet>
#include <QThread>

#include <algorithm>
#include <atomic>
#include <mutex>
#include <optional>

namespace picazoo {

namespace {

// Copie en mémoire des noms indexés, pour ne relire que les dossiers touchés.
struct CachedItem {
    FolderId folder = 0;
    QString fileName;
    std::optional<DetectedRoot> root;
};

struct NameCache {
    QHash<ItemId, CachedItem> items;
    QHash<FolderId, QSet<ItemId>> folders;
    QHash<QString, QSet<ItemId>> roots; // racine détectée -> éléments

    void insert(ItemId id, FolderId folder, const QString& fileName)
    {
        CachedItem item{folder, fileName, detectRoot(baseNameOf(fileName))};
        folders[folder].insert(id);
        if (item.root)
            roots[item.root->normalizedKey].insert(id);
        items.insert(id, std::move(item));
    }

    void remove(ItemId id)
    {
        const auto it = items.constFind(id);
        if (it == items.constEnd())
            return;
        auto folder = folders.find(it->folder);
        if (folder != folders.end()) {
            folder->remove(id);
            if (folder->isEmpty())
                folders.erase(folder);
        }
        if (it->root) {
            auto root = roots.find(it->root->normalizedKey);
            if (root != roots.end()) {
                root->remove(id);
                if (root->isEmpty())
                    roots.erase(root);
            }
        }
        items.erase(it);
    }
};

struct GroupRow {
    NamingGroupId id = 0;
    bool confirmed = false;
    bool renamed = false;
    QString name;
};

struct Membership {
    NamingGroupId group = 0;
    bool confirmed = false;
};

// Durée maximale d'une transaction du moteur : les autres connexions (vignettes, doublons,
// corrections de l'utilisateur) n'attendent jamais longtemps le verrou d'écriture.
constexpr qint64 kSliceMs = 200;

// Applique aux groupes des racines `keys` ce que disent les noms et les décisions, par tranches
// de transactions courtes ; l'état est relu en mémoire au début de chaque tranche plutôt que
// requête par requête. Vrai si un groupe a changé.
class Reconciler {
public:
    Reconciler(Database& db, const NameCache& cache, bool includeGeneric)
        : db_(db), cache_(cache), includeGeneric_(includeGeneric),
          members_(db.prepare("SELECT item_id, detected_root, manual FROM naming_member WHERE group_id = ?")),
          removeMember_(db.prepare("DELETE FROM naming_member WHERE item_id = ?")),
          addMember_(db.prepare("INSERT INTO naming_member (group_id, item_id, detected_root, manual)"
                                " VALUES (?, ?, ?, 0)")),
          setRoot_(db.prepare("UPDATE naming_member SET detected_root = ? WHERE item_id = ?"))
    {
    }

    bool run(const QSet<QString>& keys)
    {
        // Ordre stable : le résultat ne dépend pas de l'ordre de la table de hachage.
        QStringList ordered(keys.begin(), keys.end());
        ordered.sort();
        QSet<QString> done;
        std::optional<Transaction> tx;
        QElapsedTimer slice;
        for (const QString& key : std::as_const(ordered)) {
            if (!tx) {
                tx.emplace(db_);
                load(); // une correction a pu passer entre deux tranches
                slice.start();
            }
            const QString target = rules_.resolve(key);
            if (!done.contains(target)) {
                done.insert(target);
                reconcile(target);
            }
            if (slice.elapsed() >= kSliceMs) {
                endSlice();
                tx->commit();
                tx.reset();
            }
        }
        if (tx) {
            endSlice();
            tx->commit();
        }
        return changed_;
    }

private:
    void load()
    {
        rules_ = NamingRules::load(db_);
        groups_.clear();
        sources_.clear();
        membership_.clear();
        excluded_.clear();
        dropped_.clear();
        auto g = db_.prepare("SELECT id, root_key, state, renamed, display_name FROM naming_group");
        while (g.step())
            groups_.insert(g.text(1), GroupRow{g.int64(0), g.text(2) == u"confirmed", g.int32(3) != 0, g.text(4)});
        for (auto it = rules_.merged.constBegin(); it != rules_.merged.constEnd(); ++it)
            sources_[rules_.resolve(it.key())] << it.key();
        auto m = db_.prepare("SELECT m.item_id, m.group_id, g.state = 'confirmed' FROM naming_member m"
                             " JOIN naming_group g ON g.id = m.group_id");
        while (m.step())
            membership_.insert(m.int64(0), Membership{m.int64(1), m.int32(2) != 0});
        auto x = db_.prepare("SELECT item_id, root_key FROM naming_exclusion");
        while (x.step())
            excluded_.insert({x.int64(0), x.text(1).toLower()});
    }

    // Un élément a pu passer d'un groupe à un autre : ceux qu'il quitte sont recomptés.
    void endSlice()
    {
        for (NamingGroupId id : std::as_const(left_))
            changed_ |= refreshNamingGroup(db_, id);
        left_.clear();
    }

    std::optional<Membership> membershipOf(ItemId id) const
    {
        const auto it = membership_.constFind(id);
        if (it == membership_.constEnd() || dropped_.contains(it->group))
            return std::nullopt;
        return *it;
    }

    void drop(const QString& key)
    {
        const auto it = groups_.constFind(key);
        if (it == groups_.constEnd())
            return;
        deleteNamingGroup(db_, it->id);
        dropped_.insert(it->id);
        groups_.erase(it);
        changed_ = true;
    }

    void reconcile(const QString& key)
    {
        if (rules_.rejected.contains(key)) {
            drop(key);
            return;
        }
        const auto found = groups_.constFind(key);
        const bool exists = found != groups_.constEnd();
        const GroupRow group = exists ? *found : GroupRow{};

        QStringList roots{key};
        roots += sources_.value(key);
        QList<ItemId> candidates;
        for (const QString& root : std::as_const(roots)) {
            const auto it = cache_.roots.constFind(root);
            if (it != cache_.roots.constEnd())
                candidates += QList<ItemId>(it->begin(), it->end());
        }
        if (!exists && candidates.size() < 2)
            return; // FR-004

        struct Current {
            QString root;
            bool manual;
        };
        QHash<ItemId, Current> current;
        if (exists) {
            members_.reset();
            members_.bind(1, group.id);
            while (members_.step())
                current.insert(members_.int64(0), Current{members_.text(1), members_.int32(2) != 0});
            members_.reset();
        }

        // Un groupe confirmé garde tous ses membres, même renommés sur le disque (US4). Un
        // groupe à revoir suit les noms : seuls restent les éléments ajoutés à la main.
        QSet<ItemId> desired;
        for (auto it = current.constBegin(); it != current.constEnd(); ++it) {
            if (group.confirmed || it->manual)
                desired.insert(it.key());
        }
        for (ItemId id : std::as_const(candidates)) {
            if (desired.contains(id))
                continue;
            const DetectedRoot& root = *cache_.items.constFind(id)->root;
            // Une racine générique n'est regroupée que sur demande, ou si l'utilisateur a
            // confirmé le groupe (FR-003, FR-016).
            if (root.generic && !includeGeneric_ && !group.confirmed)
                continue;
            if (current.contains(id)) {
                desired.insert(id); // déjà membre : ni retiré à la main, ni ailleurs
                continue;
            }
            // Retiré de ce groupe par l'utilisateur (exclusion attachée à la racine du groupe).
            if (excluded_.contains({id, key.toLower()}))
                continue;
            const auto held = membershipOf(id);
            if (!held || held->group == group.id || !held->confirmed)
                desired.insert(id);
        }
        // Éléments pas encore vus par le cache (ajoutés entre-temps) : laissés tels quels.
        if (desired.size() < 2) {
            drop(key);
            return;
        }

        QList<ItemId> sorted(desired.begin(), desired.end());
        auto cachedItem = [this](ItemId id) -> const CachedItem& {
            static const CachedItem unknown;
            const auto it = cache_.items.constFind(id);
            return it != cache_.items.constEnd() ? *it : unknown;
        };
        std::sort(sorted.begin(), sorted.end(), [&](ItemId a, ItemId b) {
            const int c = cachedItem(a).fileName.compare(cachedItem(b).fileName, Qt::CaseInsensitive);
            return c != 0 ? c < 0 : a < b;
        });
        QList<std::optional<DetectedRoot>> detected;
        detected.reserve(sorted.size());
        QString name;
        for (ItemId id : std::as_const(sorted)) {
            const auto& root = cachedItem(id).root;
            detected << root;
            if (name.isEmpty() && root && root->normalizedKey == key)
                name = suggestedName(root->displayRoot);
        }
        if (name.isEmpty())
            name = suggestedName(key);
        if (exists && (group.renamed || group.confirmed))
            name = group.name; // le nom retenu ne bouge plus (FR-009)
        const QString confidence = toDb(confidenceFor(detected));
        const qint64 now = QDateTime::currentMSecsSinceEpoch();

        NamingGroupId id = group.id;
        if (!exists) {
            auto insert = db_.prepare("INSERT INTO naming_group (root_key, display_name, confidence, member_count,"
                                      " updated_at) VALUES (?, ?, ?, ?, ?)");
            insert.bind(1, key).bind(2, name).bind(3, confidence).bind(4, qint64(sorted.size())).bind(5, now);
            insert.run();
            id = db_.lastInsertId();
            groups_.insert(key, GroupRow{id, false, false, name});
            changed_ = true;
        }
        for (auto it = current.constBegin(); it != current.constEnd(); ++it) {
            if (!desired.contains(it.key())) {
                removeMember_.bind(1, it.key());
                removeMember_.run();
                membership_.remove(it.key());
                changed_ = true;
            }
        }
        for (ItemId item : std::as_const(sorted)) {
            const auto cached = cache_.items.constFind(item);
            const QString root = cached != cache_.items.constEnd() && cached->root ? cached->root->normalizedKey
                                                                                   : QString();
            const auto was = current.constFind(item);
            if (was != current.constEnd()) {
                if (cached != cache_.items.constEnd() && was->root != root) {
                    setRoot_.bind(1, root).bind(2, item);
                    setRoot_.run();
                    changed_ = true;
                }
                continue;
            }
            if (const auto held = membershipOf(item)) {
                left_.insert(held->group);
                removeMember_.bind(1, item);
                removeMember_.run();
            }
            addMember_.bind(1, id).bind(2, item).bind(3, root);
            addMember_.run();
            membership_.insert(item, Membership{id, exists && group.confirmed});
            changed_ = true;
        }
        auto update = db_.prepare("UPDATE naming_group SET display_name = ?1, confidence = ?2, member_count = ?3,"
                                  " updated_at = ?4 WHERE id = ?5 AND (display_name != ?1 OR confidence != ?2"
                                  " OR member_count != ?3)");
        update.bind(1, name).bind(2, confidence).bind(3, qint64(sorted.size())).bind(4, now).bind(5, id);
        update.run();
        changed_ |= db_.changes() > 0;
    }

    Database& db_;
    const NameCache& cache_;
    const bool includeGeneric_;
    NamingRules rules_;
    Statement members_, removeMember_, addMember_, setRoot_;
    QHash<QString, GroupRow> groups_;
    QHash<QString, QStringList> sources_;
    QHash<ItemId, Membership> membership_;
    QSet<std::pair<ItemId, QString>> excluded_;
    QSet<NamingGroupId> dropped_;
    QSet<NamingGroupId> left_;
    bool changed_ = false;
};

} // namespace

struct NamingEngine::State {
    // Demandes en attente, remplies par l'interface.
    std::mutex queueMutex;
    bool full = false;
    QSet<FolderId> dirtyFolders;
    bool foldersRemoved = false; // des dossiers ont pu disparaître (racine retirée)
    QSet<QString> dirtyRoots;
    bool taskQueued = false;
    bool running = false;

    std::mutex runMutex; // tenu pendant une tâche : le destructeur l'attend
    std::atomic<bool> stopped{false};
    std::atomic<bool> changed{false};
    std::atomic<int> fullScans{0};
    std::atomic<qint64> slowestPassMs{0};

    // Thread d'indexation seulement.
    std::unique_ptr<Database> db;
    NameCache cache;
    bool cacheValid = false;

    bool pending() const { return full || foldersRemoved || !dirtyFolders.isEmpty() || !dirtyRoots.isEmpty(); }

    void run(const Library& library)
    {
        std::lock_guard running_(runMutex);
        if (stopped)
            return;
        bool wantFull = false, removed = false;
        QSet<FolderId> dirty;
        QSet<QString> keys;
        {
            std::lock_guard lock(queueMutex);
            std::swap(wantFull, full);
            std::swap(removed, foldersRemoved);
            dirty.swap(dirtyFolders);
            keys.swap(dirtyRoots);
            taskQueued = false;
            running = true;
        }
        struct Done {
            State& s;
            bool ok = false;
            ~Done()
            {
                if (!ok)
                    s.cacheValid = false; // erreur de stockage : tout sera relu la prochaine fois
                std::lock_guard lock(s.queueMutex);
                s.running = false;
            }
        } done{*this};
        const bool includeGeneric =
            library.setting(QStringLiteral("naming_include_generic_roots")).toInt() != 0;
        QElapsedTimer elapsed;
        elapsed.start();
        if (wantFull || !cacheValid)
            fullScan(includeGeneric);
        else
            update(dirty, removed, keys, includeGeneric);
        slowestPassMs = std::max(slowestPassMs.load(), elapsed.elapsed());
        done.ok = true;
    }

    void fullScan(bool includeGeneric)
    {
        cache = {};
        {
            std::lock_guard lock(db->mutex());
            auto q = db->prepare("SELECT id, folder_id, file_name FROM item");
            while (q.step())
                cache.insert(q.int64(0), q.int64(1), q.text(2));
        }
        QSet<QString> keys(cache.roots.keyBegin(), cache.roots.keyEnd());
        {
            std::lock_guard lock(db->mutex());
            auto q = db->prepare("SELECT root_key FROM naming_group");
            while (q.step())
                keys.insert(q.text(0));
        }
        bool any = Reconciler(*db, cache, includeGeneric).run(keys);
        any |= prune();
        cacheValid = true;
        ++fullScans;
        if (any)
            changed = true;
    }

    // Groupes dont un membre a disparu de l'index (suppression en cascade).
    bool prune()
    {
        Transaction tx(*db);
        QList<NamingGroupId> stale;
        {
            auto q = db->prepare("SELECT g.id FROM naming_group g WHERE g.member_count !="
                                 " (SELECT COUNT(*) FROM naming_member m WHERE m.group_id = g.id)");
            while (q.step())
                stale << q.int64(0);
        }
        for (NamingGroupId id : std::as_const(stale))
            refreshNamingGroup(*db, id);
        tx.commit();
        return !stale.isEmpty();
    }

    void update(QSet<FolderId> dirty, bool checkRemoved, QSet<QString> keys, bool includeGeneric)
    {
        QHash<ItemId, QPair<FolderId, QString>> present; // contenu actuel des dossiers touchés
        {
            std::lock_guard lock(db->mutex());
            if (checkRemoved) {
                QSet<FolderId> existing;
                auto q = db->prepare("SELECT id FROM folder");
                while (q.step())
                    existing.insert(q.int64(0));
                for (auto it = cache.folders.keyBegin(); it != cache.folders.keyEnd(); ++it) {
                    if (!existing.contains(*it))
                        dirty.insert(*it);
                }
            }
            auto q = db->prepare("SELECT id, file_name FROM item WHERE folder_id = ?");
            for (FolderId folder : std::as_const(dirty)) {
                q.reset();
                q.bind(1, folder);
                while (q.step())
                    present.insert(q.int64(0), {folder, q.text(1)});
            }
        }

        bool removedAny = false;
        auto touch = [&](ItemId id) {
            const auto it = cache.items.constFind(id);
            if (it != cache.items.constEnd() && it->root)
                keys.insert(it->root->normalizedKey);
        };
        // Disparus d'abord : un élément déplacé est retrouvé ensuite dans son nouveau dossier.
        for (FolderId folder : std::as_const(dirty)) {
            const QSet<ItemId> before = cache.folders.value(folder);
            for (ItemId id : before) {
                if (!present.contains(id)) {
                    touch(id);
                    cache.remove(id);
                    removedAny = true;
                }
            }
        }
        QList<ItemId> renamed;
        for (auto it = present.constBegin(); it != present.constEnd(); ++it) {
            const auto known = cache.items.constFind(it.key());
            if (known != cache.items.constEnd() && known->fileName == it->second && known->folder == it->first)
                continue;
            if (known != cache.items.constEnd()) {
                touch(it.key());
                renamed << it.key();
                cache.remove(it.key());
            }
            cache.insert(it.key(), it->first, it->second);
            touch(it.key());
        }
        if (keys.isEmpty() && !removedAny)
            return; // vignettes ou dates mises à jour : aucun nom n'a changé

        // Un élément renommé peut figurer dans un groupe d'une autre racine (ajout, fusion).
        {
            std::lock_guard lock(db->mutex());
            auto q = db->prepare("SELECT g.root_key FROM naming_member m JOIN naming_group g ON g.id = m.group_id"
                                 " WHERE m.item_id = ?");
            for (ItemId id : std::as_const(renamed)) {
                q.reset();
                q.bind(1, id);
                if (q.step())
                    keys.insert(q.text(0));
            }
        }
        bool any = Reconciler(*db, cache, includeGeneric).run(keys);
        if (removedAny)
            any |= prune();
        if (any)
            changed = true;
    }
};

NamingEngine::NamingEngine(Library& library, NamingStore& store, QObject* parent)
    : QObject(parent), lib_(library), store_(store), state_(std::make_shared<State>())
{
    state_->db = Database::open(QDir(lib_.dataDir()).filePath(QStringLiteral("library.db")));

    connect(&lib_, &Library::scanFinished, this, [this] {
        if (!fullRequested_) {
            rescanAll(); // premier repérage (N3)
            return;
        }
        {
            std::lock_guard lock(state_->queueMutex);
            state_->foldersRemoved = true;
        }
        batchTimer_.start();
    });
    connect(&lib_, &Library::itemsChanged, this, &NamingEngine::queueFolder);
    connect(&lib_, &Library::foldersChanged, this, [this] {
        if (!fullRequested_)
            return; // le repérage complet couvrira tout
        {
            std::lock_guard lock(state_->queueMutex);
            state_->foldersRemoved = true;
        }
        if (!batchTimer_.isActive())
            batchTimer_.start();
    });
    store_.setOnChanged([this](const QStringList& keys) {
        if (fullRequested_) {
            {
                std::lock_guard lock(state_->queueMutex);
                state_->dirtyRoots.unite(QSet<QString>(keys.begin(), keys.end()));
            }
            schedule();
        }
        // Signal différé : l'appel vient souvent d'un élément d'interface que la mise à jour
        // de la liste détruirait.
        QMetaObject::invokeMethod(this, &NamingEngine::groupsChanged, Qt::QueuedConnection);
    });

    // Même fenêtre que les notifications de la bibliothèque (T023).
    batchTimer_.setSingleShot(true);
    batchTimer_.setInterval(250);
    connect(&batchTimer_, &QTimer::timeout, this, &NamingEngine::schedule);
    notifyTimer_.setInterval(250);
    connect(&notifyTimer_, &QTimer::timeout, this, &NamingEngine::flush);
    notifyTimer_.start();
}

NamingEngine::~NamingEngine()
{
    store_.setOnChanged({});
    state_->stopped = true;
    std::lock_guard wait(state_->runMutex);
}

void NamingEngine::queueFolder(qint64 folder)
{
    if (!fullRequested_)
        return;
    {
        std::lock_guard lock(state_->queueMutex);
        state_->dirtyFolders.insert(folder);
    }
    // Pas de redémarrage : une analyse continue ne doit pas repousser le calcul indéfiniment.
    if (!batchTimer_.isActive())
        batchTimer_.start();
}

void NamingEngine::rescanAll()
{
    fullRequested_ = true;
    {
        std::lock_guard lock(state_->queueMutex);
        state_->full = true;
    }
    schedule();
}

void NamingEngine::schedule()
{
    {
        std::lock_guard lock(state_->queueMutex);
        if (state_->taskQueued || !state_->pending())
            return;
        state_->taskQueued = true;
    }
    lib_.post([state = state_, library = &lib_] { state->run(*library); });
}

void NamingEngine::flush()
{
    if (state_->changed.exchange(false))
        emit groupsChanged();
}

int NamingEngine::fullScanCount() const { return state_->fullScans.load(); }

qint64 NamingEngine::takeSlowestPassMs() { return state_->slowestPassMs.exchange(0); }

bool NamingEngine::idle() const
{
    if (batchTimer_.isActive() || state_->changed.load())
        return false;
    std::lock_guard lock(state_->queueMutex);
    return !state_->taskQueued && !state_->running && !state_->pending();
}

bool NamingEngine::waitForIdle(int timeoutMs)
{
    QElapsedTimer t;
    t.start();
    if (!lib_.waitForIdle(timeoutMs))
        return false;
    int stable = 0;
    while (t.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        const bool ok = lib_.indexerIdle() && lib_.pipeline().idle() && idle();
        stable = ok ? stable + 1 : 0;
        if (stable >= 20) { // au-delà de la fenêtre de notification de la bibliothèque (250 ms)
            flush();
            QCoreApplication::processEvents();
            return true;
        }
        QThread::msleep(20);
    }
    return false;
}

} // namespace picazoo

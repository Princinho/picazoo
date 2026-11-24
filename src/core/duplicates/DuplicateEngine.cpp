#include "duplicates/DuplicateEngine.h"

#include "duplicates/Suggestion.h"
#include "duplicates/VisualFingerprint.h"
#include "index/Database.h"
#include "jobs/ThumbnailPipeline.h"
#include "library/Library.h"
#include "platform/win/IoGovernor.h"
#include "platform/win/Paths.h"
#include "platform/win/PowerState.h"
#include "platform/win/ThrottledReader.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QStringList>
#include <QThread>

#include <algorithm>
#include <cmath>

namespace picazoo {

namespace {

qint64 nowMs() { return QDateTime::currentMSecsSinceEpoch(); }

quint64 pairKey(ItemId a, ItemId b)
{
    const ItemId lo = std::min(a, b), hi = std::max(a, b);
    return (quint64(quint32(lo)) << 32) | quint32(hi);
}

QString idList(const QList<ItemId>& ids)
{
    QStringList parts;
    parts.reserve(ids.size());
    for (ItemId id : ids)
        parts << QString::number(id);
    return parts.join(u',');
}

bool aspectMatches(int wa, int ha, int wb, int hb, int rotation)
{
    if (wa <= 0 || ha <= 0 || wb <= 0 || hb <= 0)
        return true; // dimensions inconnues : la vérification fine tranchera
    if (rotation == 90 || rotation == 270)
        std::swap(wb, hb);
    const double ra = double(wa) / ha, rb = double(wb) / hb;
    return std::abs(std::log(ra / rb)) < 0.02;
}

} // namespace

DuplicateEngine::DuplicateEngine(Library& library, QObject* parent) : DuplicateEngine(library, Options{}, parent) {}

DuplicateEngine::DuplicateEngine(Library& library, Options options, QObject* parent)
    : QObject(parent), lib_(library), options_(options)
{
    const QString dbPath = QDir(lib_.dataDir()).filePath(QStringLiteral("library.db"));
    store_ = std::make_unique<DuplicateStore>(dbPath);
    store_->setOnChanged([this] { requestRegroup(); });
    db_ = Database::open(dbPath);
    governor_ = std::make_unique<platform::IoGovernor>();
    if (options_.autoStart) {
        enabled_ = lib_.setting(QStringLiteral("duplicates_enabled")).toInt() != 0;
        userPaused_ = lib_.setting(QStringLiteral("duplicates_paused")).toInt() != 0;
    }

    connect(&lib_, &Library::scanFinished, this, [this] { poke(); });
    connect(&lib_, &Library::itemUpdated, this, [this](qint64) { poke(); });
    connect(&lib_, &Library::itemsChanged, this, [this](qint64) {
        poke();
        requestRegroup();
    });
    connect(&lib_, &Library::foldersChanged, this, [this] { requestRegroup(); });

    notifyTimer_.setInterval(250);
    connect(&notifyTimer_, &QTimer::timeout, this, &DuplicateEngine::flush);
    notifyTimer_.start();

    thread_ = std::thread([this] { loop(); });
}

DuplicateEngine::~DuplicateEngine()
{
    notifyTimer_.stop();
    stop_ = true;
    cv_.notify_all();
    if (thread_.joinable())
        thread_.join();
}

// ------------------------------------------------------------------ Commandes

void DuplicateEngine::poke()
{
    workPending_ = true;
    cv_.notify_all();
}

void DuplicateEngine::setEnabled(bool enabled)
{
    enabled_ = enabled;
    if (options_.autoStart)
        lib_.setSetting(QStringLiteral("duplicates_enabled"), enabled ? 1 : 0);
    progressDirty_ = true;
    poke();
}

void DuplicateEngine::pause()
{
    userPaused_ = true;
    if (options_.autoStart)
        lib_.setSetting(QStringLiteral("duplicates_paused"), 1);
    progressDirty_ = true;
    cv_.notify_all();
}

void DuplicateEngine::resume()
{
    userPaused_ = false;
    if (options_.autoStart)
        lib_.setSetting(QStringLiteral("duplicates_paused"), 0);
    progressDirty_ = true;
    poke();
}

void DuplicateEngine::rescanAll()
{
    fullSearch_ = true;
    regroupRequested_ = true;
    poke();
}

void DuplicateEngine::requestRegroup()
{
    regroupRequested_ = true;
    cv_.notify_all();
}

std::function<void()> DuplicateEngine::forgetWhenRemoved(const QList<ItemId>& ids)
{
    // Le fil du moteur relève la boîte à chaque tour (au plus une seconde d'attente).
    return [inbox = forgetInbox_, ids] {
        std::lock_guard lock(inbox->mutex);
        for (ItemId id : ids)
            inbox->ids.insert(id);
    };
}

void DuplicateEngine::hold() { ++holds_; }

void DuplicateEngine::release()
{
    if (holds_.load() > 0)
        --holds_;
    poke();
}

DuplicateProgress DuplicateEngine::progress() const
{
    std::lock_guard lock(progressMutex_);
    DuplicateProgress p = progress_;
    p.paused = userPaused_.load();
    return p;
}

bool DuplicateEngine::idle() const
{
    if (busy_.load() || workPending_.load() || regroupRequested_.load() || fullSearch_.load())
        return false;
    {
        std::lock_guard lock(forgetInbox_->mutex);
        if (!forgetInbox_->ids.isEmpty())
            return false;
    }
    std::lock_guard lock(progressMutex_);
    return progress_.phase == DuplicateProgress::Phase::Idle;
}

bool DuplicateEngine::waitForIdle(int timeoutMs)
{
    QElapsedTimer t;
    t.start();
    int stable = 0;
    while (t.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        const bool ok = lib_.indexerIdle() && lib_.pipeline().idle() && idle();
        stable = ok ? stable + 1 : 0;
        if (stable >= 5) {
            flush();
            QCoreApplication::processEvents();
            return true;
        }
        QThread::msleep(20);
    }
    return false;
}

void DuplicateEngine::flush()
{
    if (progressDirty_.exchange(false))
        emit progressChanged();
    if (groupsDirty_.exchange(false))
        emit groupsChanged();
}

// ------------------------------------------------------------------ Fil du moteur

bool DuplicateEngine::gatesOpen(QString* reason) const
{
    auto closed = [&](const char* r) {
        if (reason)
            *reason = QString::fromLatin1(r);
        return false;
    };
    if (stop_.load())
        return closed("stopping");
    if (!enabled_.load())
        return closed("disabled");
    if (userPaused_.load())
        return closed("user_paused");
    if (holds_.load() > 0)
        return closed("viewer");
    if (options_.respectBattery && platform::onBatteryOrSaver())
        return closed("on_battery");
    if (!lib_.indexerIdle() || !lib_.pipeline().idle())
        return closed("library_busy"); // un seul gros travail de lecture à la fois (D5-4)
    return true;
}

void DuplicateEngine::loop()
{
    platform::enterBackgroundMode();
    bool loaded = false;
    while (!stop_.load()) {
        QString reason;
        if (!gatesOpen(&reason)) {
            {
                std::lock_guard lock(progressMutex_);
                if (progress_.phase != DuplicateProgress::Phase::Waiting || progress_.reason != reason) {
                    progress_.phase = DuplicateProgress::Phase::Waiting;
                    progress_.reason = reason;
                    progressDirty_ = true;
                }
            }
            libraryWasBusy_ = true;
            std::unique_lock lock(mutex_);
            cv_.wait_for(lock, std::chrono::milliseconds(300));
            continue;
        }
        if (libraryWasBusy_.exchange(false))
            workPending_ = true;

        busy_ = true;
        bool worked = false;
        try {
            if (workPending_.exchange(false))
                worked |= syncRows();
            if (!loaded) {
                loadPrints();
                loaded = true;
            }
            QSet<ItemId> forgotten;
            {
                std::lock_guard lock(forgetInbox_->mutex);
                forgotten.swap(forgetInbox_->ids);
            }
            if (!forgotten.isEmpty()) {
                dropPrints(forgotten);
                fullSearch_ = true; // recherche relancée sur ce qui reste
            }
            const bool c = contentStep();
            const bool v = visualStep();
            if (c || v) {
                worked = true;
                workPending_ = true; // il reste peut-être des lots
            }
            if (fullSearch_.exchange(false)) {
                {
                    std::lock_guard lock(progressMutex_);
                    progress_.phase = DuplicateProgress::Phase::Grouping;
                    progressDirty_ = true;
                }
                search(prints_, true);
                regroupRequested_ = true;
            }
            // La demande reste en attente si les portes viennent de se fermer.
            if (regroupRequested_.load() && gatesOpen(nullptr) && regroupRequested_.exchange(false))
                regroup();
        } catch (const DbError& e) {
            qWarning() << "Doublons (base) :" << e.what();
        } catch (const std::exception& e) {
            qWarning() << "Doublons :" << e.what();
        }
        busy_ = false;

        if (!worked && !workPending_.load() && !regroupRequested_.load() && !fullSearch_.load()) {
            {
                std::lock_guard lock(progressMutex_);
                if (progress_.phase != DuplicateProgress::Phase::Idle) {
                    progress_ = DuplicateProgress{};
                    progressDirty_ = true;
                }
            }
            std::unique_lock lock(mutex_);
            cv_.wait_for(lock, std::chrono::milliseconds(1000), [this] {
                return stop_.load() || workPending_.load() || regroupRequested_.load() || fullSearch_.load();
            });
        }
    }
}

int DuplicateEngine::countPending()
{
    auto q = db_->prepare("SELECT (SELECT COUNT(*) FROM item_fingerprint WHERE content_state = 'pending') +"
                          " (SELECT COUNT(*) FROM item_fingerprint f JOIN item i ON i.id = f.item_id"
                          "  WHERE f.visual_state = 'pending' AND i.thumb_state = 'done' AND i.kind <> 'video')");
    return q.step() ? q.int32(0) : 0;
}

bool DuplicateEngine::syncRows()
{
    std::lock_guard lock(db_->mutex());
    QList<ItemId> stale;
    int changed = 0;
    {
        Transaction tx(*db_);
        auto s = db_->prepare("SELECT f.item_id FROM item_fingerprint f JOIN item i ON i.id = f.item_id"
                              " WHERE f.version <> i.version");
        while (s.step())
            stale << s.int64(0);
        // Nouvelles lignes, ou remise à zéro des éléments dont le contenu a changé (version + 1).
        auto up = db_->prepare(
            "INSERT INTO item_fingerprint (item_id, version, content_state, visual_state, updated_at)"
            " SELECT i.id, i.version, 'not_candidate',"
            "  CASE WHEN i.kind = 'video' THEN 'not_applicable' ELSE 'pending' END, ?"
            " FROM item i LEFT JOIN item_fingerprint f ON f.item_id = i.id"
            " WHERE i.meta_state <> 'pending' AND (f.item_id IS NULL OR f.version <> i.version)"
            " ON CONFLICT(item_id) DO UPDATE SET version = excluded.version, content_state = 'not_candidate',"
            "  content_hash = NULL, visual_state = excluded.visual_state, phash_0 = NULL, phash_90 = NULL,"
            "  phash_180 = NULL, phash_270 = NULL, detail_score = NULL, updated_at = excluded.updated_at");
        up.bind(1, nowMs());
        up.run();
        changed += db_->changes();
        // Candidats aux doublons exacts : même taille et même empreinte rapide qu'un autre élément.
        db_->exec("WITH d AS (SELECT size_bytes, quick_hash FROM item GROUP BY size_bytes, quick_hash"
                  "           HAVING COUNT(*) > 1)"
                  " UPDATE item_fingerprint SET content_state = 'pending'"
                  " WHERE content_state = 'not_candidate' AND item_id IN ("
                  "  SELECT i.id FROM item i JOIN d ON d.size_bytes = i.size_bytes AND d.quick_hash IS i.quick_hash)");
        changed += db_->changes();
        tx.commit();
    }
    if (!stale.isEmpty())
        dropPrints(QSet<ItemId>(stale.begin(), stale.end()));
    pendingTotal_ = countPending();
    doneCount_ = 0;
    if (changed > 0)
        regroupRequested_ = true;
    return changed > 0;
}

// Retire des empreintes, liens visuels et échantillons en mémoire les éléments `gone`
// (contenu changé, ou quittés la bibliothèque).
void DuplicateEngine::dropPrints(const QSet<ItemId>& gone)
{
    if (gone.isEmpty())
        return;
    std::vector<Print> kept;
    kept.reserve(prints_.size());
    for (const Print& p : prints_)
        if (!gone.contains(p.id))
            kept.push_back(p);
    prints_ = std::move(kept);
    printIndex_.clear();
    for (size_t i = 0; i < prints_.size(); ++i)
        printIndex_.insert(prints_[i].id, i);
    for (auto it = edges_.begin(); it != edges_.end();) {
        if (gone.contains(it->a) || gone.contains(it->b))
            it = edges_.erase(it);
        else
            ++it;
    }
    rejected_.clear(); // clés sans identifiants lisibles : on revérifiera au besoin
    for (ItemId id : gone)
        samples_.remove(id);
    regroupRequested_ = true;
}

bool DuplicateEngine::contentStep()
{
    struct Job {
        ItemId id;
        int version;
        QString path;
        QByteArray knownHash; // empreinte d'un autre chemin vers le même fichier (lien physique)
        QByteArray fileKey;   // volume + identifiant NTFS, vide si inconnu
    };
    std::vector<Job> jobs;
    {
        std::lock_guard lock(db_->mutex());
        auto q = db_->prepare(
            "SELECT f.item_id, f.version, fo.path, i.file_name,"
            " (SELECT f2.content_hash FROM item i2 JOIN item_fingerprint f2 ON f2.item_id = i2.id"
            "  WHERE i.file_id IS NOT NULL AND i2.file_id = i.file_id AND i2.volume_serial IS i.volume_serial"
            "  AND i2.size_bytes = i.size_bytes AND i2.mtime = i.mtime AND f2.content_state = 'done'"
            "  AND f2.version = i2.version LIMIT 1), i.volume_serial, i.file_id"
            " FROM item_fingerprint f JOIN item i ON i.id = f.item_id JOIN folder fo ON fo.id = i.folder_id"
            " WHERE f.content_state = 'pending' AND f.version = i.version"
            " ORDER BY i.size_bytes LIMIT 16");
        while (q.step())
            jobs.push_back(Job{q.int64(0), q.int32(1), platform::joinPath(q.text(2), q.text(3)),
                               q.isNull(4) ? QByteArray() : q.blob(4),
                               q.isNull(6) ? QByteArray() : QByteArray::number(q.int64(5)) + ':' + q.blob(6)});
    }
    QHash<QByteArray, QByteArray> hashedInBatch; // liens physiques d'un même lot : une seule lecture
    bool did = false;
    for (const Job& job : jobs) {
        if (!gatesOpen(nullptr))
            break;
        {
            std::lock_guard lock(progressMutex_);
            progress_.phase = DuplicateProgress::Phase::Content;
            progress_.reason.clear();
            progress_.done = doneCount_;
            progress_.total = std::max(pendingTotal_, doneCount_ + 1);
            progressDirty_ = true;
        }
        platform::ContentHash r;
        const QByteArray known = !job.knownHash.isEmpty() ? job.knownHash : hashedInBatch.value(job.fileKey);
        if (!known.isEmpty() && !job.fileKey.isEmpty()) {
            r.status = platform::ReadStatus::Ok; // même fichier : rien à relire
            r.hash = known;
        } else {
            platform::ReadControl ctl;
            ctl.governor = governor_.get();
            ctl.cancelled = [this] { return !gatesOpen(nullptr); };
            r = platform::hashFileContent(job.path, ctl);
            if (r.status == platform::ReadStatus::Ok && !job.fileKey.isEmpty())
                hashedInBatch.insert(job.fileKey, r.hash);
        }
        if (r.status == platform::ReadStatus::Cancelled)
            break;
        const char* state = r.status == platform::ReadStatus::Ok           ? "done"
                            : r.status == platform::ReadStatus::OnlineOnly ? "skipped_online"
                                                                           : "failed";
        {
            std::lock_guard lock(db_->mutex());
            auto u = db_->prepare("UPDATE item_fingerprint SET content_state = ?, content_hash = ?, updated_at = ?"
                                  " WHERE item_id = ? AND version = ?");
            u.bind(1, QString::fromLatin1(state));
            if (r.status == platform::ReadStatus::Ok)
                u.bind(2, r.hash);
            else
                u.bindNull(2);
            u.bind(3, nowMs()).bind(4, job.id).bind(5, job.version);
            u.run();
        }
        if (r.status == platform::ReadStatus::Ok) {
            auto it = printIndex_.constFind(job.id);
            if (it != printIndex_.constEnd())
                prints_[*it].contentHash = r.hash;
            regroupRequested_ = true;
        }
        ++doneCount_;
        did = true;
    }
    return did;
}

bool DuplicateEngine::visualStep()
{
    struct Job {
        ItemId id;
        int version;
        int width, height;
        std::optional<qint64> takenAt;
        QByteArray contentHash;
    };
    std::vector<Job> jobs;
    {
        std::lock_guard lock(db_->mutex());
        auto q = db_->prepare("SELECT f.item_id, i.version, i.width, i.height, i.taken_at, f.content_hash"
                              " FROM item_fingerprint f JOIN item i ON i.id = f.item_id"
                              " WHERE f.visual_state = 'pending' AND f.version = i.version"
                              " AND i.thumb_state = 'done' AND i.kind <> 'video' LIMIT 100");
        while (q.step())
            jobs.push_back(Job{q.int64(0), q.int32(1), q.isNull(2) ? 0 : q.int32(2), q.isNull(3) ? 0 : q.int32(3),
                               q.optInt64(4), q.isNull(5) ? QByteArray() : q.blob(5)});
    }
    if (jobs.empty())
        return false;
    std::vector<Print> news;
    struct Result {
        ItemId id;
        int version;
        const char* state;
        VisualPrint print;
    };
    std::vector<Result> results;
    for (const Job& job : jobs) {
        if (!gatesOpen(nullptr))
            break;
        {
            std::lock_guard lock(progressMutex_);
            progress_.phase = DuplicateProgress::Phase::Visual;
            progress_.reason.clear();
            progress_.done = doneCount_;
            progress_.total = std::max(pendingTotal_, doneCount_ + 1);
            progressDirty_ = true;
        }
        const std::optional<QImage> img = lib_.thumbnail(job.id, job.version);
        Result r{job.id, job.version, "failed", {}};
        if (img && !img->isNull()) {
            r.print = VisualFingerprint::compute(*img);
            if (r.print.ok) {
                r.state = r.print.detail < VisualFingerprint::kLowDetail ? "low_detail" : "done";
                if (r.state[0] == 'd') {
                    samples_.insert(job.id, new VisualSample(PairVerifier::sample(*img)));
                    Print p;
                    p.id = job.id;
                    p.version = job.version;
                    p.hash = r.print.phash;
                    p.width = job.width;
                    p.height = job.height;
                    p.takenAt = job.takenAt;
                    p.contentHash = job.contentHash;
                    news.push_back(p);
                }
            }
        }
        results.push_back(r);
        ++doneCount_;
    }
    {
        std::lock_guard lock(db_->mutex());
        Transaction tx(*db_);
        auto u = db_->prepare("UPDATE item_fingerprint SET visual_state = ?, phash_0 = ?, phash_90 = ?, phash_180 = ?,"
                              " phash_270 = ?, detail_score = ?, updated_at = ? WHERE item_id = ? AND version = ?");
        for (const Result& r : results) {
            u.bind(1, QString::fromLatin1(r.state));
            if (r.print.ok) {
                for (int k = 0; k < 4; ++k)
                    u.bind(2 + k, qint64(r.print.phash[size_t(k)]));
                u.bind(6, qint64(std::llround(r.print.detail * 1000)));
            } else {
                for (int k = 2; k <= 6; ++k)
                    u.bindNull(k);
            }
            u.bind(7, nowMs()).bind(8, r.id).bind(9, r.version);
            u.run();
        }
        tx.commit();
    }
    if (!news.empty()) {
        search(news, false);
        for (const Print& p : news) {
            printIndex_.insert(p.id, prints_.size());
            prints_.push_back(p);
        }
        regroupRequested_ = true;
    }
    return !results.empty();
}

void DuplicateEngine::loadPrints()
{
    prints_.clear();
    printIndex_.clear();
    {
        std::lock_guard lock(db_->mutex());
        auto q = db_->prepare("SELECT f.item_id, i.version, f.phash_0, f.phash_90, f.phash_180, f.phash_270,"
                              " i.width, i.height, i.taken_at, f.content_hash"
                              " FROM item_fingerprint f JOIN item i ON i.id = f.item_id"
                              " WHERE f.visual_state = 'done' AND f.version = i.version");
        while (q.step()) {
            Print p;
            p.id = q.int64(0);
            p.version = q.int32(1);
            for (int k = 0; k < 4; ++k)
                p.hash[size_t(k)] = quint64(q.int64(2 + k));
            p.width = q.isNull(6) ? 0 : q.int32(6);
            p.height = q.isNull(7) ? 0 : q.int32(7);
            p.takenAt = q.optInt64(8);
            if (!q.isNull(9))
                p.contentHash = q.blob(9);
            printIndex_.insert(p.id, prints_.size());
            prints_.push_back(p);
        }
        forbidden_.clear();
        auto d = db_->prepare("SELECT item_a, item_b FROM dup_decision WHERE kind = 'not_duplicate'");
        while (d.step())
            forbidden_.insert(pairKey(d.int64(0), d.int64(1)));
    }
    fullSearch_ = true;
}

std::optional<VisualSample> DuplicateEngine::sampleFor(const Print& p)
{
    if (const VisualSample* s = samples_.object(p.id))
        return *s;
    const std::optional<QImage> img = lib_.thumbnail(p.id, p.version);
    if (!img || img->isNull())
        return std::nullopt;
    VisualSample s = PairVerifier::sample(*img);
    samples_.insert(p.id, new VisualSample(s));
    return s;
}

std::optional<VisualEdge> DuplicateEngine::verifyPair(const Print& a, const Print& b, int rotation)
{
    const std::optional<VisualSample> sa = sampleFor(a);
    const std::optional<VisualSample> sb = sampleFor(b);
    if (!sa || !sb)
        return std::nullopt;
    const PairVerdict v = PairVerifier::verify(*sa, *sb, rotation, a.takenAt, b.takenAt);
    if (!v.match)
        return std::nullopt;
    return VisualEdge{a.id, b.id, v.level, v.ssim, rotation};
}

void DuplicateEngine::search(const std::vector<Print>& news, bool againstAll)
{
    auto consider = [&](const Print& a, const Print& b) {
        if (a.id == b.id)
            return;
        if (!a.contentHash.isEmpty() && a.contentHash == b.contentHash)
            return; // identiques : regroupées par leur contenu
        if (PairVerifier::distinctShots(a.takenAt, b.takenAt))
            return; // deux prises de vue différentes (rafale, série)
        int rotation = 0;
        if (VisualFingerprint::bestDistance(a.hash[0], b.hash, &rotation) > VisualFingerprint::kMaxDistance)
            return;
        if (!aspectMatches(a.width, a.height, b.width, b.height, rotation))
            return;
        const quint64 k = pairKey(a.id, b.id);
        if (edges_.contains(k) || rejected_.contains(k) || forbidden_.contains(k))
            return;
        if (auto e = verifyPair(a, b, rotation))
            edges_.insert(k, *e);
        else
            rejected_.insert(k);
    };
    if (againstAll) {
        for (size_t i = 0; i < prints_.size(); ++i) {
            if (i % 256 == 0 && !gatesOpen(nullptr)) {
                fullSearch_ = true; // reprendra plus tard ; le travail vérifié est gardé en cache
                return;
            }
            for (size_t j = i + 1; j < prints_.size(); ++j)
                consider(prints_[i], prints_[j]);
        }
        return;
    }
    for (size_t n = 0; n < news.size(); ++n) {
        for (const Print& p : prints_)
            consider(news[n], p);
        for (size_t m = n + 1; m < news.size(); ++m)
            consider(news[n], news[m]);
    }
}

void DuplicateEngine::regroup()
{
    {
        std::lock_guard lock(progressMutex_);
        progress_.phase = DuplicateProgress::Phase::Grouping;
        progressDirty_ = true;
    }
    std::lock_guard lock(db_->mutex());

    // 1) Ensembles identiques, liens physiques fusionnés (FR-006).
    QList<QList<ItemId>> exactSets;
    {
        auto q = db_->prepare(
            "SELECT f.content_hash, f.item_id, i.volume_serial, i.file_id FROM item_fingerprint f"
            " JOIN item i ON i.id = f.item_id"
            " WHERE f.content_state = 'done' AND f.version = i.version AND f.content_hash IN ("
            "  SELECT content_hash FROM item_fingerprint WHERE content_state = 'done'"
            "  GROUP BY content_hash HAVING COUNT(*) > 1)"
            " ORDER BY f.content_hash, f.item_id");
        QByteArray current;
        QList<ItemId> set;
        QSet<QByteArray> files;
        auto close = [&] {
            if (set.size() >= 2)
                exactSets << set;
            set.clear();
            files.clear();
        };
        while (q.step()) {
            const QByteArray h = q.blob(0);
            if (h != current) {
                close();
                current = h;
            }
            if (!q.isNull(3)) {
                const QByteArray fileKey = QByteArray::number(q.int64(2)) + ':' + q.blob(3);
                if (files.contains(fileKey))
                    continue; // même fichier sous un autre chemin
                files.insert(fileKey);
            }
            set << q.int64(1);
        }
        close();
    }

    // 2) Décisions de l'utilisateur.
    forbidden_.clear();
    QSet<quint64> keepAllPairs;
    {
        auto d = db_->prepare("SELECT item_a, item_b, kind FROM dup_decision");
        while (d.step()) {
            const quint64 k = pairKey(d.int64(0), d.int64(1));
            if (d.text(2) == u"not_duplicate")
                forbidden_.insert(k);
            else
                keepAllPairs.insert(k);
        }
    }

    // 3) Faits sur les copies concernées.
    QSet<ItemId> involved;
    for (const auto& s : exactSets)
        for (ItemId id : s)
            involved.insert(id);
    for (const VisualEdge& e : std::as_const(edges_)) {
        involved.insert(e.a);
        involved.insert(e.b);
    }
    QHash<ItemId, CopyFacts> facts;
    QHash<ItemId, qint64> sizes;
    QHash<QByteArray, ItemId> fileRep;    // fichier physique -> plus petit identifiant
    QHash<ItemId, QByteArray> fileKeyOf;
    const QList<ItemId> all(involved.begin(), involved.end());
    for (qsizetype off = 0; off < all.size(); off += 500) {
        const QList<ItemId> chunk = all.mid(off, 500);
        const QString sql = QStringLiteral("SELECT i.id, i.kind, i.width, i.height, i.size_bytes, i.taken_at,"
                                           " i.sort_date, fo.path, i.file_name, i.volume_serial, i.file_id FROM item i"
                                           " JOIN folder fo ON fo.id = i.folder_id WHERE i.id IN (") +
                            idList(chunk) + u')';
        auto q = db_->prepare(sql.toUtf8().constData());
        while (q.step()) {
            CopyFacts f;
            f.id = q.int64(0);
            f.isRaw = q.text(1) == u"raw";
            f.pixels = qint64(q.isNull(2) ? 0 : q.int32(2)) * (q.isNull(3) ? 0 : q.int32(3));
            f.sizeBytes = q.int64(4);
            f.hasTakenAt = !q.isNull(5);
            f.date = q.int64(6);
            f.path = platform::joinPath(q.text(7), q.text(8));
            facts.insert(f.id, f);
            sizes.insert(f.id, f.sizeBytes);
            if (!q.isNull(10)) {
                const QByteArray fileKey = QByteArray::number(q.int64(9)) + ':' + q.blob(10);
                auto it = fileRep.find(fileKey);
                if (it == fileRep.end())
                    fileRep.insert(fileKey, f.id);
                else if (f.id < *it)
                    *it = f.id;
                fileKeyOf.insert(f.id, fileKey);
            }
        }
    }
    // Liens physiques (FR-006) : un seul représentant par fichier, les autres chemins sont ignorés.
    auto sameFileAlias = [&](ItemId id) {
        const auto k = fileKeyOf.constFind(id);
        return k != fileKeyOf.constEnd() && fileRep.value(*k) != id;
    };
    for (auto& s : exactSets)
        s.erase(std::remove_if(s.begin(), s.end(), sameFileAlias), s.end());
    // Éléments disparus de l'index : on les oublie.
    for (auto& s : exactSets)
        s.erase(std::remove_if(s.begin(), s.end(), [&](ItemId id) { return !facts.contains(id); }), s.end());
    exactSets.erase(std::remove_if(exactSets.begin(), exactSets.end(), [](const QList<ItemId>& s) { return s.size() < 2; }),
                    exactSets.end());
    QList<VisualEdge> edges;
    for (auto it = edges_.begin(); it != edges_.end();) {
        if (!facts.contains(it->a) || !facts.contains(it->b)) {
            it = edges_.erase(it);
            continue;
        }
        if (sameFileAlias(it->a) || sameFileAlias(it->b)) {
            ++it; // gardée en cache : le représentant peut changer
            continue;
        }
        edges << *it;
        ++it;
    }

    const QList<BuiltGroup> groups = Grouper::build(
        facts, exactSets, edges, [this](ItemId a, ItemId b) { return forbidden_.contains(pairKey(a, b)); });

    // 4) Écriture : identifiants de groupe conservés quand le centre ne change pas.
    Transaction tx(*db_);
    QHash<ItemId, GroupId> oldIds;
    GroupId nextId = 1;
    {
        auto q = db_->prepare("SELECT id, center_item_id FROM dup_group");
        while (q.step()) {
            oldIds.insert(q.int64(1), q.int64(0));
            nextId = std::max(nextId, q.int64(0) + 1);
        }
    }
    QHash<ItemId, int> choices;
    {
        auto q = db_->prepare("SELECT item_id, keep FROM dup_choice");
        while (q.step())
            choices.insert(q.int64(0), q.int32(1));
    }
    db_->exec("DELETE FROM dup_member");
    db_->exec("DELETE FROM dup_group");
    auto ins = db_->prepare("INSERT INTO dup_group (id, center_item_id, keeper_reason, level, member_count,"
                            " reclaimable_bytes, state, updated_at) VALUES (?, ?, ?, ?, ?, 0, ?, ?)");
    auto mem = db_->prepare("INSERT INTO dup_member (group_id, item_id, level, similarity, rotation) VALUES (?, ?, ?, ?, ?)");
    auto rec = db_->prepare(kReclaimableSql);
    auto clearChoice = db_->prepare("DELETE FROM dup_choice WHERE item_id = ?");
    for (const BuiltGroup& g : groups) {
        bool keptAll = true;
        for (const GroupMember& m : g.members)
            if (m.item != g.center && !keepAllPairs.contains(pairKey(m.item, g.center)))
                keptAll = false;
        // Les choix enregistrés ne doivent jamais écarter toutes les copies (FR-017).
        bool anyKept = false;
        for (const GroupMember& m : g.members) {
            const auto c = choices.constFind(m.item);
            anyKept |= c != choices.constEnd() ? *c != 0 : m.item == g.center;
        }
        if (!anyKept) {
            for (const GroupMember& m : g.members) {
                clearChoice.bind(1, m.item);
                clearChoice.run();
            }
        }
        // Identifiant conservé si le centre ne change pas, sinon un identifiant neuf (jamais un
        // ancien identifiant qu'un autre groupe pourrait reprendre).
        const GroupId oldId = oldIds.value(g.center, 0);
        const GroupId gid = oldId ? oldId : nextId++;
        ins.bind(1, gid);
        ins.bind(2, g.center)
            .bind(3, toDb(g.reason))
            .bind(4, toDb(g.level))
            .bind(5, int(g.members.size()))
            .bind(6, toDb(keptAll ? GroupState::KeptAll : GroupState::ToReview))
            .bind(7, nowMs());
        ins.run();
        for (const GroupMember& m : g.members) {
            mem.bind(1, gid).bind(2, m.item).bind(3, toDb(m.level));
            if (m.similarity)
                mem.bind(4, qint64(std::llround(*m.similarity * 1e6)));
            else
                mem.bindNull(4);
            mem.bind(5, m.rotation);
            mem.run();
        }
        rec.bind(1, gid);
        rec.run();
    }
    // 006 : copie suggérée (qualité + règles de dossier), place récupérable, garde-fou FR-017.
    Suggestion::apply(*db_);
    tx.commit();
    groupsDirty_ = true;
}

} // namespace picazoo

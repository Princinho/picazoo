#pragma once

// Pipeline des métadonnées et des vignettes (research R6, R12 ; T038).
// 1) vignette intégrée -> provisional ; 2) vignette finale 320 px -> done.
// Les résultats sont écrits par lots (une transaction toutes les 200 ms au plus).

#include "jobs/MemoryBudget.h"
#include "jobs/PriorityQueue.h"
#include "library/Types.h"

#include <QImage>
#include <QList>
#include <QSet>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

namespace picazoo {

class Database;
class LibraryDb;
class ThumbStore;
struct Item;

constexpr int kThumbSize = 320;

class ThumbnailPipeline {
public:
    using Notify = std::function<void(const QList<ItemId>& updated, const QSet<FolderId>& folders)>;
    // Production de la vignette d'une vidéo (US3). Renvoie l'image et la durée, ou rien.
    struct VideoThumb {
        QImage image;
        std::optional<qint64> durationMs;
    };
    using VideoHandler = std::function<std::optional<VideoThumb>(const Item&)>;

    ThumbnailPipeline(Database& readDb, LibraryDb& writer, ThumbStore& thumbs, Notify notify);
    ~ThumbnailPipeline();

    void start(int threads);
    void stop();

    // Budget mémoire du scan (008, research B3) : ajuste le nombre de fils de calcul
    // actifs. Une baisse réduit progressivement le parallélisme (les fils en excès
    // terminent leur tâche en cours puis s'arrêtent) ; une hausse ne démarre pas de
    // nouveau fil avant le prochain `start()`, pour éviter les oscillations pendant
    // un même scan. Jamais en dessous de 1 fil actif.
    void applyMemoryBudget(const MemoryBudget& budget);

    void enqueue(const QList<ItemId>& ids, Priority p = Priority::Background);
    void prioritize(const QList<ItemId>& visible, const QList<ItemId>& nearby);
    void remove(const QList<ItemId>& ids);
    void pause();
    void resume();
    bool paused() const;
    // Suspension temporaire (décodage de l'image affichée dans la visionneuse), cumulable.
    void hold();
    void release();

    void setVideoHandler(VideoHandler h);
    bool handlesVideos() const;

    size_t pending() const;
    qint64 processed() const { return processed_.load(); }
    // Rien en attente, rien en cours, tout écrit.
    bool idle() const;

private:
    void workerLoop(int index);
    void writerLoop();
    void process(const QueuedItem& job);
    void flush();

    Database& readDb_;
    LibraryDb& writer_;
    ThumbStore& thumbs_;
    Notify notify_;
    VideoHandler video_;
    mutable std::mutex videoMutex_;

    PriorityQueue queue_;
    std::vector<std::thread> workers_;
    std::thread writerThread_;
    std::atomic<int> inFlight_{0};
    std::atomic<qint64> processed_{0};
    std::atomic<bool> stopping_{false};
    std::mutex holdMutex_;
    int holds_ = 0;
    bool userPaused_ = false;
    void applyPause();

    // Budget mémoire (008) : combien de fils, parmi `workers_`, doivent rester actifs.
    // Un fil d'index >= activeThreads_ termine sa tâche en cours puis sort de sa boucle.
    std::atomic<int> activeThreads_{1};

    struct Pending;
    std::unique_ptr<Pending> pending_;
    mutable std::mutex pendingMutex_;
    std::condition_variable pendingCv_;
};

} // namespace picazoo

#pragma once

// Pipeline des métadonnées et des vignettes (research R6, R12 ; T038).
// 1) vignette intégrée -> provisional ; 2) vignette finale 320 px -> done.
// Les résultats sont écrits par lots (une transaction toutes les 200 ms au plus).

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
    void workerLoop();
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

    struct Pending;
    std::unique_ptr<Pending> pending_;
    mutable std::mutex pendingMutex_;
    std::condition_variable pendingCv_;
};

} // namespace picazoo

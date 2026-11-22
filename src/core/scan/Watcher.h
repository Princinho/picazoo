#pragma once

// Suivi des changements en direct (research R5, T068) : événements regroupés pendant
// 500 ms ; un fichier en cours de copie (taille ou date qui change) est réessayé jusqu'à
// stabilité (1 s) ; débordement -> parcours de la racine ; racine perdue -> indisponible.

#include "scan/Discovery.h"

#include <QHash>
#include <QString>
#include <QStringList>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace picazoo {

namespace platform {
class DirectoryWatcher;
}

class Watcher {
public:
    struct Batch {
        QHash<RootId, QStringList> paths; // chemins stables, à réexaminer
        QList<RootId> overflow;           // racines à reparcourir
        QList<RootId> lost;               // racines devenues inaccessibles
    };
    using Sink = std::function<void(const Batch&)>;

    explicit Watcher(Sink sink);
    ~Watcher();

    // Racines effectives à surveiller ; les surveillances existantes sont conservées.
    void setRoots(const std::vector<RootSpec>& roots);
    bool watching(RootId id) const;

    static constexpr auto kDebounce = std::chrono::milliseconds(500);
    static constexpr auto kStability = std::chrono::milliseconds(1000);

private:
    using Clock = std::chrono::steady_clock;
    struct Pending {
        RootId root = 0;
        Clock::time_point firstSeen;
        qint64 size = -1;
        qint64 mtime = -1;
        bool checked = false;
    };
    void onEvent(RootId root, const QStringList& paths, bool overflow, bool lost);
    void flusherLoop();

    Sink sink_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    QHash<RootId, std::shared_ptr<platform::DirectoryWatcher>> watchers_;
    QHash<QString, Pending> pending_; // clé : chemin en minuscules
    QHash<QString, QString> originalPath_;
    QList<RootId> overflow_;
    QList<RootId> lost_;
    Clock::time_point lastEvent_{};
    std::thread flusher_;
    bool stop_ = false;
};

} // namespace picazoo

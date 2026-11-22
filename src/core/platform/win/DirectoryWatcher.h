#pragma once

// Surveillance d'une racine (research R5, T067) : un ReadDirectoryChangesW récursif en
// E/S asynchrones sur un thread dédié. Tampon de 64 Ko ; un débordement est signalé
// pour que l'appelant refasse un parcours de la racine.

#include <QString>
#include <QStringList>

#include <atomic>
#include <functional>
#include <thread>

namespace picazoo::platform {

class DirectoryWatcher {
public:
    struct Event {
        QStringList paths;  // chemins absolus touchés (fichiers ou dossiers)
        bool overflow = false;
        bool lost = false;  // racine devenue inaccessible (disque débranché…)
    };
    using Callback = std::function<void(const Event&)>;

    DirectoryWatcher(QString root, Callback cb);
    ~DirectoryWatcher();
    DirectoryWatcher(const DirectoryWatcher&) = delete;
    DirectoryWatcher& operator=(const DirectoryWatcher&) = delete;

    bool start();
    void stop();
    const QString& root() const { return root_; }

private:
    void loop();

    QString root_;
    Callback cb_;
    void* dir_ = nullptr;       // HANDLE
    void* stopEvent_ = nullptr; // HANDLE
    std::thread thread_;
    std::atomic<bool> running_{false};
};

} // namespace picazoo::platform

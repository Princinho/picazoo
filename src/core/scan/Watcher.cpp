#include "scan/Watcher.h"

#include "platform/win/DirectoryWatcher.h"
#include "platform/win/DirectoryWalker.h"
#include "platform/win/Paths.h"

#include <windows.h>

namespace picazoo {

namespace {

// Taille et date d'un fichier sans l'ouvrir ; vide si c'est un dossier ou s'il n'existe pas.
bool statFile(const QString& path, qint64& size, qint64& mtime, bool& isDir)
{
    WIN32_FILE_ATTRIBUTE_DATA d{};
    const std::wstring w = platform::toWin32(path);
    if (!GetFileAttributesExW(w.c_str(), GetFileExInfoStandard, &d))
        return false;
    isDir = (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    size = (qint64(d.nFileSizeHigh) << 32) | d.nFileSizeLow;
    mtime = platform::fileTimeToEpochMs(d.ftLastWriteTime.dwLowDateTime, d.ftLastWriteTime.dwHighDateTime);
    return true;
}

} // namespace

Watcher::Watcher(Sink sink) : sink_(std::move(sink))
{
    flusher_ = std::thread([this] { flusherLoop(); });
}

Watcher::~Watcher()
{
    {
        std::lock_guard lock(mutex_);
        stop_ = true;
    }
    cv_.notify_all();
    if (flusher_.joinable())
        flusher_.join();
    QHash<RootId, std::shared_ptr<platform::DirectoryWatcher>> w;
    {
        std::lock_guard lock(mutex_);
        w.swap(watchers_);
    }
    w.clear(); // arrête les threads de surveillance hors du verrou
}

bool Watcher::watching(RootId id) const
{
    std::lock_guard lock(mutex_);
    return watchers_.contains(id);
}

void Watcher::setRoots(const std::vector<RootSpec>& roots)
{
    QHash<RootId, std::shared_ptr<platform::DirectoryWatcher>> keep;
    QList<std::shared_ptr<platform::DirectoryWatcher>> drop;
    {
        std::lock_guard lock(mutex_);
        for (auto it = watchers_.begin(); it != watchers_.end(); ++it) {
            const bool wanted = std::any_of(roots.begin(), roots.end(), [&](const RootSpec& r) {
                return r.id == it.key() && platform::samePath(r.path, it.value()->root());
            });
            if (wanted)
                keep.insert(it.key(), it.value());
            else
                drop << it.value();
        }
    }
    drop.clear(); // arrêt hors verrou
    for (const auto& r : roots) {
        if (keep.contains(r.id))
            continue;
        const RootId id = r.id;
        auto w = std::make_shared<platform::DirectoryWatcher>(
            r.path, [this, id](const platform::DirectoryWatcher::Event& e) { onEvent(id, e.paths, e.overflow, e.lost); });
        if (w->start())
            keep.insert(id, w);
    }
    std::lock_guard lock(mutex_);
    watchers_ = std::move(keep);
}

void Watcher::onEvent(RootId root, const QStringList& paths, bool overflow, bool lost)
{
    {
        std::lock_guard lock(mutex_);
        const auto now = Clock::now();
        lastEvent_ = now;
        if (overflow && !overflow_.contains(root))
            overflow_ << root;
        if (lost && !lost_.contains(root))
            lost_ << root;
        for (const QString& p : paths) {
            const QString key = p.toLower();
            auto it = pending_.find(key);
            if (it == pending_.end()) {
                pending_.insert(key, Pending{root, now});
                originalPath_.insert(key, p);
            } else {
                it->checked = false; // nouvel événement : la stabilité doit être revérifiée
            }
        }
    }
    cv_.notify_all();
}

void Watcher::flusherLoop()
{
    std::unique_lock lock(mutex_);
    while (!stop_) {
        cv_.wait_for(lock, std::chrono::milliseconds(100));
        if (stop_)
            break;
        const auto now = Clock::now();
        if (pending_.isEmpty() && overflow_.isEmpty() && lost_.isEmpty())
            continue;
        if (now - lastEvent_ < kDebounce)
            continue;

        Batch batch;
        batch.overflow = std::exchange(overflow_, {});
        batch.lost = std::exchange(lost_, {});
        for (auto it = pending_.begin(); it != pending_.end();) {
            Pending& p = it.value();
            const QString path = originalPath_.value(it.key());
            qint64 size = -1, mtime = -1;
            bool isDir = false;
            const bool exists = statFile(path, size, mtime, isDir);
            bool ready = !exists || isDir;
            if (!ready) {
                // Fichier : prêt quand taille et date n'ont pas bougé depuis kStability.
                if (p.checked && p.size == size && p.mtime == mtime && now - p.firstSeen >= kStability) {
                    ready = true;
                } else if (!p.checked || p.size != size || p.mtime != mtime) {
                    p.size = size;
                    p.mtime = mtime;
                    p.checked = true;
                    p.firstSeen = now;
                }
            }
            if (ready) {
                batch.paths[p.root] << path;
                originalPath_.remove(it.key());
                it = pending_.erase(it);
            } else {
                ++it;
            }
        }
        if (batch.paths.isEmpty() && batch.overflow.isEmpty() && batch.lost.isEmpty())
            continue;
        lock.unlock();
        sink_(batch);
        lock.lock();
    }
}

} // namespace picazoo

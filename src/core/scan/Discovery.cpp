#include "scan/Discovery.h"

#include "media/Formats.h"
#include "platform/win/DirectoryWalker.h"
#include "platform/win/Paths.h"

#include <algorithm>
#include <deque>

namespace picazoo {

std::vector<RootSpec> Discovery::effectiveRoots(std::vector<RootSpec> roots)
{
    for (auto& r : roots)
        r.path = platform::normalizePath(r.path);
    std::sort(roots.begin(), roots.end(), [](const RootSpec& a, const RootSpec& b) {
        return a.path.size() < b.path.size();
    });
    std::vector<RootSpec> out;
    for (auto& r : roots) {
        auto cover = std::find_if(out.begin(), out.end(), [&](const RootSpec& o) {
            return platform::isInside(r.path, o.path);
        });
        if (cover == out.end()) {
            out.push_back(r);
        } else {
            for (const QString& e : r.excluded) {
                if (!cover->excluded.contains(e, Qt::CaseInsensitive))
                    cover->excluded << e;
            }
        }
    }
    return out;
}

bool Discovery::isExcluded(const QString& path, const QStringList& excluded)
{
    return std::any_of(excluded.begin(), excluded.end(),
                       [&](const QString& e) { return platform::isInside(path, e); });
}

bool Discovery::listFolder(RootId root, const QString& path, DiscoveredFolder& out, QStringList* subdirs)
{
    std::vector<platform::DirEntry> entries;
    if (platform::listDirectory(path, entries) != platform::ListResult::Ok)
        return false;
    out.rootId = root;
    out.path = path;
    out.files.clear();
    for (auto& e : entries) {
        if (e.isDir) {
            if (subdirs)
                *subdirs << platform::joinPath(path, e.name);
            continue;
        }
        const Format f = formatFromPath(e.name);
        if (f == Format::Unknown)
            continue;
        out.files.push_back(DiscoveredFile{std::move(e.name), e.size, e.mtime, f});
    }
    return true;
}

RootState Discovery::walkRoot(const RootSpec& root, const DiscoveryCallbacks& cb)
{
    switch (platform::probeDirectory(root.path)) {
    case platform::ListResult::NotFound: return RootState::Unavailable;
    case platform::ListResult::Denied: return RootState::Denied;
    case platform::ListResult::Error: return RootState::Unavailable;
    case platform::ListResult::Ok: break;
    }

    std::deque<QString> todo{root.path};
    while (!todo.empty()) {
        if (cb.cancelled && cb.cancelled())
            return RootState::Available;
        if (cb.waitIfPaused)
            cb.waitIfPaused();
        const QString dir = todo.front();
        todo.pop_front();
        if (isExcluded(dir, root.excluded))
            continue;
        DiscoveredFolder folder;
        QStringList subdirs;
        if (!listFolder(root.id, dir, folder, &subdirs)) {
            if (cb.onFolderFailed)
                cb.onFolderFailed(dir);
            continue;
        }
        std::sort(subdirs.begin(), subdirs.end(), [](const QString& a, const QString& b) {
            return a.compare(b, Qt::CaseInsensitive) < 0;
        });
        for (const QString& s : subdirs)
            todo.push_back(s);
        if (!folder.files.empty() && cb.onFolder)
            cb.onFolder(folder);
    }
    return RootState::Available;
}

} // namespace picazoo

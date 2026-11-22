#include "platform/win/DirectoryWalker.h"

#include "platform/win/Paths.h"

#include <windows.h>

namespace picazoo::platform {

qint64 fileTimeToEpochMs(quint32 low, quint32 high)
{
    const quint64 t = (quint64(high) << 32) | low;
    constexpr quint64 kEpochDiff = 116444736000000000ULL; // 1601 -> 1970, en 100 ns
    if (t < kEpochDiff)
        return 0;
    return qint64((t - kEpochDiff) / 10000);
}

ListResult listDirectory(const QString& dir, std::vector<DirEntry>& out)
{
    out.clear();
    const std::wstring pattern = toWin32(joinPath(dir, QStringLiteral("*")));
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileExW(pattern.c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr,
                                FIND_FIRST_EX_LARGE_FETCH);
    if (h == INVALID_HANDLE_VALUE) {
        switch (GetLastError()) {
        case ERROR_FILE_NOT_FOUND: return ListResult::Ok; // dossier vide
        case ERROR_PATH_NOT_FOUND:
        case ERROR_INVALID_NAME:
        case ERROR_BAD_NETPATH:
        case ERROR_BAD_NET_NAME:
        case ERROR_NOT_READY:
        case ERROR_INVALID_DRIVE:
            return ListResult::NotFound;
        case ERROR_ACCESS_DENIED: return ListResult::Denied;
        default: return ListResult::Error;
        }
    }
    do {
        const DWORD a = fd.dwFileAttributes;
        if (a & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_REPARSE_POINT))
            continue;
        const wchar_t* n = fd.cFileName;
        if (n[0] == L'.' && (n[1] == 0 || (n[1] == L'.' && n[2] == 0)))
            continue;
        DirEntry e;
        e.name = QString::fromWCharArray(n);
        e.isDir = (a & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (!e.isDir) {
            if (e.name.compare(u"Thumbs.db", Qt::CaseInsensitive) == 0 ||
                e.name.compare(u"desktop.ini", Qt::CaseInsensitive) == 0)
                continue;
            e.size = (qint64(fd.nFileSizeHigh) << 32) | fd.nFileSizeLow;
        }
        e.mtime = fileTimeToEpochMs(fd.ftLastWriteTime.dwLowDateTime, fd.ftLastWriteTime.dwHighDateTime);
        out.push_back(std::move(e));
    } while (FindNextFileW(h, &fd));
    const DWORD err = GetLastError();
    FindClose(h);
    return err == ERROR_NO_MORE_FILES ? ListResult::Ok : ListResult::Error;
}

bool pathExists(const QString& path, bool* isDir)
{
    const std::wstring w = toWin32(path);
    const DWORD a = GetFileAttributesW(w.c_str());
    if (a == INVALID_FILE_ATTRIBUTES)
        return false;
    if (isDir)
        *isDir = (a & FILE_ATTRIBUTE_DIRECTORY) != 0;
    return true;
}

ListResult probeDirectory(const QString& dir)
{
    const std::wstring w = toWin32(dir);
    const DWORD a = GetFileAttributesW(w.c_str());
    if (a == INVALID_FILE_ATTRIBUTES)
        return GetLastError() == ERROR_ACCESS_DENIED ? ListResult::Denied : ListResult::NotFound;
    if (!(a & FILE_ATTRIBUTE_DIRECTORY))
        return ListResult::NotFound;
    std::vector<DirEntry> tmp;
    const ListResult r = listDirectory(dir, tmp);
    return r;
}

} // namespace picazoo::platform

#include "browse/FolderListing.h"

#include "media/Formats.h"

#include <windows.h>

#include <shlwapi.h>

#include <QDir>
#include <QFileInfo>

#include <algorithm>

namespace picazoo {

namespace {

qint64 fileTimeToMs(const FILETIME& ft)
{
    ULARGE_INTEGER u;
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    constexpr quint64 kEpochDiff = 116444736000000000ULL; // 1601 -> 1970, en 100 ns
    if (u.QuadPart < kEpochDiff)
        return 0;
    return qint64((u.QuadPart - kEpochDiff) / 10000ULL);
}

std::wstring longPath(const QString& nativePath)
{
    std::wstring p = nativePath.toStdWString();
    if (p.size() >= MAX_PATH - 12 && p.rfind(L"\\\\?\\", 0) != 0) {
        if (p.rfind(L"\\\\", 0) == 0)
            p = L"\\\\?\\UNC\\" + p.substr(2);
        else
            p = L"\\\\?\\" + p;
    }
    return p;
}

} // namespace

int naturalCompare(const QString& a, const QString& b)
{
    return StrCmpLogicalW(reinterpret_cast<const wchar_t*>(a.utf16()), reinterpret_cast<const wchar_t*>(b.utf16()));
}

bool isBrowsableImage(Format f)
{
    if (f == Format::Unknown)
        return false;
    const Kind k = kindOf(f);
    return k == Kind::Image || k == Kind::Raw;
}

bool isBrowsableImage(const QString& path) { return isBrowsableImage(formatFromPath(path)); }

QString parentFolder(const QString& filePath)
{
    return QDir::toNativeSeparators(QFileInfo(filePath).absolutePath());
}

QList<BrowseEntry> listFolderImages(const QString& folder, bool* ok)
{
    QList<BrowseEntry> out;
    QString base = QDir::toNativeSeparators(folder);
    if (!base.endsWith(u'\\'))
        base += u'\\';
    const std::wstring pattern = longPath(base + u'*');

    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileExW(pattern.c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr,
                                FIND_FIRST_EX_LARGE_FETCH);
    if (h == INVALID_HANDLE_VALUE) {
        if (ok)
            *ok = GetLastError() == ERROR_FILE_NOT_FOUND; // dossier vide
        return out;
    }
    constexpr DWORD kSkip = FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM |
                            FILE_ATTRIBUTE_REPARSE_POINT;
    do {
        if (fd.dwFileAttributes & kSkip)
            continue;
        const QString name = QString::fromWCharArray(fd.cFileName);
        const Format f = formatFromPath(name);
        if (!isBrowsableImage(f))
            continue;
        BrowseEntry e;
        e.name = name;
        e.path = base + name;
        e.format = f;
        e.size = (qint64(fd.nFileSizeHigh) << 32) | fd.nFileSizeLow;
        e.modified = fileTimeToMs(fd.ftLastWriteTime);
        out.push_back(std::move(e));
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    if (ok)
        *ok = true;
    sortNatural(out);
    return out;
}

BrowseEntry entryForFile(const QString& filePath)
{
    BrowseEntry e;
    e.path = QDir::toNativeSeparators(QFileInfo(filePath).absoluteFilePath());
    e.name = QFileInfo(e.path).fileName();
    e.format = formatFromPath(e.name);
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (GetFileAttributesExW(longPath(e.path).c_str(), GetFileExInfoStandard, &data)) {
        e.size = (qint64(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
        e.modified = fileTimeToMs(data.ftLastWriteTime);
    }
    return e;
}

void sortNatural(QList<BrowseEntry>& entries)
{
    std::stable_sort(entries.begin(), entries.end(),
                     [](const BrowseEntry& a, const BrowseEntry& b) { return naturalLess(a.name, b.name); });
}

} // namespace picazoo

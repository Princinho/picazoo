#include "platform/win/FileIdentity.h"

#include "platform/win/FileIO.h"
#include "platform/win/Paths.h"

#include <windows.h>

#include <xxhash.h>

#include <algorithm>
#include <cstring>

namespace picazoo::platform {

namespace {

constexpr qint64 kQuickChunk = 64 * 1024;

HANDLE openAttributes(const QString& path)
{
    const std::wstring w = toWin32(path);
    return CreateFileW(w.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
}

} // namespace

std::optional<FileId> fileIdentity(const QString& path)
{
    HANDLE h = openAttributes(path);
    if (h == INVALID_HANDLE_VALUE)
        return std::nullopt;
    FileId out;
    FILE_ID_INFO info{};
    if (GetFileInformationByHandleEx(h, FileIdInfo, &info, sizeof(info))) {
        out.volumeSerial = info.VolumeSerialNumber;
        static const FILE_ID_128 zero{};
        if (std::memcmp(&info.FileId, &zero, sizeof(zero)) != 0)
            out.id = QByteArray(reinterpret_cast<const char*>(&info.FileId), sizeof(info.FileId));
    } else {
        BY_HANDLE_FILE_INFORMATION bh{};
        if (GetFileInformationByHandle(h, &bh)) {
            out.volumeSerial = bh.dwVolumeSerialNumber;
            const quint64 idx = (quint64(bh.nFileIndexHigh) << 32) | bh.nFileIndexLow;
            if (idx != 0)
                out.id = QByteArray(reinterpret_cast<const char*>(&idx), sizeof(idx));
        }
    }
    CloseHandle(h);
    return out;
}

std::optional<quint64> quickHash(const QString& path, qint64 size)
{
    bool ok = false;
    QByteArray head = readFileRange(path, 0, kQuickChunk, &ok);
    if (!ok)
        return std::nullopt;
    QByteArray tail;
    if (size > kQuickChunk) {
        tail = readFileRange(path, std::max(kQuickChunk, size - kQuickChunk), kQuickChunk, &ok);
        if (!ok)
            return std::nullopt;
    }
    XXH3_state_t* st = XXH3_createState();
    XXH3_64bits_reset(st);
    XXH3_64bits_update(st, &size, sizeof(size));
    XXH3_64bits_update(st, head.constData(), size_t(head.size()));
    XXH3_64bits_update(st, tail.constData(), size_t(tail.size()));
    const quint64 h = XXH3_64bits_digest(st);
    XXH3_freeState(st);
    return h;
}

std::optional<quint64> fullHash(const QString& path)
{
    bool ok = false;
    const QByteArray data = readWholeFile(path, &ok);
    if (!ok)
        return std::nullopt;
    return XXH3_64bits(data.constData(), size_t(data.size()));
}

std::optional<quint64> volumeSerial(const QString& path)
{
    wchar_t root[MAX_PATH + 1]{};
    const std::wstring w = toWin32(path);
    if (!GetVolumePathNameW(w.c_str(), root, MAX_PATH))
        return std::nullopt;
    DWORD serial = 0;
    if (!GetVolumeInformationW(root, nullptr, 0, &serial, nullptr, nullptr, nullptr, 0))
        return std::nullopt;
    return quint64(serial);
}

} // namespace picazoo::platform

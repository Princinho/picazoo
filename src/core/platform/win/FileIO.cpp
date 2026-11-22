#include "platform/win/FileIO.h"

#include "platform/win/Paths.h"

#include <windows.h>

#include <algorithm>

namespace picazoo::platform {

namespace {

struct Handle {
    HANDLE h = INVALID_HANDLE_VALUE;
    ~Handle()
    {
        if (h != INVALID_HANDLE_VALUE)
            CloseHandle(h);
    }
    bool valid() const { return h != INVALID_HANDLE_VALUE; }
};

HANDLE openForRead(const QString& path)
{
    const std::wstring w = toWin32(path);
    return CreateFileW(w.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
}

} // namespace

QByteArray readFileRange(const QString& path, qint64 offset, qint64 maxBytes, bool* ok)
{
    if (ok)
        *ok = false;
    Handle f{openForRead(path)};
    if (!f.valid() || maxBytes <= 0)
        return {};
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(f.h, &size))
        return {};
    if (offset < 0)
        offset = std::max<qint64>(0, size.QuadPart + offset);
    if (offset >= size.QuadPart) {
        if (ok)
            *ok = true;
        return {};
    }
    const qint64 toRead = std::min<qint64>(maxBytes, size.QuadPart - offset);
    LARGE_INTEGER pos{};
    pos.QuadPart = offset;
    if (!SetFilePointerEx(f.h, pos, nullptr, FILE_BEGIN))
        return {};
    QByteArray buf;
    buf.resize(toRead);
    qint64 done = 0;
    while (done < toRead) {
        DWORD chunk = static_cast<DWORD>(std::min<qint64>(toRead - done, 64 * 1024 * 1024));
        DWORD got = 0;
        if (!ReadFile(f.h, buf.data() + done, chunk, &got, nullptr))
            return {};
        if (got == 0)
            break;
        done += got;
    }
    buf.resize(done);
    if (ok)
        *ok = true;
    return buf;
}

QByteArray readWholeFile(const QString& path, bool* ok, qint64 maxBytes)
{
    return readFileRange(path, 0, maxBytes, ok);
}

qint64 fileSize(const QString& path)
{
    WIN32_FILE_ATTRIBUTE_DATA data{};
    const std::wstring w = toWin32(path);
    if (!GetFileAttributesExW(w.c_str(), GetFileExInfoStandard, &data))
        return -1;
    return (qint64(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
}

} // namespace picazoo::platform

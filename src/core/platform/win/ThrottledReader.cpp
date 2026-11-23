#include "platform/win/ThrottledReader.h"

#include "platform/win/IoGovernor.h"
#include "platform/win/Paths.h"

#include <windows.h>

#include <xxhash.h>

#include <QElapsedTimer>
#include <QThread>

#include <algorithm>

namespace picazoo::platform {

namespace {

constexpr DWORD kBlock = 1024 * 1024; // multiple de toutes les tailles de secteur courantes

#ifndef FILE_ATTRIBUTE_RECALL_ON_OPEN
#define FILE_ATTRIBUTE_RECALL_ON_OPEN 0x00040000
#endif
#ifndef FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS
#define FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS 0x00400000
#endif

struct Handle {
    HANDLE h = INVALID_HANDLE_VALUE;
    ~Handle()
    {
        if (h != INVALID_HANDLE_VALUE)
            CloseHandle(h);
    }
};

struct AlignedBuffer {
    void* p = nullptr;
    explicit AlignedBuffer(size_t size) { p = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE); }
    ~AlignedBuffer()
    {
        if (p)
            VirtualFree(p, 0, MEM_RELEASE);
    }
};

HANDLE openUnbuffered(const std::wstring& w, bool unbuffered)
{
    const DWORD flags = FILE_FLAG_SEQUENTIAL_SCAN | (unbuffered ? FILE_FLAG_NO_BUFFERING : 0);
    return CreateFileW(w.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                       OPEN_EXISTING, flags, nullptr);
}

void sleepUs(qint64 us)
{
    while (us > 0) {
        const qint64 step = std::min<qint64>(us, 200000);
        QThread::usleep(quint64(step));
        us -= step;
    }
}

} // namespace

ReadCounters& readCounters()
{
    static ReadCounters counters;
    return counters;
}

bool isOnlineOnly(const QString& path)
{
    const DWORD attrs = GetFileAttributesW(toWin32(path).c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES)
        return false;
    return (attrs & (FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS | FILE_ATTRIBUTE_RECALL_ON_OPEN | FILE_ATTRIBUTE_OFFLINE)) != 0;
}

ContentHash hashFileContent(const QString& path, const ReadControl& control)
{
    ContentHash out;
    const std::wstring w = toWin32(path);
    const DWORD attrs = GetFileAttributesW(w.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        out.status = ReadStatus::Missing;
        return out;
    }
    if (attrs & (FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS | FILE_ATTRIBUTE_RECALL_ON_OPEN | FILE_ATTRIBUTE_OFFLINE)) {
        out.status = ReadStatus::OnlineOnly;
        return out;
    }

    Handle f{openUnbuffered(w, true)};
    if (f.h == INVALID_HANDLE_VALUE)
        f.h = openUnbuffered(w, false); // certains partages refusent la lecture non tamponnée
    if (f.h == INVALID_HANDLE_VALUE) {
        const DWORD err = GetLastError();
        out.status = (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) ? ReadStatus::Missing : ReadStatus::Failed;
        return out;
    }

    AlignedBuffer buf(kBlock);
    if (!buf.p)
        return out;
    XXH3_state_t* state = XXH3_createState();
    if (!state)
        return out;
    XXH3_128bits_reset(state);
    ++readCounters().files;

    QElapsedTimer timer;
    for (;;) {
        if (control.waitWhileHeld)
            control.waitWhileHeld();
        if (control.cancelled && control.cancelled()) {
            XXH3_freeState(state);
            out.status = ReadStatus::Cancelled;
            return out;
        }
        if (control.governor)
            sleepUs(control.governor->reserve(kBlock));
        DWORD got = 0;
        timer.start();
        if (!ReadFile(f.h, buf.p, kBlock, &got, nullptr)) {
            XXH3_freeState(state);
            out.status = ReadStatus::Failed;
            return out;
        }
        const qint64 latency = timer.nsecsElapsed() / 1000;
        if (got == 0)
            break;
        if (control.governor)
            control.governor->recordRead(got, latency);
        XXH3_128bits_update(state, buf.p, got);
        out.bytesRead += got;
        readCounters().bytes += got;
        if (got < kBlock)
            break;
    }
    XXH128_canonical_t canon;
    XXH128_canonicalFromHash(&canon, XXH3_128bits_digest(state));
    XXH3_freeState(state);
    out.hash = QByteArray(reinterpret_cast<const char*>(canon.digest), sizeof(canon.digest));
    out.status = ReadStatus::Ok;
    return out;
}

} // namespace picazoo::platform

#include "platform/win/DirectoryWatcher.h"

#include "platform/win/Paths.h"

#include <windows.h>

#include <vector>

namespace picazoo::platform {

namespace {

constexpr DWORD kBufferSize = 64 * 1024;
constexpr DWORD kFilter = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_SIZE |
                          FILE_NOTIFY_CHANGE_LAST_WRITE;

} // namespace

DirectoryWatcher::DirectoryWatcher(QString root, Callback cb) : root_(std::move(root)), cb_(std::move(cb)) {}

DirectoryWatcher::~DirectoryWatcher() { stop(); }

bool DirectoryWatcher::start()
{
    if (running_)
        return true;
    const std::wstring w = toWin32(root_);
    HANDLE dir = CreateFileW(w.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                             nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
    if (dir == INVALID_HANDLE_VALUE)
        return false;
    dir_ = dir;
    stopEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    running_ = true;
    thread_ = std::thread([this] { loop(); });
    return true;
}

void DirectoryWatcher::stop()
{
    running_ = false;
    if (stopEvent_)
        SetEvent(static_cast<HANDLE>(stopEvent_));
    if (thread_.joinable())
        thread_.join();
    if (dir_)
        CloseHandle(static_cast<HANDLE>(dir_));
    if (stopEvent_)
        CloseHandle(static_cast<HANDLE>(stopEvent_));
    dir_ = nullptr;
    stopEvent_ = nullptr;
}

void DirectoryWatcher::loop()
{
    HANDLE dir = static_cast<HANDLE>(dir_);
    HANDLE stop = static_cast<HANDLE>(stopEvent_);
    std::vector<DWORD> buffer(kBufferSize / sizeof(DWORD)); // alignement DWORD exigé
    OVERLAPPED ov{};
    ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

    while (running_) {
        ResetEvent(ov.hEvent);
        if (!ReadDirectoryChangesW(dir, buffer.data(), kBufferSize, TRUE, kFilter, nullptr, &ov, nullptr)) {
            cb_(Event{{}, false, true});
            break;
        }
        HANDLE handles[2] = {ov.hEvent, stop};
        const DWORD w = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
        if (w != WAIT_OBJECT_0) {
            CancelIoEx(dir, &ov);
            DWORD ignored = 0;
            GetOverlappedResult(dir, &ov, &ignored, TRUE);
            break;
        }
        DWORD bytes = 0;
        if (!GetOverlappedResult(dir, &ov, &bytes, FALSE)) {
            const DWORD err = GetLastError();
            if (err == ERROR_NOTIFY_ENUM_DIR) {
                cb_(Event{{}, true, false});
                continue;
            }
            cb_(Event{{}, false, true}); // disque retiré, accès perdu…
            break;
        }
        if (bytes == 0) {
            cb_(Event{{}, true, false}); // tampon débordé
            continue;
        }
        Event e;
        const auto* base = reinterpret_cast<const BYTE*>(buffer.data());
        for (DWORD offset = 0;;) {
            const auto* info = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(base + offset);
            const QString rel = QString::fromWCharArray(info->FileName, int(info->FileNameLength / sizeof(WCHAR)));
            e.paths << joinPath(root_, rel);
            if (info->NextEntryOffset == 0)
                break;
            offset += info->NextEntryOffset;
        }
        cb_(e);
    }
    CloseHandle(ov.hEvent);
}

} // namespace picazoo::platform

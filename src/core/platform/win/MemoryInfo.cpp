#include "platform/win/MemoryInfo.h"

#include <windows.h>

#include <psapi.h>

namespace picazoo::platform {

MemorySnapshot memorySnapshot()
{
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    if (!GlobalMemoryStatusEx(&status))
        return {16LL * 1024 * 1024 * 1024, 8LL * 1024 * 1024 * 1024}; // optimiste : ne jamais brider par erreur
    return {qint64(status.ullTotalPhys), qint64(status.ullAvailPhys)};
}

qint64 processPeakWorkingSetBytes()
{
    PROCESS_MEMORY_COUNTERS counters{};
    counters.cb = sizeof(counters);
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)))
        return 0;
    return qint64(counters.PeakWorkingSetSize);
}

qint64 processWorkingSetBytes()
{
    PROCESS_MEMORY_COUNTERS counters{};
    counters.cb = sizeof(counters);
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)))
        return 0;
    return qint64(counters.WorkingSetSize);
}

} // namespace picazoo::platform

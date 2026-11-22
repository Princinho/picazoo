#include "platform/win/CpuInfo.h"

#include <windows.h>

#include <algorithm>
#include <bit>
#include <map>
#include <thread>
#include <vector>

namespace picazoo::platform {

int recommendedWorkerThreads()
{
    const int logical = std::max(1, int(std::thread::hardware_concurrency()));
    DWORD len = 0;
    GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &len);
    if (len == 0)
        return std::max(1, logical - 1);
    std::vector<char> buf(len);
    auto* info = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buf.data());
    if (!GetLogicalProcessorInformationEx(RelationProcessorCore, info, &len))
        return std::max(1, logical - 1);

    // Par classe d'efficacité : nombre de cœurs physiques et de threads logiques.
    std::map<int, std::pair<int, int>> classes;
    for (DWORD off = 0; off < len;) {
        auto* p = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buf.data() + off);
        if (p->Relationship == RelationProcessorCore) {
            int threads = 0;
            for (WORD g = 0; g < p->Processor.GroupCount; ++g)
                threads += std::popcount(static_cast<unsigned long long>(p->Processor.GroupMask[g].Mask));
            auto& c = classes[p->Processor.EfficiencyClass];
            c.first += 1;
            c.second += threads;
        }
        off += p->Size;
    }
    if (classes.size() < 2)
        return std::max(1, logical - 1);
    const auto& performance = classes.rbegin()->second; // classe la plus élevée = cœurs performants
    int efficient = 0;
    for (auto it = classes.begin(); it != std::prev(classes.end()); ++it)
        efficient += it->second.first;
    return std::clamp(performance.second + efficient / 2, 2, std::max(2, logical - 1));
}

} // namespace picazoo::platform

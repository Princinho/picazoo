#include "jobs/MemoryBudget.h"

#include <algorithm>
#include <cmath>

namespace picazoo {

namespace {

constexpr double kGiB = 1024.0 * 1024 * 1024;

// Au-delà de cette RAM disponible par thread de vignette visé, un thread
// supplémentaire est autorisé ; en-deçà, le nombre de threads est réduit.
// Calibré avec une hypothèse de coût pic par décodage d'environ 150-200 Mo
// (HEIC pleine résolution dans le pire cas, sans miniature intégrée) plus de
// la marge pour Qt et le reste du processus.
constexpr double kAvailableGiBPerThread = 0.75;

// RAM disponible au-delà de laquelle les caches ne sont plus réduits par
// rapport au comportement d'avant cette fonctionnalité (32 Mo par connexion
// SQLite, 256 Mo pour le cache LRU de vignettes).
constexpr double kCacheReferenceAvailableGiB = 8.0;

constexpr qint64 kSqliteCacheFloorBytes = 4LL * 1024 * 1024;     // plafond bas sur une machine très contrainte
constexpr qint64 kSqliteCacheDefaultBytes = 32LL * 1024 * 1024;  // comportement actuel (Database::open)
constexpr qint64 kThumbnailLruFloorBytes = 32LL * 1024 * 1024;   // plafond bas
constexpr qint64 kThumbnailLruDefaultBytes = 256LL * 1024 * 1024; // comportement actuel (ThumbnailImageProvider)

qint64 scaleToward(qint64 floorBytes, qint64 defaultBytes, double fraction)
{
    return floorBytes + static_cast<qint64>(std::llround(double(defaultBytes - floorBytes) * fraction));
}

} // namespace

MemoryBudget computeMemoryBudget(const MemoryBudgetInputs& inputs)
{
    const qint64 availableBytes = std::max<qint64>(0, inputs.availablePhysicalBytes);
    const int cpuThreads = std::max(1, inputs.recommendedCpuThreads);
    const double availableGiB = double(availableBytes) / kGiB;

    const double rawThreads = availableGiB / kAvailableGiBPerThread;
    const qint64 roundedThreads = static_cast<qint64>(std::lround(rawThreads));
    const int threads = int(std::clamp<qint64>(roundedThreads, 1, cpuThreads));

    const double cacheFraction = std::clamp(availableGiB / kCacheReferenceAvailableGiB, 0.0, 1.0);

    MemoryBudget budget;
    budget.thumbnailThreads = threads;
    budget.sqliteCacheBytesPerConnection = scaleToward(kSqliteCacheFloorBytes, kSqliteCacheDefaultBytes, cacheFraction);
    budget.thumbnailLruCacheBytes = scaleToward(kThumbnailLruFloorBytes, kThumbnailLruDefaultBytes, cacheFraction);
    return budget;
}

} // namespace picazoo

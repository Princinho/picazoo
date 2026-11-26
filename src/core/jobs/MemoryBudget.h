#pragma once

// Budget mémoire du scan (research B2, 008) : fonction pure (pas d'appel Windows,
// pas d'E/S) qui dimensionne le nombre de threads de vignettes et les plafonds de
// cache à partir de la RAM disponible sur la machine, pour que le pic de mémoire
// du scan initial reste raisonnable sans réglage de l'utilisateur.

#include <QtGlobal>

namespace picazoo {

struct MemoryBudgetInputs {
    qint64 totalPhysicalBytes = 0;
    qint64 availablePhysicalBytes = 0;
    int recommendedCpuThreads = 1; // CpuInfo::recommendedWorkerThreads()
};

struct MemoryBudget {
    int thumbnailThreads = 1;                    // >= 1, <= recommendedCpuThreads
    qint64 sqliteCacheBytesPerConnection = 0;     // PRAGMA cache_size
    qint64 thumbnailLruCacheBytes = 0;            // plafond ThumbnailImageProvider
};

MemoryBudget computeMemoryBudget(const MemoryBudgetInputs& inputs);

} // namespace picazoo

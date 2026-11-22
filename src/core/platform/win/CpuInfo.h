#pragma once

namespace picazoo::platform {

// Nombre de threads de calcul pour les vignettes (research R12).
// Processeur hybride (cœurs performants et efficaces) : threads des cœurs performants
// + la moitié des cœurs efficaces ; au-delà, le débit baisse (mesuré sur Core Ultra 5 225U :
// 23 vignettes/s à 8 threads, 18 à 12). Processeur classique : cœurs logiques - 1.
int recommendedWorkerThreads();

} // namespace picazoo::platform

#pragma once

// Mémoire physique de la machine et mémoire du processus (research B1, 008).
// Sert à calculer le budget mémoire du scan (jobs/MemoryBudget) sans que cette
// logique, testable sans Windows, dépende d'un appel système.

#include <QtGlobal>

namespace picazoo::platform {

struct MemorySnapshot {
    qint64 totalPhysicalBytes = 0;
    qint64 availablePhysicalBytes = 0;
};

// GlobalMemoryStatusEx. En cas d'échec de l'appel Windows (improbable), renvoie
// une valeur optimiste pour ne jamais brider le scan par erreur.
MemorySnapshot memorySnapshot();

// GetProcessMemoryInfo (PeakWorkingSetSize) : même grandeur que la colonne
// « Mémoire » du Gestionnaire des tâches au pic observé depuis le lancement du
// processus.
qint64 processPeakWorkingSetBytes();

// GetProcessMemoryInfo (WorkingSetSize) : valeur instantanée (retour au repos
// après le scan).
qint64 processWorkingSetBytes();

} // namespace picazoo::platform

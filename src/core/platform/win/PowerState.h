#pragma once

// Alimentation et priorité d'arrière-plan (research D5).

namespace picazoo::platform {

// Vrai sur batterie ou en mode économiseur d'énergie ; faux sur secteur ou si inconnu.
bool onBatteryOrSaver();

// Passe le fil courant en mode d'arrière-plan de Windows (priorités processeur, E/S et
// mémoire basses). Sans effet si le fil y est déjà.
void enterBackgroundMode();

} // namespace picazoo::platform

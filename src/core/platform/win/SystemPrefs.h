#pragma once

// Préférences d'affichage de Windows respectées par la visionneuse (FR-015, FR-023).

namespace picazoo::platform {

// « Afficher les animations dans Windows » (accessibilité).
bool animationsEnabled();

// « Effets de transparence » (Personnalisation › Couleurs).
bool transparencyEnabled();

} // namespace picazoo::platform

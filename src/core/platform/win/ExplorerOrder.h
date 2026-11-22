#pragma once

// Ordre d'affichage d'un dossier dans une fenêtre de l'Explorateur (research V5).
// Lecture seule : rien n'est modifié dans l'Explorateur.

#include <QStringList>

#include <optional>

namespace picazoo::platform {

// Noms des fichiers de `folder`, dans l'ordre où la fenêtre de l'Explorateur qui
// l'affiche les montre (tri et regroupements compris). Parmi plusieurs fenêtres ou
// onglets sur ce dossier, préfère celle qui était au premier plan (`foreground`,
// un HWND) puis un onglet visible. Le bureau est pris en compte. Rien si aucune
// fenêtre ne montre ce dossier. Initialise COM (STA) sur le thread appelant.
std::optional<QStringList> explorerViewOrder(const QString& folder, quintptr foreground);

} // namespace picazoo::platform

#pragma once

#include <QString>

namespace picazoo::platform {

// Ouvre l'Explorateur sur le dossier du fichier, fichier sélectionné (FR-023).
bool revealInExplorer(const QString& path);

// Ouvre le fichier avec l'application par défaut du système (FR-022).
bool openWithDefaultApp(const QString& path);

} // namespace picazoo::platform

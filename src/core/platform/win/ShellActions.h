#pragma once

#include <QString>
#include <QStringList>

namespace picazoo::platform {

// Ouvre l'Explorateur sur le dossier du fichier, fichier sélectionné (FR-023).
bool revealInExplorer(const QString& path);

// Ouvre le fichier avec l'application par défaut du système (FR-022).
bool openWithDefaultApp(const QString& path);

// 007 : fenêtre Propriétés de Windows pour un ou plusieurs fichiers (dossiers différents
// acceptés). `owner` : fenêtre propriétaire (HWND), 0 sinon. Sur le fil de l'interface.
bool showProperties(const QStringList& paths, quintptr owner = 0);

} // namespace picazoo::platform

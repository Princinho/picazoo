#pragma once

// Presse-papiers (007, research M2) : « Copier » et « Copier le chemin » des menus contextuels.
// À appeler sur le fil de l'interface (QClipboard).

#include <QImage>
#include <QStringList>

namespace picazoo::platform {

// Met les fichiers dans le presse-papiers comme l'Explorateur (coller dans un dossier les
// **copie**, jamais ne les déplace : « Preferred DropEffect » = copie), et l'image décodée en
// plus si `image` n'est pas vide (coller dans un document, une messagerie).
bool copyFilesToClipboard(const QStringList& paths, const QImage& image = {});

// Texte brut ; « Copier le chemin » : un chemin Windows par ligne.
bool copyTextToClipboard(const QString& text);

// Chemins au format Windows, un par ligne (séparateur CRLF).
QString pathsAsText(const QStringList& paths);

} // namespace picazoo::platform

#pragma once

#include <QString>

#include <string>

namespace picazoo::platform {

// Chemin absolu normalisé : séparateurs '\', lettre de lecteur en majuscule,
// sans '\' final (sauf racine "C:\"), casse conservée.
QString normalizePath(const QString& path);

// Comparaison insensible à la casse, comme NTFS.
bool samePath(const QString& a, const QString& b);

// Vrai si `child` est `parent` ou se trouve à l'intérieur (chemins normalisés).
bool isInside(const QString& child, const QString& parent);

// Dossier parent d'un chemin normalisé ("C:\a\b" -> "C:\a", "C:\a" -> "C:\").
QString parentPath(const QString& path);

// Dernier segment ("C:\a\b" -> "b", "C:\" -> "C:").
QString lastSegment(const QString& path);

// Chemin pour les API Win32, préfixé par \\?\ au-delà de 248 caractères.
std::wstring toWin32(const QString& path);

// Joint un dossier et un nom.
QString joinPath(const QString& dir, const QString& name);

} // namespace picazoo::platform

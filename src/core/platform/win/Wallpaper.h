#pragma once

// Fond d'écran de Windows (007, research W1) : « Définir comme fond d'écran » et son annulation.
// Windows fait sa propre copie de l'image ; le fichier de l'utilisateur n'est ni copié ni modifié.
// Les fonctions initialisent COM sur le fil appelant ; à appeler hors du fil de l'interface.

#include <QList>
#include <QPair>
#include <QString>

#include <optional>

namespace picazoo::platform {

struct WallpaperSnapshot {
    QList<QPair<QString, QString>> monitors; // identifiant de l'écran, chemin du fond
    int position = 0;                        // DESKTOP_WALLPAPER_POSITION
    bool slideshow = false;                  // diaporama Windows actif
    bool restorable = false;                 // faux si diaporama ou si un ancien fond n'existe plus
};

// Formats que Windows accepte comme fond d'écran sans conversion.
bool wallpaperSupported(const QString& path);

std::optional<WallpaperSnapshot> snapshotWallpaper();
bool setWallpaper(const QString& path);                 // tous les écrans, disposition inchangée
bool restoreWallpaper(const WallpaperSnapshot& snapshot); // seulement si restorable

} // namespace picazoo::platform

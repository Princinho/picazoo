#pragma once

// Listage d'un seul dossier pour la visionneuse autonome (005, research V6) :
// images prises en charge, sans sous-dossiers, sans fichiers cachés ni système.
// Aucun fichier n'est ouvert : seules les entrées du dossier sont lues.

#include "library/Types.h"

#include <QList>
#include <QString>

namespace picazoo {

struct BrowseEntry {
    QString path;          // chemin absolu natif
    QString name;          // nom du fichier
    Format format = Format::Unknown;
    qint64 size = -1;      // octets, -1 si inconnu
    qint64 modified = 0;   // date de modification, ms depuis l'époque (UTC)
};

// Comparaison « naturelle » de l'Explorateur (`photo2` avant `photo10`).
int naturalCompare(const QString& a, const QString& b);
inline bool naturalLess(const QString& a, const QString& b) { return naturalCompare(a, b) < 0; }

// Vrai pour une image que la visionneuse sait parcourir (vidéos et formats inconnus exclus).
bool isBrowsableImage(Format f);
bool isBrowsableImage(const QString& path);

// Dossier parent natif d'un chemin de fichier.
QString parentFolder(const QString& filePath);

// Images du dossier, dans l'ordre naturel des noms. `ok` vaut false si le dossier
// ne peut pas être lu.
QList<BrowseEntry> listFolderImages(const QString& folder, bool* ok = nullptr);

// Entrée pour un seul fichier (taille et date lues sans ouvrir le fichier).
BrowseEntry entryForFile(const QString& filePath);

// Trie des entrées dans l'ordre naturel des noms.
void sortNatural(QList<BrowseEntry>& entries);

} // namespace picazoo

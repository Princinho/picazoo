#pragma once

// Fusion de l'ordre d'affichage de l'Explorateur avec le listage du disque
// (research V5) : les images que la fenêtre montre, dans son ordre, puis celles
// qu'elle ne montre pas (filtre), dans l'ordre naturel.

#include "browse/FolderListing.h"

#include <QStringList>

namespace picazoo {

// `viewOrderNames` : noms de fichiers dans l'ordre de la fenêtre (tous types, la
// fonction ne garde que ceux du listage) ; `listing` : listage en ordre naturel.
// Les noms sont comparés sans tenir compte de la casse.
QList<BrowseEntry> mergeOrder(const QStringList& viewOrderNames, const QList<BrowseEntry>& listing);

} // namespace picazoo

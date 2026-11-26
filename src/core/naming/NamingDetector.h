#pragma once

// Détecteur de racine de nommage (specs/003-images-liees/research.md N1/N2/N4) : fonction
// pure, aucune E/S, aucun accès à library.db. Entrée : noms de fichiers déjà en mémoire.

#include "naming/NamingTypes.h"

#include <QList>
#include <QString>

#include <optional>

namespace picazoo {

// Analyse un seul nom (sans extension) ; std::nullopt si aucune racine reconnue (N1).
std::optional<DetectedRoot> detectRoot(const QString& fileNameWithoutExtension);

// Confiance d'un ensemble d'éléments (N4) ; std::nullopt : élément sans racine reconnue.
Confidence confidenceFor(const QList<std::optional<DetectedRoot>>& roots);

// Nom proposé pour un groupe (FR-007) : « vacances_bali » -> « vacances bali ».
QString suggestedName(const QString& displayRoot);

// Nom de fichier sans sa dernière extension.
QString baseNameOf(const QString& fileName);

// Regroupe un ensemble d'éléments par racine détectée (N1/N2/N4). Pure : aucune lecture de
// fichier, aucun accès à library.db. `includeGeneric` reflète le réglage
// `naming_include_generic_roots` (N2). Jamais de groupe d'un seul élément (FR-004).
QList<NamingCandidate> groupByRoot(const QList<NamingInput>& items, bool includeGeneric);

} // namespace picazoo

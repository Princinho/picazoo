#pragma once

// Accès aux tables des images liées partagés par le moteur (thread d'indexation) et le dépôt
// (thread de l'interface). L'appelant tient la transaction.

#include "naming/NamingTypes.h"

#include <QHash>
#include <QSet>
#include <QString>

namespace picazoo {

class Database;

// Décisions sur les racines (naming_decision).
struct NamingRules {
    QHash<QString, QString> merged; // racine -> racine du groupe qui l'a absorbée
    QSet<QString> rejected;         // « Ce n'est pas un groupe »

    static NamingRules load(Database& db);
    // Racine sous laquelle se rangent les éléments de `key` après les fusions.
    QString resolve(const QString& key) const;
};

// Recalcule le nombre d'éléments et la confiance d'un groupe ; le supprime s'il lui reste moins
// de deux éléments (FR-004). Vrai si quelque chose a changé.
bool refreshNamingGroup(Database& db, NamingGroupId id);

void deleteNamingGroup(Database& db, NamingGroupId id);

} // namespace picazoo

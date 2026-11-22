#pragma once

// Règle de la copie à garder (FR-008, research D7-3) : RAW avant tout autre format, puis le
// plus de pixels, puis le plus gros fichier à pixels égaux (moins compressé), puis présence
// d'une date de prise de vue, puis la date la plus ancienne, puis le chemin le plus court.

#include "duplicates/DuplicateTypes.h"

#include <QList>
#include <QString>

namespace picazoo {

struct CopyFacts {
    ItemId id = 0;
    bool isRaw = false;
    qint64 pixels = 0;
    qint64 sizeBytes = 0;
    bool hasTakenAt = false;
    qint64 date = 0; // date de prise de vue, sinon date du fichier
    QString path;
};

class KeeperRule {
public:
    // Négatif si `a` est meilleure que `b`, positif si `b` l'est, 0 si égales.
    // `decisive` reçoit le critère qui les départage.
    static int compare(const CopyFacts& a, const CopyFacts& b, KeeperReason* decisive = nullptr);
    // Trie du meilleur au moins bon (ordre stable et total : l'identifiant départage).
    static void sort(QList<CopyFacts>& copies);
    // Raison pour laquelle `best` l'emporte sur toutes les autres copies (le critère le plus
    // important qui la départage de la deuxième).
    static KeeperReason reason(const QList<CopyFacts>& sortedCopies);
};

} // namespace picazoo

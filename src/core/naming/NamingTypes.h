#pragma once

// Types des images liées (specs/003-images-liees/data-model.md, contracts/core-api.md).
// Aucun type ici ne dépend de `duplicates/` (research.md N5) : indépendance totale.

#include "library/Types.h"

#include <QList>
#include <QString>

#include <optional>

namespace picazoo {

using NamingGroupId = qint64;

// Forme de numérotation reconnue par le détecteur (research.md N1).
enum class RootShape { None, SeparatorDigits, Parenthesized, CopySuffix, GluedDigits };

// Plus la valeur est petite, plus la confiance est forte (research.md N4).
enum class Confidence { RegularSeries = 0, SameRoot = 1, GenericRoot = 2 };
enum class ConfidenceFilter { All, RegularSeries, SameRoot, GenericRoot };
enum class NamingGroupState { ToReview, Confirmed, Rejected };

// --- Détecteur pur (aucune E/S, aucun accès à library.db)

struct DetectedRoot {
    QString normalizedKey;  // racine normalisée (casse ignorée) : identifie le groupe candidat
    QString displayRoot;    // racine telle que rencontrée (casse d'origine), pour le nom suggéré
    RootShape shape = RootShape::None;
    bool generic = false;  // préfixe d'appareil ou racine trop courte (research.md N2)
    std::optional<qint64> number;  // numéro de la série, pour juger sa régularité (N4)
};

struct NamingInput {
    ItemId item = 0;
    QString fileName;  // sans extension
};

struct NamingCandidate {
    QString rootKey;
    QString displayRoot;
    QList<ItemId> items;
    Confidence confidence = Confidence::SameRoot;
};

// --- Lectures/écritures pour l'interface

struct NamingMember {
    ItemId item = 0;
    QString detectedRoot;
    bool manual = false;
    // Repris de l'index de la 001 pour l'affichage.
    QString fileName;
    QString folderPath;
    Kind kind = Kind::Image;
    ThumbState thumbState = ThumbState::Pending;
    int version = 1;
    bool available = true;  // faux : dossier surveillé indisponible (disque débranché)
};

struct NamingGroup {
    NamingGroupId id = 0;
    QString rootKey;
    QString displayName;
    Confidence confidence = Confidence::SameRoot;
    NamingGroupState state = NamingGroupState::ToReview;
    bool renamed = false;
    QList<NamingMember> members;
};

// Ligne de la liste des groupes ; `signature` change dès que le contenu affiché change.
struct NamingGroupRow {
    NamingGroupId id = 0;
    QString displayName;
    int memberCount = 0;
    QString signature;
    Confidence confidence = Confidence::SameRoot;
};

struct NamingSummary {
    int groupCount = 0;
    int regularSeries = 0;
    int sameRoot = 0;
    int genericRoot = 0;
};

// Décision durable sur une racine (data-model.md, naming_decision), annulable (FR-009).
struct NamingDecision {
    enum class Kind { Rejected, MergedInto };
    QString rootKey;
    Kind kind = Kind::Rejected;
    QString label;       // nom du groupe au moment de la décision
    QString targetName;  // MergedInto : nom actuel du groupe qui l'a absorbé
};

// Élément proposé par « Ajouter des éléments… ».
struct NamingItem {
    ItemId item = 0;
    QString fileName;
    QString folderPath;
    ThumbState thumbState = ThumbState::Pending;
    int version = 1;
};

QString toDb(Confidence);
QString toDb(NamingGroupState);
Confidence confidenceFromDb(const QString&);
NamingGroupState namingGroupStateFromDb(const QString&);

// Textes affichés (français).
QString confidenceLabel(Confidence);

} // namespace picazoo

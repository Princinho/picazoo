#pragma once

// Types de la détection des doublons (specs/002-detection-doublons/data-model.md).

#include "library/Types.h"

#include <QList>
#include <QString>

#include <optional>

namespace picazoo {

using GroupId = qint64;
using OperationId = qint64;

// Niveau de confiance ; plus la valeur est petite, plus la ressemblance est forte.
enum class DupLevel { Identical = 0, SameImage = 1, Similar = 2 };
enum class KeeperReason { Raw, Resolution, LessCompressed, HasExif, Oldest, Path };
enum class GroupState { ToReview, KeptAll };
enum class LevelFilter { All, Identical, SameImage, Similar };

// 006 : la suggestion peut venir d'une règle de dossier (research R2).
enum class SuggestionReason { Quality, FolderPreferred, FolderAvoided };

// Drapeaux d'un groupe (dup_group.flags).
enum GroupFlag : int {
    FlagAllAvoided = 1,     // toutes les copies sont dans des dossiers à écarter
    FlagPreferredWorse = 2, // la copie du dossier préféré est moins bonne : suggestion de qualité gardée
};

struct DuplicateCopy {
    ItemId item = 0;
    bool isCenter = false;    // centre du groupe (toutes les copies lui ressemblent)
    bool isSuggested = false; // copie suggérée à garder (qualité 002 + règles de dossier)
    bool keep = false;       // choix actuel (suggestion ou choix de l'utilisateur)
    bool userChoice = false; // vrai si `keep` vient de l'utilisateur
    DupLevel level = DupLevel::Identical; // par rapport au centre
    std::optional<double> similarity;
    int rotation = 0;
};

struct DuplicateGroup {
    GroupId id = 0;
    DupLevel level = DupLevel::Identical; // le plus faible des membres
    KeeperReason reason = KeeperReason::Path;
    GroupState state = GroupState::ToReview;
    qint64 reclaimableBytes = 0;
    ItemId center = 0;
    ItemId suggested = 0;
    SuggestionReason suggestionReason = SuggestionReason::Quality;
    QString suggestionDetail; // chemin de la règle qui a décidé
    int flags = 0;
    QList<DuplicateCopy> copies; // le centre en premier
};

// --- 006 : requêtes, règles, lots, paires (contracts/core-api.md)

struct GroupQuery {
    LevelFilter level = LevelFilter::All;
    QString folder;        // vide = pas de filtre ; sinon une copie dans ce dossier ou dessous
    QString pairA, pairB;  // tous deux non vides = une copie dans A et une copie dans B
    enum class Type { Any, Photos, Videos, RawJpeg } type = Type::Any;
    enum class State { Any, Untouched, Modified, Flagged } state = State::Any;
    qint64 minBytes = 0;   // 0 = pas de filtre
    qint64 dateFrom = 0;   // epoch ms, 0 = ouvert ; date de tri de la copie gardée
    qint64 dateTo = 0;
    enum class Sort { Reclaimable, Copies, Date, Folder } sort = Sort::Reclaimable;
};

enum class RuleKind { Prefer, Avoid };

struct FolderRule {
    qint64 id = 0;
    QString path;
    RuleKind kind = RuleKind::Prefer;
    qint64 createdAt = 0;
    bool found = true; // un dossier de la bibliothèque est égal ou en dessous
    int groups = 0;    // groupes dont la suggestion vient de cette règle
};

enum class BatchKind { Reset, KeepFolder, KeepAll, NotDuplicate };

struct BatchResult {
    qint64 batchId = 0; // 0 : rien n'a été fait
    int changed = 0;    // groupes modifiés
    int skipped = 0;    // groupes sans copie dans le dossier (KeepFolder)
    int flagged = 0;    // groupes protégés : la copie du dossier est moins bonne (KeepFolder)
    QString message;
};

struct BatchInfo {
    qint64 id = 0;
    BatchKind kind = BatchKind::Reset;
    QString label;
    QString detail;
    int groups = 0;
    qint64 createdAt = 0;
    bool undone = false;
};

struct UndoResult {
    int restored = 0;    // copies rétablies
    int notRestored = 0; // groupes modifiés depuis, laissés tels quels
};

struct SelectionTotals {
    int groups = 0;
    int files = 0; // copies « à la corbeille »
    qint64 bytes = 0;
};

struct FolderPair {
    QString a, b;
    int groups = 0;
    qint64 bytes = 0;
    int subPairs = 1; // paires de sous-dossiers réunies sous ces deux racines
};

struct FolderStat {
    QString folder;
    int groups = 0;
    qint64 bytes = 0;
};

struct SetAsideEntry {
    enum class Kind { KeptAll, NotDuplicate } kind = Kind::KeptAll;
    QList<ItemId> items;
    qint64 decidedAt = 0;
    GroupId group = 0; // KeptAll seulement
};

struct DuplicateSummary {
    int groups = 0;
    qint64 reclaimableBytes = 0;
};

struct DuplicateProgress {
    enum class Phase { Idle, Waiting, Content, Visual, Grouping };
    Phase phase = Phase::Idle;
    qint64 done = 0;
    qint64 total = 0;
    bool paused = false;
    // Raison d'une attente : library_busy, on_battery, disabled, user_paused, viewer.
    QString reason;
};

QString toDb(DupLevel);
QString toDb(KeeperReason);
QString toDb(GroupState);
DupLevel dupLevelFromDb(const QString&);
KeeperReason keeperReasonFromDb(const QString&);
GroupState groupStateFromDb(const QString&);

QString toDb(SuggestionReason); // vide pour Quality (NULL en base)
SuggestionReason suggestionReasonFromDb(const QString&);
QString toDb(RuleKind);
RuleKind ruleKindFromDb(const QString&);
QString toDb(BatchKind);
BatchKind batchKindFromDb(const QString&);

// Textes affichés (français).
QString levelLabel(DupLevel);
QString reasonLabel(KeeperReason);
// Raison de la suggestion : celle de la 002, ou « Dossier préféré : X » / « Dossier à écarter : X ».
QString suggestionLabel(const DuplicateGroup&);
QString flagLabel(int flags); // vide si aucun drapeau

} // namespace picazoo

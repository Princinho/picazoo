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

struct DuplicateCopy {
    ItemId item = 0;
    bool isCenter = false;   // copie suggérée à garder
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
    QList<DuplicateCopy> copies; // le centre en premier
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

// Textes affichés (français).
QString levelLabel(DupLevel);
QString reasonLabel(KeeperReason);

} // namespace picazoo

# Implementation Plan: Détection des doublons

**Branch**: `002-detection-doublons` | **Date**: 2026-11-22 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/002-detection-doublons/spec.md`

## Summary

Retrouver dans la bibliothèque de la 001 les copies identiques (photos et
vidéos) et les copies d'une même photo en une autre taille, un autre format,
une autre compression, une autre orientation ou légèrement retouchée ; les
présenter en groupes avec un niveau de confiance en mots et une copie
suggérée à garder ; et laisser l'utilisateur mettre le reste à la corbeille
de Windows, après un récapitulatif, avec un journal et une annulation.

Deux idées portent le plan :

1. **Lire le moins possible.** Les doublons exacts sont filtrés par la taille
   puis par l'empreinte rapide déjà calculée par la 001 : seuls les fichiers
   presque sûrement en double sont lus en entier (XXH3-128). Les
   quasi-doublons sont trouvés **à partir des vignettes de 320 px déjà
   stockées**, sans relire aucune photo : pHash sur quatre rotations,
   recherche par force brute en mémoire, vérification par SSIM sur image
   normalisée et par la date de prise de vue pour écarter les rafales.
2. **Ne jamais gêner l'ordinateur ni risquer un fichier.** Les lectures
   passent par un lecteur régulé (priorité d'arrière-plan du système, sans
   cache, débit plafonné et adaptatif, jamais en même temps que l'indexation
   de la 001, pause sur batterie). La corbeille passe par `IFileOperation`
   avec un double verrou qui rend l'effacement définitif impossible ; le
   journal est écrit avant d'agir.

Le détail des choix est dans [research.md](research.md) (D1–D12).

## Technical Context

**Language/Version**: C++20 (MSVC 14.51, Visual Studio Build Tools 2026), comme la 001

**Primary Dependencies**: celles de la 001, **sans ajout** : Qt 6.12 LTS
(Core, Gui, Quick, QuickControls2), libjpeg-turbo (TurboJPEG 3, décodage
réduit des vignettes), SQLite 3, xxHash (XXH3-128). API Windows du SDK :
`IFileOperation` et le dossier shell de la corbeille (shell32, ole32),
`SetThreadPriority(THREAD_MODE_BACKGROUND_BEGIN)`, `GetSystemPowerStatus`.

**Storage**: `library.db` (migration non destructive vers le schéma 2 :
empreintes, groupes, décisions, journal ; voir [data-model.md](data-model.md)) ;
`thumbs.db` lu seulement. Rien dans les dossiers de l'utilisateur.

**Testing**: Qt Test + CTest ; tests d'exactitude de la détection sur un jeu
de copies générées (SC-001, SC-002) ; tests d'intégration de la corbeille
réelle sur dossiers temporaires avec invariant SC-008 ; `picazoo-bench
--duplicates` (SC-003, SC-005).

**Target Platform**: Windows 10 et 11 x64. Code propre à Windows (lecteur
régulé, corbeille, alimentation, attributs « en ligne ») confiné dans
`src/core/platform/win/`.

**Project Type**: application de bureau (modules ajoutés à `picazoo_core` +
écrans Qt Quick dans `picazoo`).

**Performance Goals**: analyse complète de 20 000 photos déjà indexées en
< 30 min sur secteur ; nouvelle copie regroupée en < 1 min ; aucune
dégradation du défilement ni de l'ouverture des photos pendant l'analyse ;
ralentissement d'une copie de fichier concurrente ≤ 20 %.

**Constraints**: aucune E/S ni empreinte sur le fil de l'interface ; débit
de lecture plafonné (2–32 Mo/s adaptatif, 16 Mo/s au départ) ; jamais
d'effacement définitif ; aucune lecture des fichiers « en ligne
seulement » ; mémoire ajoutée < 50 Mo pour 100 000 éléments (empreintes
≈ 3 Mo, cache de vérification ≈ 8 Mo) ; interface en français.

**Scale/Scope**: jusqu'à 100 000 éléments et quelques milliers de groupes ;
4 écrans ou fenêtres (Doublons, Comparaison, Récapitulatif, Journal) et un
réglage.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principe | Vérification | Statut |
|---|---|---|
| I. Les fichiers de l'utilisateur sont sacrés | Seule opération : mise à la corbeille, toujours après un récapitulatif et une confirmation explicite (FR-016) ; double verrou contre l'effacement définitif (D8) ; vérification que rien n'a changé juste avant d'agir (FR-019) ; au moins une copie gardée, contrôlé par le cœur et l'interface (FR-017) ; journal écrit avant l'action et annulation par restauration depuis la corbeille (D9) ; aucune écriture dans les dossiers de l'utilisateur ; tests automatisés sur dossiers temporaires, jamais sur de vraies photos (D12). Pas de copie ni de déplacement dans la 002, donc rien à vérifier côté « fichier d'arrivée ». | ✅ |
| II. Local et privé | Aucun accès réseau ; les fichiers « en ligne seulement » ne sont pas lus, pour ne déclencher aucun téléchargement (D6). | ✅ |
| III. Fluide sur une vraie bibliothèque | Analyse en arrière-plan sur un fil à priorité d'arrière-plan, progression visible, pause, reprise par l'état en base (D10) ; lecture régulée et exclusion mutuelle avec la 001 (D5) ; partie visuelle sans lecture de fichier (D2) ; liste des groupes virtualisée. | ✅ |
| IV. Le logiciel propose, l'utilisateur décide | Groupes et copie gardée sont des suggestions avec un niveau de confiance en mots et une raison ; rien ne part sans confirmation ; décisions retenues et réversibles (D7) ; application en lot seulement pour les groupes « Identique », qui ne reposent pas sur une estimation, et toujours avec récapitulatif. | ✅ |
| V. Simple comme Picasa | Une entrée « Doublons » dans la liste des dossiers, l'écran remplace la grille dans la même fenêtre ; réglage rangé dans Outils ; textes en français. | ✅ |
| Contraintes produit | Windows d'abord ; logique de détection indépendante de la plateforme, code Windows isolé ; tous les formats d'images et RAW de la 001 comparés par leurs vignettes, vidéos par leur contenu. | ✅ |
| Cycle de travail | Spec en français, relue et clarifiée ; tests automatisés des opérations sur les fichiers sur dossiers de test. | ✅ |

**Re-check après la phase 1** : le modèle de données garde le journal
indépendant de l'index (`file_operation_entry.item_id` sans clé
étrangère, pour survivre au retrait de l'élément) ; le contrat du cœur fait
de `TrashService` le seul code autorisé à supprimer, et inscrit les
garanties « jamais d'effacement définitif », « rien sans plan confirmé » et
« au moins une copie reste » comme garanties testées. Aucune violation,
donc rien dans Complexity Tracking.

## Project Structure

### Documentation (this feature)

```text
specs/002-detection-doublons/
├── plan.md              # ce fichier
├── research.md          # phase 0 : décisions D1–D12
├── data-model.md        # phase 1 : migration 002 de library.db
├── quickstart.md        # phase 1 : tests, mesures, validation manuelle
├── contracts/
│   ├── core-api.md      # DuplicateEngine, DuplicateStore, TrashService
│   └── ui-contract.md   # écran Doublons, comparaison, récapitulatif, journal
├── checklists/
│   └── requirements.md
└── tasks.md             # phase 2 (/speckit-tasks)
```

### Source Code (repository root)

Ajouts à l'arborescence de la 001 (fichiers existants modifiés signalés
par « ~ ») :

```text
src/
├── core/
│   ├── duplicates/                # nouveau module
│   │   ├── ContentHasher          # D1 : candidats (taille, quick_hash), XXH3-128 régulé
│   │   ├── VisualFingerprint      # D2 : pHash ×4 rotations, score de détail, depuis thumbs.db
│   │   ├── CandidateSearch        # D3 : force brute popcount, tranches et reprise
│   │   ├── PairVerifier           # D4 : date EXIF, SSIM 64×64 normalisé, LumaCache
│   │   ├── Grouper                # D7 : étoile autour de la copie gardée, incrémental
│   │   ├── KeeperRule             # FR-008 : RAW, pixels, poids, EXIF, date, chemin
│   │   ├── DuplicateStore         # tables 002, lectures et décisions
│   │   └── DuplicateEngine        # D10 : fil d'arrière-plan, déclenchement, progression
│   ├── fileops/                   # nouveau module
│   │   ├── TrashService           # D8 : plan, vérification, exécution, refus
│   │   └── OperationJournal       # D9 : journal, reprise après arrêt, annulation
│   ├── index/
│   │   ├── migrations/002_doublons.sql
│   │   └── ~Migrator              # kLibrarySchemaVersion = 2
│   └── platform/win/
│       ├── ThrottledReader        # D5 : NO_BUFFERING, blocs alignés 1 Mo
│       ├── IoGovernor             # D5 : seau à jetons adaptatif (ou celui du correctif 001)
│       ├── RecycleBin             # D8/D9 : IFileOperation + puits, énumération, undelete
│       ├── PowerState             # D5 : secteur/batterie, économiseur
│       └── CloudFiles             # D6 : attributs « en ligne seulement »
└── app/
    ├── models/DuplicateGroupModel, OperationLogModel
    ├── controllers/DuplicatesController, TrashController
    ├── ~controllers/AppController # crée le moteur, relaie hold()/release() de la visionneuse
    └── qml/
        ├── Duplicates.qml, DuplicateGroupRow.qml, CompareView.qml
        ├── TrashSummary.qml, OperationLog.qml
        ├── ~FolderList.qml        # entrée « Doublons »
        ├── ~AppHeader.qml         # indicateur « Doublons : n / N »
        └── ~InfoPanel.qml         # « 2 doublons — Voir »
tests/
├── unit/                          # pHash, rotations, SSIM, KeeperRule, Grouper, IoGovernor
├── integration/                   # moteur incrémental, reprise, corbeille réelle + annulation, SC-008
├── accuracy/                      # SC-001 / SC-002 sur le jeu généré
└── support/                       # ~FixtureFactory : générateur de copies et de fausses paires
tools/
└── make-testdata/                 # picazoo-testdata.exe (quickstart)
bench/
└── ~picazoo-bench.cpp             # --duplicates, --concurrent-copy
```

**Structure Decision**: on garde le découpage de la 001 : toute la logique
dans `picazoo_core`, testable sans interface, en deux nouveaux modules
(`duplicates/` pour la détection, `fileops/` pour tout ce qui touche aux
fichiers, que la 004 réutilisera pour ses déplacements). L'application
n'ajoute que des modèles, des contrôleurs et du QML. Le code de la 001 n'est
modifié qu'aux points d'intégration listés « ~ » ; **ni l'indexeur, ni le
pipeline des vignettes, ni `FileIO`** ne sont modifiés par la 002.

## Dépendance au correctif de la 001

Un correctif de la saturation du disque au lancement de la 001 est en cours
dans une autre branche et touche probablement l'indexation et les
vignettes. Avant `/speckit-tasks` ou au plus tard avant l'implémentation :

1. rebaser cette branche sur le correctif une fois fusionné ;
2. si le correctif a introduit un régulateur d'E/S ou une priorité
   d'arrière-plan, la 002 l'adopte (D5) au lieu de créer `IoGovernor` ;
3. vérifier que les signaux utilisés par le moteur (`scanFinished`,
   `itemUpdated`, `indexerIdle()`, `pipeline().idle()`) existent toujours.

## Risques à lever en premier (spike)

| Risque | Mesure de contrôle | Plan B |
|---|---|---|
| `PreDeleteItem` ne signale pas de façon fiable un effacement définitif (D8) | essai sur clé USB, partage réseau, fichier plus gros que la corbeille, Windows 10 et 11 | n'accepter que les volumes `DRIVE_FIXED` où `SHQueryRecycleBinW` réussit **et** vérifier après coup la présence de chaque fichier dans la corbeille ; arrêter l'opération au premier écart |
| Restauration par le verbe `undelete` (D9) : boîte de dialogue en cas de conflit | essai avec un fichier du même nom recréé à l'emplacement d'origine | détecter le conflit avant d'invoquer le verbe et le signaler (`restore_conflict`) |
| Seuils pHash/SSIM (D3, D4) | SC-001 et SC-002 sur le jeu généré | pHash + dHash combinés ; SSIM multi-échelle ; seuil propre aux images sans EXIF |
| Mode d'arrière-plan du fil insuffisant pour SC-005 (D5) | copie concurrente de 2 Go pendant un hachage | indice de priorité par fichier (`FileIoPriorityHintInfo`) ; débit maximal abaissé à 16 Mo/s |
| Durée de SC-003 si la bibliothèque contient beaucoup de vidéos en double | banc d'essai avec 5 % de vidéos dupliquées | hachage des vidéos en dernier, progression séparée |

## Complexity Tracking

Aucune violation de la constitution à justifier.

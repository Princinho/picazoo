# Implementation Plan: Ergonomie des visionneuses et clic droit

**Branch**: `claude/007-clic-droit-zoom-plein-ecran` | **Date**: 2026-11-24 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/007-clic-droit-zoom-plein-ecran/spec.md`

## Summary

Cinq retouches, surtout dans les deux visionneuses (intégrée 001, autonome
005) : un bouton pour quitter le plein écran à la souris, une butée de zoom à
100 % pour les gestes commencés sous 90 %, ↑ / ↓ pour zoomer, un zoom plus
doux avec élan « à la Picasa », et des menus contextuels partout, avec mise à
la corbeille, fond d'écran et Propriétés de Windows.

Trois idées portent le plan :

1. **Un seul moteur de zoom, en C++, testé sans fenêtre.** La logique de
   zoom existe aujourd'hui en deux copies QML. Elle devient
   `core/browse/ZoomMotion` : une poursuite exponentielle de la cible en
   logarithme (τ = 75 ms, ≈ 0,3 s par pas, jamais de dépassement), une
   accélération selon la cadence des crans pour l'élan, et un « geste » qui
   porte la butée (plafond 1,0 si le geste de zoom avant part sous 0,9).
   Les deux visionneuses n'en gardent que l'affichage ([research Z1–Z6](research.md)).
2. **La corbeille reste un seul chemin.** `TrashService` apprend à mettre à
   la corbeille des éléments sans copie gardée (`planItems`), et son passage
   à la corbeille est extrait dans `FileTrash`, que la visionneuse autonome
   utilise directement sur `library.db` (ouverte seulement à ce moment-là).
   Le journal note l'origine (`file_operation.source`, schéma 4) ([T1, T2](research.md)).
3. **Les menus sont du QML, les actions du C++.** Menus Qt Quick Controls au
   thème existant ; copier, fond d'écran (`IDesktopWallpaper`, annulable) et
   Propriétés (`SHObjectProperties` / `SHMultiFileProperties`) sont des
   fonctions de `core/platform/win` ([M1, M2, W1, P1](research.md)).

## Technical Context

**Language/Version**: C++20 (MSVC, Visual Studio Build Tools 2026), QML (Qt Quick), comme 001–006

**Primary Dependencies**: celles de la 006, **sans ajout de bibliothèque** : Qt 6.12 LTS (Core, Gui, Quick, QuickControls2), SQLite 3 ; API Windows déjà disponibles dans le SDK (Shell : `IDesktopWallpaper`, `SHObjectProperties`, `SHMultiFileProperties`, `SHCreateShellItemArrayFromIDLists`).

**Storage**: `library.db`, migration non destructive vers le schéma 4
(`004_origine_operations.sql` : colonne `file_operation.source` ; voir
[data-model.md](data-model.md)). Rien dans les dossiers de l'utilisateur.

**Testing**: Qt Test + CTest. Unitaire : `tst_zoommotion` (nouveau, temps
injecté, sans fenêtre). Intégration : `tst_trash` complété, `tst_filetrash`
(nouveau, corbeille réelle sur dossiers temporaires, deux connexions WAL),
`tst_database` (migration 3 → 4). Les menus, le bouton de plein écran et le
ressenti du zoom se valident à la main ([quickstart.md](quickstart.md)).

**Target Platform**: Windows 10 et 11 x64. Le nouveau code propre à Windows
(fond d'écran, Propriétés, presse-papiers de fichiers) reste dans
`core/platform/win`, comme `RecycleBin` et `ShellActions`.

**Project Type**: application de bureau : cœur `picazoo_core`, application
`picazoo` (bibliothèque + visionneuse intégrée), visionneuse autonome
`picazoo-viewer`.

**Performance Goals**: zoom fluide à la cadence de l'écran sur la machine de
référence (Core Ultra 5 225U), y compris pendant le chargement de la pleine
résolution (FR-046) ; un pas ≈ 0,3 s, élan éteint < 0,6 s ; menu contextuel
ouvert < 0,1 s avec 1 000 éléments sélectionnés (SC-004) ; temps d'ouverture
de la visionneuse autonome inchangé (la base n'est ouverte qu'à la mise à la
corbeille).

**Constraints**: aucune E/S disque sur le fil de l'interface (corbeille,
« a une corbeille ? », décodage pour le presse-papiers, Propriétés sont hors
fil ou asynchrones) ; jamais d'effacement définitif ; coller dans
l'Explorateur copie, ne déplace jamais ; interface en français.

**Scale/Scope**: 2 visionneuses (scène de zoom refaite, menu, clavier),
1 bouton flottant, 5 surfaces de menu nouvelles ou complétées, 1 dialogue de
confirmation, 1 message « Annuler » dans la bibliothèque et dans la
visionneuse autonome ; jusqu'à 100 000 éléments dans la bibliothèque.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principe | Vérification | Statut |
|---|---|---|
| I. Les fichiers de l'utilisateur sont sacrés | La seule action qui retire un fichier est « Mettre à la corbeille… » : confirmation qui nomme le ou les fichiers, « Annuler » par défaut, corbeille de Windows uniquement (jamais d'effacement, grisé sur disque sans corbeille), journal écrit avant d'agir, revérification que le fichier n'a pas changé, annulation immédiate puis depuis le journal — y compris depuis la visionneuse autonome (T1, T2). « Copier » pose l'effet *copie* pour que coller dans l'Explorateur ne déplace jamais (M2). Le fond d'écran ne copie ni ne modifie le fichier (Windows fait sa propre copie) et s'annule (W1). Rien n'est écrit dans les dossiers de l'utilisateur. | ✅ |
| II. Local et privé | Rien ne quitte la machine. | ✅ |
| III. Fluide sur une vraie bibliothèque | Zoom cadencé par l'affichage (`FrameAnimation`), calcul en C++ ; aucune E/S sur le fil de l'interface ; visionneuse autonome qui n'ouvre pas la base au démarrage ; menus qui ne lisent que la sélection en mémoire. | ✅ |
| IV. Le logiciel propose, l'utilisateur décide | Aucune estimation nouvelle ; chaque action de menu est une demande explicite de l'utilisateur ; la butée de zoom se franchit d'un nouveau geste. | ✅ |
| V. Simple comme Picasa | Le clic droit, les flèches et le zoom avec élan reprennent Picasa ; le bouton de plein écran n'apparaît qu'au mouvement de la souris ; les actions rares sont en bas des menus ; textes en français. | ✅ |
| Contraintes produit | Code Windows confiné à `core/platform/win`, derrière des fonctions sans type Windows dans leur en-tête ; macOS / Linux restent possibles (autres implémentations des mêmes fonctions). | ✅ |
| Cycle de travail | Spec en français, relue et clarifiée ; opérations sur fichiers couvertes par des tests automatisés sur dossiers temporaires (corbeille réelle). | ✅ |

**Re-check après la phase 1** : le modèle de données n'ajoute qu'une colonne
avec valeur par défaut (migration non destructive) ; le contrat du cœur
garde `RecycleBin` comme seul code qui retire un fichier, appelé seulement
par `FileTrash`, lui-même utilisé par `TrashService` et par la visionneuse
autonome ; les opérations d'une visionneuse sont visibles et annulables dans
le journal de Picazoo. La visionneuse autonome qui écrit dans `library.db`
est un changement d'architecture assumé (pas une violation) : WAL et
`busy_timeout` existants le permettent, et la reprise après arrêt brutal
ignore les opérations de moins de 60 s pour ne pas interférer. Aucune
violation, donc rien dans Complexity Tracking.

## Project Structure

### Documentation (this feature)

```text
specs/007-clic-droit-zoom-plein-ecran/
├── plan.md              # ce fichier
├── research.md          # phase 0 : décisions Z1–Z6, M1–M2, T1–T2, W1, P1, F1
├── data-model.md        # phase 1 : migration 004, ZoomMotion, WallpaperSnapshot
├── quickstart.md        # phase 1 : tests, validation manuelle, mesures
├── contracts/
│   ├── core-api.md      # ZoomMotion / ZoomController, FileTrash, TrashService, presse-papiers, fond d'écran, Propriétés
│   └── ui-contract.md   # menus par surface, clavier, corbeille, fond d'écran, plein écran, zoom
├── checklists/
│   └── requirements.md
└── tasks.md             # phase 2 (/speckit-tasks)
```

### Source Code (repository root)

Fichiers nouveaux, et existants modifiés signalés par « ~ » :

```text
src/
├── core/
│   ├── browse/
│   │   ├── ZoomMotion           # Z1–Z5 : poursuite, élan, geste et butée (pur, temps injecté)
│   │   ├── ZoomController       # façade QObject pour QML (Picazoo.Zoom 1.0)
│   │   └── ~ZoomMath            # inchangé sur le fond ; constantes partagées
│   ├── fileops/
│   │   ├── FileTrash            # T2 : journal + corbeille d'une liste de chemins, restauration
│   │   └── ~TrashService        # planItems(), source, délégation à FileTrash, reprise > 60 s
│   ├── index/
│   │   ├── migrations/004_origine_operations.sql
│   │   └── ~Migrator            # kLibrarySchemaVersion = 4
│   ├── platform/win/
│   │   ├── ClipboardActions     # M2 : fichiers (effet copie) + image, texte
│   │   ├── Wallpaper            # W1 : IDesktopWallpaper, cliché, rétablissement
│   │   ├── ~ShellActions        # P1 : showProperties()
│   │   └── ~RecycleBin          # volumeHasRecycleBinCached()
│   └── ~CMakeLists.txt
├── app/
│   ├── ~main.cpp                # enregistre ZoomController
│   ├── ~controllers/AppController    # copier, copier le chemin, propriétés, fond d'écran (+ annuler), visionneuse autonome
│   ├── ~controllers/TrashController  # mise à la corbeille d'éléments (bibliothèque, visionneuse), titres par source
│   ├── ~controllers/ViewerController # suite après mise à la corbeille (élément voisin)
│   └── qml/
│       ├── ~Viewer.qml          # ZoomController + FrameAnimation, ↑/↓, Suppr, menu, bouton de plein écran
│       ├── ImageContextMenu.qml # menu de l'image (visionneuse intégrée)
│       ├── ItemContextMenu.qml  # menu d'une vignette ou d'une sélection
│       ├── TrashConfirm.qml     # dialogue de confirmation (Annuler par défaut)
│       ├── ~ThumbGrid.qml       # menu d'élément / de zone vide, Menu, Maj+F10, Suppr, Ctrl+C…
│       ├── ~FolderList.qml      # Afficher dans l'Explorateur, Copier le chemin
│       ├── ~Library.qml         # message « Annuler » (corbeille, fond d'écran)
│       └── ~OperationLog.qml    # origine de l'opération
└── viewer/
    ├── ~main.cpp                # enregistre ZoomController
    ├── ~ViewerSession           # corbeille (FileTrash, base ouverte à la demande), copier, propriétés, fond d'écran, suite
    └── qml/
        ├── ~Stage.qml           # un ZoomController par emplacement + FrameAnimation
        ├── ~Overlay.qml         # ↑/↓, Suppr, Menu, Maj+F10, Échap avec menu ouvert
        ├── ImageContextMenu.qml # menu de l'image (visionneuse autonome)
        ├── TrashConfirm.qml     # dialogue de confirmation
        └── ~Chrome.qml          # message « Annuler »

tests/
├── unit/tst_zoommotion.cpp             # nouveau
├── unit/~tst_database.cpp              # migration 3 → 4
├── integration/tst_filetrash.cpp       # nouveau
├── integration/~tst_trash.cpp          # planItems, source, reprise > 60 s
└── ~CMakeLists.txt
```

**Structure Decision**: on reste dans les modules existants (`browse`,
`fileops`, `platform/win`) et dans les écrans existants. Les deux unités
nouvelles du cœur qui portent des règles (`ZoomMotion`, `FileTrash`) sont
isolées pour être testées sans fenêtre ni bibliothèque complète. Les
composants QML de menu sont dupliqués par exécutable, comme `Theme.qml` et
`IconButton.qml` le sont déjà.

## Ordre de réalisation suggéré

1. US1 (bouton de plein écran) — indépendante, petite.
2. `ZoomMotion` + `tst_zoommotion`, puis branchement dans les deux
   visionneuses : US2 (butée), US3 (flèches) et US4 (douceur, élan) se
   livrent ensemble puisqu'elles partagent ce moteur.
3. US5 en deux temps : menus et actions sans fichiers (copier, chemin,
   Explorateur, Propriétés, fond d'écran) ; puis corbeille (migration 004,
   `FileTrash`, `planItems`, confirmation, journal, visionneuse autonome).

## Complexity Tracking

*Aucune violation de la constitution.*

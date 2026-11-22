# Implementation Plan: Bibliothèque et visionneuse

**Branch**: `001-bibliotheque-visionneuse` | **Date**: 2026-11-22 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/001-bibliotheque-visionneuse/spec.md`

## Summary

Construire le socle de Picazoo, comme Picasa : des dossiers surveillés
parcourus en arrière-plan, une liste des dossiers (par année ou en
arborescence), une grille continue de vignettes avec un en-tête par
dossier, une visionneuse pour les photos (y compris HEIC et RAW par leur
aperçu intégré) et pour les vidéos, et une bibliothèque tenue à jour en
direct, sans jamais toucher aux fichiers.

La **vitesse** est le critère qui guide les choix. Le projet est en C++20 et
Qt 6.12 LTS (interface Qt Quick dessinée par la carte graphique), et appelle
directement les décodeurs natifs les plus rapides : libjpeg-turbo avec
décodage réduit, libheif, LibRaw (aperçu intégré seulement). Les vignettes
intégrées aux fichiers s'affichent immédiatement, puis les vignettes de
320 px sont stockées dans SQLite. Une file à priorités suit le défilement.
Le parcours des dossiers et la surveillance passent par les API Win32. Le
détail des choix est dans [research.md](research.md).

## Technical Context

**Language/Version**: C++20 (MSVC 14.51, Visual Studio Build Tools 2026)

**Primary Dependencies**: Qt 6.12 LTS (Core, Gui, Quick, QuickControls2,
Multimedia avec moteur FFmpeg, ImageFormats) ; libjpeg-turbo (TurboJPEG 3) ;
libheif + libde265 ; LibRaw ; SQLite 3 ; TinyEXIF ; xxHash. Dépendances hors
Qt via un manifeste vcpkg (`x64-windows`, liaison dynamique).

**Storage**: SQLite (WAL) dans `%LOCALAPPDATA%\Picazoo\` : `library.db`
(index, préférences) et `thumbs.db` (vignettes JPEG en BLOB). Rien dans les
dossiers de l'utilisateur.

**Testing**: Qt Test + CTest (unitaires, intégration sur dossiers
temporaires avec contrôle de l'invariant SC-006) ; banc d'essai
`picazoo-bench` pour SC-001, SC-002 et SC-004.

**Target Platform**: Windows 10 et 11 x64. Code spécifique à Windows isolé
dans `src/core/platform/win/` pour un portage macOS/Linux ultérieur.

**Project Type**: application de bureau (cœur en bibliothèque statique +
exécutable Qt Quick).

**Performance Goals**: premiers dossiers en < 5 s ; 20 000 photos analysées
avec vignettes en < 15 min ; bibliothèque de 100 000 éléments ouverte en
< 3 s ; défilement à 60 images/s sans à-coup pendant l'analyse ; photo de
24 Mpx ouverte en < 1 s, suivante en < 0,5 s ; changement sur le disque
visible en < 10 s.

**Constraints**: hors ligne ; lecture seule stricte des fichiers de
l'utilisateur ; aucune E/S disque ni aucun décodage sur le thread de
l'interface ; mémoire des vignettes plafonnée à 256 Mo ; interface en
français.

**Scale/Scope**: jusqu'à 100 000 éléments, 5 000 dossiers ; 4 écrans
(accueil, bibliothèque, visionneuse, dossiers surveillés).

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principe | Vérification | Statut |
|---|---|---|
| I. Les fichiers de l'utilisateur sont sacrés | Accès en lecture seule et partagée ; aucune API d'écriture sur les racines surveillées ; données dans `%LOCALAPPDATA%\Picazoo` ; retirer une racine ne touche que l'index ; test d'intégration avant/après (SC-006) sur chaque scénario. La 001 n'a aucune opération sur les fichiers (pas de suppression ni de déplacement), donc pas d'aperçu ni d'annulation à prévoir. | ✅ |
| II. Local et privé | Aucune dépendance réseau ; aucune télémétrie ; Qt et vcpkg n'ajoutent pas d'appel réseau à l'exécution. | ✅ |
| III. Fluide sur une vraie bibliothèque | Analyse en arrière-plan avec pause, reprise et progression (R12) ; grille virtualisée (R9) ; file à priorités (R6) ; seuils mesurés par `picazoo-bench`. | ✅ |
| IV. Le logiciel propose, l'utilisateur décide | Sans objet dans la 001 (pas de suggestion). Le schéma garde un `item.id` stable pour les suggestions de 002 et 003. | ✅ (N/A) |
| V. Simple comme Picasa | Une fenêtre : dossiers à gauche, vignettes à droite, double-clic pour voir en grand ; écran « Dossiers surveillés » rangé dans Outils ; textes en français. | ✅ |
| Contraintes produit | Windows d'abord, Qt multiplateforme, code Win32 isolé ; tous les formats listés, RAW par aperçu intégré. | ✅ |
| Cycle de travail | Spec en français ; tests automatisés sur dossiers de test générés, jamais sur de vraies photos. | ✅ |

**Re-check après la phase 1** : le modèle de données et les contrats
confirment ces points. Le contrat du cœur
([core-api.md](contracts/core-api.md)) inscrit la lecture seule comme
garantie testée, et le data-model interdit de supprimer les éléments d'une
racine indisponible. Aucune violation, donc rien dans Complexity Tracking.

## Project Structure

### Documentation (this feature)

```text
specs/001-bibliotheque-visionneuse/
├── plan.md              # ce fichier
├── research.md          # phase 0 : décisions techniques R1–R13
├── data-model.md        # phase 1 : schéma library.db / thumbs.db
├── quickstart.md        # phase 1 : compilation, tests, validation
├── contracts/
│   ├── ui-contract.md   # écrans, raccourcis, messages
│   └── core-api.md      # frontière cœur / application
├── checklists/
│   └── requirements.md
└── tasks.md             # phase 2 (/speckit-tasks)
```

### Source Code (repository root)

```text
CMakeLists.txt
CMakePresets.json            # windows-debug, windows-release
vcpkg.json                   # libjpeg-turbo, libheif, libraw, sqlite3, xxhash, tinyexif
src/
├── core/                    # bibliothèque statique picazoo_core (QtCore seulement)
│   ├── library/             # Library (façade), réglages
│   ├── index/               # LibraryDb, ThumbStore, migrations SQL
│   ├── scan/                # Discovery, Reconciler (rattrapage R4), Watcher (R5)
│   ├── media/               # Decoder, JpegDecoder, HeifDecoder, RawPreview, QtImageDecoder, Metadata
│   ├── video/               # VideoProbe (vignette et durée, Qt Multimedia)
│   ├── jobs/                # PriorityQueue, ThumbnailPipeline
│   └── platform/win/        # FindFirstFileEx, FileId, ReadDirectoryChangesW, chemins connus
└── app/                     # exécutable picazoo.exe (Qt Quick)
    ├── main.cpp
    ├── models/              # FolderTreeModel, GridRowModel, ItemSelection
    ├── providers/           # ThumbnailImageProvider, ViewerImageProvider
    ├── controllers/         # ViewerController, OnboardingController
    └── qml/                 # Main, Library, FolderList, ThumbGrid, Viewer, VideoPlayer, WatchedFolders, Onboarding
tests/
├── unit/                    # identité, empreinte, découpage en lignes, EXIF, facteurs de réduction
├── integration/             # découverte, rattrapage, renommage, racine indisponible, invariant SC-006
├── fixtures/                # petits fichiers réels : RAW, HEIC, JPEG pivoté, MP4, MKV, fichier tronqué
└── support/                 # générateur de dossiers de test, instantané avant/après
bench/
└── picazoo-bench.cpp        # SC-001, SC-002, SC-004
third_party/                 # seulement si un paquet manque dans vcpkg (ex. TinyEXIF)
```

**Structure Decision**: un dépôt, deux cibles. `picazoo_core` contient
toute la logique, testable sans interface. Les fonctionnalités 002 à 004 y
ajouteront leurs modules (`duplicates/`, `grouping/`, `reorganize/`).
`picazoo` (l'application) ne contient que les modèles Qt Quick et le QML.
Le code propre à Windows est confiné dans `src/core/platform/win/`.

## Risques à lever en premier (spike)

| Risque | Mesure de contrôle | Plan B |
|---|---|---|
| Coût d'une vignette vidéo via Qt Multimedia (R11) | temps par vidéo sur 20 MP4/MKV | lier libavformat/libavcodec à la version du FFmpeg livré par Qt |
| Fluidité de la grille aplatie à 100 000 éléments (R9) | images/s mesurées en défilement rapide pendant l'analyse | réduire la taille des délégués, `Image.asynchronous` + textures précalculées |
| Port TinyEXIF absent de vcpkg (R2) | `vcpkg install` | copie dans `third_party/` |
| Débit de `thumbs.db` en écriture concurrente (R10) | vignettes/s dans le banc d'essai | écriture par lots de 200 sur un thread dédié |

## Écarts aux seuils de performance (T076)

| Seuil | Mesure | Justification |
|---|---|---|
| SC-001 : 20 000 photos en < 15 min | 15,26 min pour 20 000 photos de 24 Mpx bruitées ; 7,96 min pour des photos de 12 Mpx | Mesuré sur un processeur basse consommation (Core Ultra 5 225U) avec des images de pire cas pour le décodeur JPEG. Le coût est le décodage de Huffman, incompressible sans changer de source d'image. La piste retenue pour plus tard est l'aperçu MPF intégré aux JPEG des appareils photo (research.md, « Mesures »). |

## Complexity Tracking

Aucune violation de la constitution à justifier.

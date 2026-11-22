---

description: "Tâches de la fonctionnalité 001 — Bibliothèque et visionneuse"
---

# Tasks: Bibliothèque et visionneuse

**Input**: Design documents from `/specs/001-bibliotheque-visionneuse/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/ui-contract.md, contracts/core-api.md, quickstart.md

**Tests**: inclus. La constitution (principe I et « Cycle de travail »)
impose des tests automatisés sur dossiers de test pour tout ce qui touche
aux fichiers, et la vitesse est une exigence mesurée (SC-001 à SC-005).
**Chaque test d'intégration** prend un instantané des dossiers de test avant
et après, et échoue si un fichier a été créé, modifié, renommé ou supprimé
(SC-006).

**Organization**: tâches groupées par user story ; chemins relatifs à la
racine du dépôt `C:\Ideas\Picazoo`.

## Format: `[ID] [P?] [Story] Description`

- **[P]** : parallélisable (fichiers différents, pas de dépendance sur une tâche non terminée)
- **[Story]** : user story concernée (US1 à US5)

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: dépôt, chaîne de compilation, squelette des cibles.

**Prérequis manuel** : Qt 6.12 LTS (dernier correctif, kit MSVC 64 bits, avec
Qt Multimedia, Qt Image Formats, Qt Shader Tools) installé, et la variable
d'environnement `QT_DIR` pointant vers le kit (ex. `C:\Qt\6.12.x\msvc2022_64`).

- [X] T001 Initialiser le dépôt git et créer `.gitignore` (ignorer `build/`, `out/`, `.vs/`, `vcpkg_installed/`, `*.user`, `CMakeUserPresets.json`) dans `.gitignore`
- [X] T002 Créer le manifeste vcpkg avec `libjpeg-turbo`, `libheif` (avec décodeur libde265), `libraw`, `sqlite3`, `xxhash`, `tinyexif` et un `builtin-baseline` figé dans `vcpkg.json` ; si `tinyexif` n'existe pas dans vcpkg, le retirer et ajouter TinyEXIF (TinyEXIF.h/.cpp, licence BSD-2) dans `third_party/tinyexif/` (research R2)
- [X] T003 Créer le CMake racine : C++20, options MSVC `/W4 /permissive- /utf-8 /EHsc`, `find_package(Qt6 6.12 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Multimedia Test)`, `qt_standard_project_setup(REQUIRES 6.12)`, `enable_testing()`, sous-répertoires `src/core`, `src/app`, `tests`, `bench` dans `CMakeLists.txt`
- [X] T004 Créer les presets `windows-debug` et `windows-release` (générateur Ninja, toolchain vcpkg `$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake` avec repli sur le vcpkg de Visual Studio Build Tools 2026, `CMAKE_PREFIX_PATH=$env{QT_DIR}`, triplet `x64-windows`, presets de build et de test associés) dans `CMakePresets.json`
- [X] T005 [P] Créer la cible bibliothèque statique `picazoo_core` (liée à Qt6::Core, Qt6::Gui, Qt6::Multimedia, libjpeg-turbo, libheif, LibRaw, SQLite, xxHash, TinyEXIF) dans `src/core/CMakeLists.txt`
- [X] T006 [P] Créer la cible exécutable `picazoo` (Qt Quick, module QML `Picazoo`, `WIN32_EXECUTABLE`) liée à `picazoo_core` dans `src/app/CMakeLists.txt`
- [X] T007 [P] Créer les cibles de test (une par fichier `tst_*.cpp`, enregistrées dans CTest, liées à Qt6::Test et `picazoo_core`) dans `tests/CMakeLists.txt`
- [X] T008 [P] Créer la cible `picazoo-bench` (console) liée à `picazoo_core` dans `bench/CMakeLists.txt`
- [X] T009 [P] Ajouter un style de code (base Microsoft, indentation 4, colonnes 110) dans `.clang-format`
- [X] T010 [P] Lister les échantillons binaires réels nécessaires (un DNG, un CR2 ou NEF, un RAW sans aperçu, un HEIC, un MP4 H.264 de 5 s avec son, un MKV de 5 s), chacun < 5 Mo et libre de droits, avec leur source et leur licence, dans `tests/fixtures/README.md` ; les fichiers eux-mêmes sont ajoutés par le propriétaire (pas de téléchargement automatique)

**Checkpoint**: `cmake --preset windows-debug` puis `cmake --build --preset windows-debug` réussissent avec des cibles vides.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: base de données, types, décodage JPEG/Qt, files de priorité, outils de test, fenêtre vide.

**⚠️ CRITICAL**: aucune user story ne commence avant la fin de cette phase.

- [X] T011 [P] Définir les types du cœur : `ItemId`, `FolderId`, `RootId`, énumérations `Kind {image, raw, video}`, `Format {jpeg, png, gif, bmp, tiff, webp, heic, cr2, cr3, nef, arw, orf, rw2, raf, dng, mp4, mov, avi, mkv}`, `ThumbState {pending, provisional, done, unreadable, no_preview}`, `MetaState {pending, done, failed}`, `RootState {available, unavailable, denied}`, structures `Item`, `Folder`, `WatchedRoot`, `ScanProgress {phase: discovering|indexing|thumbnails|idle, done, total, paused}` selon data-model.md, dans `src/core/library/Types.h`
- [X] T012 [P] Implémenter la normalisation des chemins (séparateurs `\`, sans `\` final, casse conservée, comparaison insensible à la casse, préfixe `\\?\` pour les chemins > 260 caractères, test « est à l'intérieur de ») dans `src/core/platform/win/Paths.h` et `src/core/platform/win/Paths.cpp`
- [X] T013 [P] Implémenter la table extension → `Format`/`Kind` (extensions de FR-003 et FR-004, insensible à la casse, `.jpg`/`.jpeg`/`.jpe`, `.tif`/`.tiff`, `.heic`/`.heif`) dans `src/core/media/Formats.h` et `src/core/media/Formats.cpp`
- [X] T014 Implémenter l'enveloppe SQLite (ouverture avec `journal_mode=WAL`, `synchronous=NORMAL`, `foreign_keys=ON`, `busy_timeout=5000` ; RAII `Statement` avec liaisons typées ; `Transaction` ; une connexion d'écriture et des connexions de lecture) dans `src/core/index/Database.h` et `src/core/index/Database.cpp`
- [X] T015 Écrire la migration initiale de `library.db` exactement selon data-model.md : tables `schema_version`, `watched_root` (`path` TEXT UNIQUE, `state` `available`|`unavailable`|`denied`), `excluded_path` (`root_id` FK ON DELETE CASCADE), `folder` (`path` UNIQUE), `item` (tous les champs, `orientation` défaut 1, `meta_state` et `thumb_state` défaut `pending`, `version` défaut 1), `settings` (`key` TEXT PK) ; index `(folder_id, sort_date)`, `(folder_id, file_name)`, `(volume_serial, file_id)`, `(quick_hash, size_bytes)`, `sort_date` — dans `src/core/index/migrations/001_initial.sql`, appliquée par `src/core/index/Migrator.cpp` (fichiers SQL embarqués en ressource Qt)
- [X] T016 Implémenter le stockage des vignettes `thumbs.db` (table `thumb`, clé primaire `(item_id, kind)`, `kind` `provisional`|`final`, BLOB JPEG ; `get(item_id, kind, version)` renvoie vide si la version ne correspond pas ; `put`, `remove(item_ids)`, écriture par lots) dans `src/core/index/ThumbStore.h` et `src/core/index/ThumbStore.cpp`
- [X] T017 [P] Implémenter les préférences avec leurs valeurs par défaut (`thumb_size`=`160` borné à 64–320, `folder_sort`=`date_desc` parmi `date_desc|date_asc|name_asc`, `item_sort`=`taken_asc` parmi `taken_asc|name_asc`, `folder_view`=`by_year` parmi `by_year|tree`, `last_folder_id` vide, `onboarding_done`=`0`) dans `src/core/library/Settings.h` et `src/core/library/Settings.cpp`
- [X] T018 [P] Implémenter la lecture des métadonnées EXIF via TinyEXIF (date de prise de vue → epoch ms heure locale sans fuseau, orientation 1–8, « Marque Modèle », dimensions, position et taille de la vignette EXIF intégrée) en lisant au plus les 256 premiers Ko du fichier, dans `src/core/media/Metadata.h` et `src/core/media/Metadata.cpp`
- [X] T019 Implémenter le décodeur JPEG TurboJPEG 3 : choix du plus petit facteur de réduction (1/8, 1/4, 1/2, 1) qui reste ≥ la taille cible, décodage réduit, application de l'orientation EXIF (8 cas), encodage JPEG qualité 85 pour les vignettes, décodage d'une région (rognage) pour les tuiles, dans `src/core/media/JpegDecoder.h` et `src/core/media/JpegDecoder.cpp`
- [X] T020 [P] Implémenter le décodeur des formats Qt (PNG, GIF première image, BMP, TIFF, WebP) via `QImageReader::setScaledSize`, avec orientation, dans `src/core/media/QtImageDecoder.h` et `src/core/media/QtImageDecoder.cpp`
- [X] T021 Implémenter la façade `Decoder` (`decode`, `embeddedThumbnail`, `readMetadata` selon contracts/core-api.md) qui aiguille par `Format` ; HEIC et RAW renvoient « non pris en charge » tant que T034 et T035 ne sont pas faits ; dans `src/core/media/Decoder.h` et `src/core/media/Decoder.cpp`
- [X] T022 [P] Implémenter la file à priorités à trois niveaux (visible, proche, reste), avec reclassement d'un lot d'identifiants, rétrogradation sans annulation, pause et reprise, et un pool de calcul de (cœurs − 1) threads, dans `src/core/jobs/PriorityQueue.h` et `src/core/jobs/PriorityQueue.cpp`
- [X] T023 [P] Créer les outils de test : `FsSnapshot` (chemins, tailles, dates, hachage XXH3 de chaque fichier d'un dossier ; comparaison avec un message listant les différences), `TempTree` (dossier temporaire supprimé en fin de test), `FixtureFactory` (génère des JPEG avec orientation EXIF et date choisies, des PNG, un JPEG tronqué, des fichiers cachés, `Thumbs.db`, une jonction) dans `tests/support/FsSnapshot.h`, `tests/support/TempTree.h` et `tests/support/FixtureFactory.h` (et leurs `.cpp`)
- [X] T024 [P] Tests unitaires de la base : migration sur base vide, contraintes UNIQUE et CASCADE, valeurs par défaut des préférences, dans `tests/unit/tst_database.cpp`
- [X] T025 [P] Tests unitaires du décodage : choix du facteur de réduction (24 Mpx → écran 1920×1080 donne 1/2), 8 orientations, table des formats, lecture EXIF sur des fichiers de `FixtureFactory`, dans `tests/unit/tst_decoding.cpp`
- [X] T026 Créer le point d'entrée : `QGuiApplication`, création de `%LOCALAPPDATA%\Picazoo\`, ouverture de `Library` (stub), `QQmlApplicationEngine` chargeant une fenêtre vide titrée « Picazoo », style Qt Quick Controls `Fusion` sombre, dans `src/app/main.cpp` et `src/app/qml/Main.qml`

**Checkpoint**: `ctest --preset windows-debug` passe ; `picazoo.exe` ouvre une fenêtre vide.

---

## Phase 3: User Story 1 - Retrouver toutes ses photos sans rien importer (Priority: P1) 🎯 MVP

**Goal**: choisir les dossiers surveillés ; analyse en arrière-plan ; liste des dossiers par année à gauche ; grille continue avec en-têtes à droite ; vignettes de tous les formats d'images (dont HEIC et RAW) ; écran Dossiers surveillés ; réouverture immédiate.

**Independent Test**: dossier de test avec 3 sous-dossiers de photos + une vidéo + un sous-dossier vide ; premier lancement, choisir ce dossier → les 3 sous-dossiers apparaissent (pas le vide) avec les bonnes vignettes et les bons nombres.

### Tests for User Story 1 ⚠️

> Écrire ces tests d'abord et vérifier qu'ils échouent.

- [X] T027 [P] [US1] Test d'intégration de la découverte : 3 sous-dossiers de photos, 1 vidéo, 1 dossier vide, fichiers cachés, `Thumbs.db`, `desktop.ini`, une jonction vers un dossier parent, une racine imbriquée dans une autre, un sous-dossier exclu → dossiers et nombres attendus, aucune image en double, jonction non suivie, invariant `FsSnapshot`, dans `tests/integration/tst_discovery.cpp`
- [X] T028 [P] [US1] Test d'intégration des dossiers surveillés : ajout, exclusion, retrait d'une racine (ses `folder`/`item`/vignettes quittent l'index, ceux d'une autre racine qui les couvre restent), dossier sans droit d'accès → `state = denied` sans échec de l'analyse, invariant `FsSnapshot`, dans `tests/integration/tst_roots.cpp`
- [X] T029 [P] [US1] Test d'intégration de la reprise : interrompre l'analyse à mi-parcours (destruction de `Library`), rouvrir → aucune corruption, les éléments `pending` sont repris, les dossiers déjà connus s'affichent sans nouvelle découverte, dans `tests/integration/tst_resume.cpp`
- [X] T030 [P] [US1] Test unitaire du modèle de grille aplati : lignes d'en-tête et de vignettes pour plusieurs largeurs et tailles, position y d'un dossier, recherche de la ligne à une position y en O(log n), dans `tests/unit/tst_gridrowmodel.cpp`

### Implementation for User Story 1

- [X] T031 [P] [US1] Implémenter le parcours Win32 `FindFirstFileExW` (`FindExInfoBasic`, `FIND_FIRST_EX_LARGE_FETCH`), en largeur, qui renvoie nom, taille, date de modification et attributs, et ignore `FILE_ATTRIBUTE_HIDDEN`, `FILE_ATTRIBUTE_SYSTEM`, `FILE_ATTRIBUTE_REPARSE_POINT`, `Thumbs.db` et `desktop.ini` (research R3), dans `src/core/platform/win/DirectoryWalker.h` et `src/core/platform/win/DirectoryWalker.cpp`
- [X] T032 [P] [US1] Implémenter l'identité de fichier : ouverture `CreateFileW` avec `FILE_READ_ATTRIBUTES` et partage `FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE`, `GetFileInformationByHandleEx(FileIdInfo)` → `(volume_serial, file_id)` ; empreinte rapide XXH3-64 de (taille + 64 Ko du début + 64 Ko de la fin) (research R4), dans `src/core/platform/win/FileIdentity.h` et `src/core/platform/win/FileIdentity.cpp`
- [X] T033 [P] [US1] Implémenter les emplacements par défaut Images, Bureau, Documents via `SHGetKnownFolderPath` (FR-001) dans `src/core/platform/win/KnownFolders.h` et `src/core/platform/win/KnownFolders.cpp`
- [X] T034 [P] [US1] Implémenter le décodeur HEIC avec libheif : vignette intégrée (`heif_image_handle_get_list_of_thumbnail_IDs`), image principale réduite à la taille cible, bloc EXIF transmis à `Metadata`, dans `src/core/media/HeifDecoder.h` et `src/core/media/HeifDecoder.cpp`
- [X] T035 [P] [US1] Implémenter l'aperçu RAW avec LibRaw (`open_file` + `unpack_thumb`, aperçu JPEG décodé par `JpegDecoder`, aperçu bitmap converti ; pas de `dcraw_process`) ; sans aperçu exploitable → résultat `no_preview` (FR-004) ; métadonnées (date, appareil, orientation via `flip`) dans `src/core/media/RawPreview.h` et `src/core/media/RawPreview.cpp`, puis brancher HEIC et RAW dans `src/core/media/Decoder.cpp`
- [X] T036 [US1] Implémenter le dépôt de l'index : écriture par lots de 1 000 dans une transaction (racines, exclusions, dossiers avec `item_count` et `date` = date de l'élément le plus ancien, éléments), suppression d'un dossier quand son dernier élément disparaît, requêtes de lecture (dossiers triés `date_desc|date_asc|name_asc`, éléments d'un dossier triés `taken_asc|name_asc` sur `sort_date` ou `file_name`), dans `src/core/index/LibraryDb.h` et `src/core/index/LibraryDb.cpp`
- [X] T037 [US1] Implémenter la découverte : racines normalisées puis dédupliquées (une racine contenue dans une autre n'est pas parcourue deux fois), exclusions, filtre par extension (T013), publication d'un lot par dossier terminé, identité et empreinte calculées sur un thread d'E/S dédié, racine sans accès → `denied`, dans `src/core/scan/Discovery.h` et `src/core/scan/Discovery.cpp`
- [X] T038 [US1] Implémenter le pipeline des vignettes : (1) vignette intégrée → `thumb_state = provisional` ; (2) vignette finale de 320 px sur le grand côté, orientation appliquée, JPEG qualité 85 → `done` ; fichier illisible → `unreadable` ; RAW sans aperçu → `no_preview` ; métadonnées → `meta_state` ; priorités pilotées par `prioritize(visible, nearby)` ; pause et reprise (FR-005) ; reprise des `pending` à l'ouverture, dans `src/core/jobs/ThumbnailPipeline.h` et `src/core/jobs/ThumbnailPipeline.cpp`
- [X] T039 [US1] Implémenter la façade `Library` selon contracts/core-api.md (open, roots, addRoot, removeRoot qui ne touche qu'à l'index, excludePath, includePath, defaultRoots, startScan, pauseScan, resumeScan, progress, folders, items, item, prioritize, thumbnail, setting, setSetting ; signaux `foldersChanged` et `itemsChanged` regroupés au plus toutes les 250 ms, `itemUpdated`, `rootStateChanged`, `progressChanged`, `storageError`), lectures sans attente sur l'indexeur, dans `src/core/library/Library.h` et `src/core/library/Library.cpp`
- [X] T040 [P] [US1] Implémenter le fournisseur de vignettes asynchrone `image://thumb/<id>/<version>` : cache LRU de `QImage` plafonné à 256 Mo, puis `thumbs.db`, sinon image provisoire ou de remplacement (« image illisible », « aperçu indisponible », vidéo), dans `src/app/providers/ThumbnailImageProvider.h` et `src/app/providers/ThumbnailImageProvider.cpp`
- [X] T041 [P] [US1] Implémenter le modèle de la liste des dossiers en mode « Par année » (intitulés d'année selon `folder.date`, nom, nombre, date, état indisponible), dans `src/app/models/FolderTreeModel.h` et `src/app/models/FolderTreeModel.cpp`
- [X] T042 [US1] Implémenter le modèle de grille aplati (lignes en-tête / rangées de N vignettes d'un même dossier, N = largeur / (taille + marge), hauteurs connues, `rowAtY`, `yOfFolder`, recalcul sur changement de largeur, de taille, de tri ou de contenu ; envoie les identifiants visibles et proches à `Library::prioritize`) dans `src/app/models/GridRowModel.h` et `src/app/models/GridRowModel.cpp`
- [X] T043 [US1] Implémenter la sélection simple (un élément courant, déplacement aux flèches entre rangées et dossiers) utilisée par la grille et la visionneuse, dans `src/app/models/ItemSelection.h` et `src/app/models/ItemSelection.cpp`
- [X] T044 [US1] Créer l'écran Bibliothèque : `SplitView` avec la liste des dossiers à gauche (clic → défilement jusqu'au dossier) et la grille à droite (`ListView` sur `GridRowModel`, délégués d'en-tête avec nom, date, nombre et chemin au survol, rangées d'`Image` asynchrones), barre du haut avec menus Fichier / Affichage / Outils, dans `src/app/qml/Library.qml`, `src/app/qml/FolderList.qml` et `src/app/qml/ThumbGrid.qml`
- [X] T045 [P] [US1] Créer l'indicateur d'analyse discret (« Analyse : 4 210 / 20 000 » + bouton pause/reprise, invisible au repos) dans `src/app/qml/ScanIndicator.qml`
- [X] T046 [US1] Créer l'accueil du premier lancement (« Où sont vos photos ? », « Images, Bureau et Documents (recommandé) », « Choisir des dossiers… » avec sélecteur de dossiers, « Commencer » → ajoute les racines, lance l'analyse et met `onboarding_done` à `1`) dans `src/app/controllers/OnboardingController.h`, `src/app/controllers/OnboardingController.cpp` et `src/app/qml/Onboarding.qml`
- [X] T047 [US1] Créer l'écran Dossiers surveillés (menu Outils › Dossiers surveillés… ; liste des racines avec leur état ; ajouter, retirer, exclure un sous-dossier ; confirmation de retrait exactement : « Retirer « X » de Picazoo ? Ses photos ne seront plus affichées. Aucun fichier ne sera supprimé de votre ordinateur. » ; racine refusée : « Accès refusé — ce dossier est ignoré. ») dans `src/app/qml/WatchedFolders.qml`
- [X] T048 [P] [US1] Implémenter « Afficher dans l'Explorateur » (`SHOpenFolderAndSelectItems`) et « Ouvrir avec l'application par défaut » (`QDesktopServices::openUrl`) (FR-022, FR-023) dans `src/core/platform/win/ShellActions.h` et `src/core/platform/win/ShellActions.cpp`, et le menu contextuel « Ouvrir / Afficher dans l'Explorateur / Ouvrir avec l'application par défaut » dans `src/app/qml/ThumbGrid.qml`
- [X] T049 [US1] Brancher `storageError` sur le message « Picazoo ne peut plus enregistrer sa bibliothèque (espace disque insuffisant ?). Vos photos ne sont pas concernées. » et afficher la bibliothèque depuis `library.db` dès l'ouverture, sans relancer de découverte complète, dans `src/app/main.cpp` et `src/app/qml/Main.qml`
- [X] T050 [US1] Créer le banc d'essai : génération de N JPEG de 12–24 Mpx répartis dans des dossiers d'un répertoire temporaire (supprimé à la fin), mesure du délai avant le premier dossier publié (SC-001 : < 5 s), de la durée de l'analyse complète avec vignettes pour 20 000 éléments (< 15 min), du temps d'ouverture d'une bibliothèque déjà analysée de 100 000 éléments (`--warm`, SC-002 : < 3 s), sortie en tableau texte, dans `bench/picazoo-bench.cpp`

**Checkpoint**: US1 complète ; le test indépendant passe ; `picazoo-bench --items 20000` respecte SC-001. Mesurer la fluidité du défilement à 100 000 éléments pendant une analyse (risque R9 du plan) et noter le résultat dans `specs/001-bibliotheque-visionneuse/research.md`.

---

## Phase 4: User Story 2 - Voir une photo en grand et passer d'une photo à l'autre (Priority: P1)

**Goal**: visionneuse : ajustement à la fenêtre, précédent/suivant d'un dossier à l'autre, zoom et déplacement, plein écran, informations, Explorateur, retour sur la bonne vignette.

**Independent Test**: dans un dossier de 10 photos, ouvrir la 3e, 4 × → (7e), zoomer, Échap → retour à la bibliothèque avec la 7e vignette visible et sélectionnée.

**Dépend de** : US1 (il faut une bibliothèque à regarder).

### Tests for User Story 2 ⚠️

- [X] T051 [P] [US2] Test unitaire de la navigation : suivant après le dernier élément d'un dossier → premier du dossier suivant, précédent avant le premier → dernier du dossier précédent, bornes de la bibliothèque, préchargement ±1 et ±2, dans `tests/unit/tst_viewer_navigation.cpp`

### Implementation for User Story 2

- [X] T052 [US2] Implémenter le contrôleur de visionneuse (élément courant, suivant et précédent à travers les dossiers, préchargement de ±1 et ±2 à la taille de l'écran, cache de 5 images, mise en pause du pool de vignettes pendant le décodage de l'image affichée, élément de retour) dans `src/app/controllers/ViewerController.h` et `src/app/controllers/ViewerController.cpp`
- [X] T053 [US2] Implémenter le fournisseur d'images de la visionneuse : vignette de 320 px immédiate, puis décodage à la taille de l'écran (facteur TurboJPEG le plus proche par excès), pleine résolution au premier zoom au-delà de 100 % de l'aperçu, tuiles (décodage de régions) pour les images de plus de 50 Mpx (research R8), dans `src/app/providers/ViewerImageProvider.h` et `src/app/providers/ViewerImageProvider.cpp`
- [X] T054 [US2] Créer la visionneuse : fond sombre, ajustement à la fenêtre sans déformation, molette = zoom centré sur le pointeur, Ctrl + / Ctrl − = zoom, glisser pour se déplacer, `0` = ajuster, `1` = 100 %, ← → et boutons à l'écran, `F11` ou `F` = plein écran, Échap = retour, barre d'outils (Retour à la bibliothèque, précédent, suivant, zoom, plein écran, informations, Afficher dans l'Explorateur), GIF animés joués via `AnimatedImage`, dans `src/app/qml/Viewer.qml`
- [X] T055 [P] [US2] Créer le panneau d'informations (touche `I` ; nom, chemin, date de prise de vue ou à défaut date du fichier, dimensions, poids lisible en Ko/Mo, appareil ; champs inconnus masqués) (FR-020) dans `src/app/qml/InfoPanel.qml`
- [X] T056 [US2] Brancher l'ouverture depuis la bibliothèque (double-clic ou Entrée → visionneuse sur l'élément) et le retour (Échap → bibliothèque, défilement jusqu'à la vignette du dernier élément regardé et sélection) (FR-016, FR-024) dans `src/app/qml/Library.qml` et `src/app/qml/Main.qml`
- [X] T057 [US2] Pour un élément non affichable, afficher « Picazoo ne sait pas afficher ce fichier. » et un bouton « Ouvrir avec l'application par défaut » (FR-022) dans `src/app/qml/Viewer.qml`

**Checkpoint**: US1 et US2 fonctionnent ; ouverture d'une photo de 24 Mpx < 1 s, suivante < 0,5 s (SC-004, mesure manuelle en attendant T075).

---

## Phase 5: User Story 3 - Regarder ses vidéos dans Picazoo (Priority: P2)

**Goal**: vignettes vidéo avec ▶ et durée ; lecture intégrée avec son, pause, déplacement, volume, plein écran ; message et ouverture externe si illisible.

**Independent Test**: dossier avec 2 photos et un MP4 de 30 s : la vidéo a une vignette avec sa durée ; double-clic → lecture avec son ; avancer à la moitié, pause ; → passe à la photo suivante.

**Dépend de** : US1 (grille) et US2 (visionneuse).

### Tests for User Story 3 ⚠️

- [X] T058 [P] [US3] Tests unitaires du format de durée (`m:ss` sous une heure, `h:mm:ss` au-delà ; 0, 59 s, 61 s, 3 600 s) et test d'intégration de `VideoProbe` sur le MP4 et le MKV de `tests/fixtures/` (durée à ±100 ms, image non vide), dans `tests/unit/tst_video.cpp`

### Implementation for User Story 3

- [X] T059 [US3] Implémenter `VideoProbe` : `QMediaPlayer` sans affichage sur un `QThread` dédié avec sa boucle d'événements, branché sur un `QVideoSink`, positionnement à 10 % de la durée, capture d'une image, lecture de la durée dans les métadonnées, délai maximal de 5 s par vidéo puis `unreadable` (research R11), dans `src/core/video/VideoProbe.h` et `src/core/video/VideoProbe.cpp`
- [X] T060 [US3] Brancher `VideoProbe` dans le pipeline pour `kind = video` (vignette finale 320 px, `duration_ms`) dans `src/core/jobs/ThumbnailPipeline.cpp`
- [ ] T061 [US3] Mesurer le coût moyen d'une vignette vidéo sur 20 MP4/MKV (option `--videos` du banc d'essai) dans `bench/picazoo-bench.cpp` ; noter le résultat dans `specs/001-bibliotheque-visionneuse/research.md` (R11) ; au-delà de 500 ms par vidéo, ouvrir une tâche de suivi pour lier libavformat/libavcodec directement
- [X] T062 [P] [US3] Ajouter sur les vignettes vidéo le symbole ▶ et la durée (FR-013) dans `src/app/qml/ThumbGrid.qml`
- [X] T063 [US3] Créer le lecteur vidéo (`MediaPlayer` + `VideoOutput` + `AudioOutput` ; lecture/pause au bouton et à `Espace`, barre de progression déplaçable, volume, plein écran ; ← → passent à l'élément voisin et arrêtent la lecture ; erreur de lecture → « Picazoo ne sait pas afficher ce fichier. » + « Ouvrir avec l'application par défaut ») dans `src/app/qml/VideoPlayer.qml`, intégré à `src/app/qml/Viewer.qml` pour `kind = video`

**Checkpoint**: US1 à US3 fonctionnent ; le MKV de test se lit avec le son.

---

## Phase 6: User Story 4 - Une bibliothèque qui se tient à jour toute seule (Priority: P2)

**Goal**: changements visibles en direct en < 10 s ; rattrapage au démarrage ; renommages et déplacements reconnus ; racines indisponibles conservées.

**Independent Test**: Picazoo ouvert sur un dossier de test ; copier 5 photos, en supprimer 1, en renommer 1 dans l'Explorateur → en < 10 s la bibliothèque montre les 5 nouvelles, plus la supprimée, la renommée sous son nouveau nom.

**Dépend de** : US1.

### Tests for User Story 4 ⚠️

- [X] T064 [P] [US4] Test d'intégration du rattrapage (application fermée) appliquant les 5 règles de data-model.md : inchangé ; même chemin avec taille ou date changée → `version + 1` et `thumb_state = pending` ; renommage et déplacement entre racines → même `item.id` (par `(volume_serial, file_id)` puis par `(quick_hash, size_bytes)`) ; suppression ; racine `unavailable` → **aucun** élément supprimé ; invariant `FsSnapshot` sur les fichiers non modifiés par le test, dans `tests/integration/tst_reconcile.cpp`
- [X] T065 [P] [US4] Test d'intégration de la surveillance en direct : ajout de 5 fichiers, suppression, renommage → index à jour en < 10 s ; fichier écrit par morceaux pendant 3 s → n'apparaît qu'une fois stable ; débordement simulé → nouveau parcours de la racine, dans `tests/integration/tst_watcher.cpp`

### Implementation for User Story 4

- [X] T066 [US4] Implémenter le rattrapage selon les règles 1 à 5 de data-model.md (comparaison `(chemin, size_bytes, mtime)`, rapprochement des nouveaux chemins avec les éléments disparus, `last_seen_at`, nettoyage des vignettes des éléments supprimés, aucune suppression pour une racine `unavailable`) dans `src/core/scan/Reconciler.h` et `src/core/scan/Reconciler.cpp`
- [X] T067 [US4] Implémenter la surveillance Win32 : un `ReadDirectoryChangesW` récursif par racine en E/S asynchrones (tampon de 64 Ko, filtres nom de fichier, nom de dossier, taille, date d'écriture), signal de débordement, dans `src/core/platform/win/DirectoryWatcher.h` et `src/core/platform/win/DirectoryWatcher.cpp`
- [X] T068 [US4] Implémenter le service de suivi : événements regroupés pendant 500 ms puis rattrapage limité aux dossiers touchés ; fichier en cours de copie (taille qui change ou ouverture partagée refusée) réessayé jusqu'à stabilité ; débordement → parcours de la racine ; racine inaccessible → `unavailable`, nouvel essai toutes les 30 s et sur `WM_DEVICECHANGE` (research R5) dans `src/core/scan/Watcher.h` et `src/core/scan/Watcher.cpp`
- [X] T069 [US4] Brancher dans `Library` : à l'ouverture, affichage depuis l'index puis rattrapage en arrière-plan ; démarrage du suivi ; `F5` = réanalyse ; transmission de `WM_DEVICECHANGE` via un `QAbstractNativeEventFilter` dans `src/core/library/Library.cpp` et `src/app/main.cpp`
- [X] T070 [US4] Afficher les racines indisponibles grisées avec « (indisponible) » dans la liste des dossiers, et dans Dossiers surveillés le message « Disque ou dossier introuvable. Il sera de nouveau pris en compte dès qu'il sera accessible. » (FR-009) dans `src/app/qml/FolderList.qml` et `src/app/qml/WatchedFolders.qml`

**Checkpoint**: US1 à US4 fonctionnent ; SC-005 vérifié par `tst_watcher`.

---

## Phase 7: User Story 5 - Parcourir à sa façon (Priority: P3)

**Goal**: taille des vignettes, tris, mode arborescence, sélection multiple, réglages conservés.

**Independent Test**: vignettes au plus grand, dossiers triés par nom, liste en arborescence ; fermer et rouvrir → les trois réglages sont conservés.

**Dépend de** : US1.

### Tests for User Story 5 ⚠️

- [X] T071 [P] [US5] Tests unitaires : arborescence calculée à partir de `parent_path` (dossiers intermédiaires sans élément affichés, branches sans élément masquées) ; sélection multiple (clic, Ctrl+clic, Maj+clic sur plusieurs dossiers, Ctrl+A) ; persistance des réglages après réouverture de `Library`, dans `tests/unit/tst_browsing.cpp`

### Implementation for User Story 5

- [X] T072 [P] [US5] Ajouter le mode « Arborescence » (dossiers tels que rangés sur le disque à partir des racines, dépliables, branches menant à des éléments seulement) et la bascule « Par année | Arborescence », avec conservation du dossier sélectionné et de `folder_view` (FR-011) dans `src/app/models/FolderTreeModel.cpp` et `src/app/qml/FolderList.qml`
- [X] T073 [P] [US5] Ajouter le curseur de taille des vignettes (64 à 320 px, Ctrl+molette, Ctrl + / Ctrl −, effet immédiat, enregistré dans `thumb_size`) et les menus de tri des dossiers et des éléments (enregistrés dans `folder_sort` et `item_sort`) dans `src/app/qml/Library.qml` et `src/app/qml/ThumbGrid.qml`
- [X] T074 [US5] Étendre la sélection à la sélection multiple (Ctrl+clic, Maj+clic, sélection au cadre, Ctrl+A sur le dossier courant, Échap vide la sélection) et afficher le nombre d'éléments sélectionnés dans la barre du bas (FR-015) dans `src/app/models/ItemSelection.cpp` et `src/app/qml/ThumbGrid.qml`

**Checkpoint**: toutes les user stories fonctionnent indépendamment.

---

## Phase 8: Polish & Cross-Cutting Concerns

**Purpose**: performances mesurées, déploiement, validation finale.

- [X] T075 Étendre le banc d'essai à SC-004 (ouverture d'une photo de 24 Mpx < 1 s, suivante < 0,5 s, via `ViewerImageProvider` sans interface) et à SC-005 (délai entre la copie d'un fichier et `itemsChanged` < 10 s), dans `bench/picazoo-bench.cpp` ; consigner les résultats des cinq critères dans `specs/001-bibliotheque-visionneuse/research.md`
- [X] T076 Optimiser ce que le banc d'essai fait échouer (profilage avec l'analyseur de performances de Visual Studio) ; toute exception aux seuils est justifiée dans `specs/001-bibliotheque-visionneuse/plan.md`
- [X] T077 [P] Ajouter l'étape d'installation et de déploiement : `qt_generate_deploy_qml_app_script` + copie des DLL vcpkg et des DLL FFmpeg de Qt Multimedia, pour obtenir un dossier `picazoo` autonome, dans `src/app/CMakeLists.txt`
- [X] T078 [P] Vérifier que tous les textes visibles sont en français et identiques à contracts/ui-contract.md, et que chaque raccourci du tableau du contrat fonctionne, dans `src/app/qml/`
- [X] T079 [P] Écrire le README (présentation, prérequis, compilation, tests, banc d'essai, emplacement des données `%LOCALAPPDATA%\Picazoo`) dans `README.md`
- [ ] T080 Dérouler intégralement `specs/001-bibliotheque-visionneuse/quickstart.md` (compilation, tests, banc d'essai, 7 étapes de validation manuelle dont l'instantané SC-006) et cocher le résultat dans `specs/001-bibliotheque-visionneuse/checklists/requirements.md`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)** : aucune dépendance, sauf Qt 6.12 installé à la main.
- **Foundational (Phase 2)** : dépend de la phase 1, bloque toutes les user stories.
- **US1 (Phase 3)** : dépend de la phase 2. C'est le MVP.
- **US2 (Phase 4)** : dépend de US1.
- **US3 (Phase 5)** : dépend de US1 et US2.
- **US4 (Phase 6)** : dépend de US1 ; peut se faire en parallèle de US2 et US3.
- **US5 (Phase 7)** : dépend de US1 ; peut se faire en parallèle de US2 à US4.
- **Polish (Phase 8)** : dépend des stories retenues.

```text
Setup ─► Foundational ─► US1 ─┬─► US2 ─► US3 ─┐
                              ├─► US4 ────────┼─► Polish
                              └─► US5 ────────┘
```

### Within Each User Story

- Tests écrits d'abord, et ils doivent échouer.
- Couche plateforme et décodeurs → index → services du cœur (`Discovery`, pipeline, `Library`) → modèles Qt → QML.
- Story terminée et vérifiée avant la suivante dans l'ordre de priorité.

### Parallel Opportunities

- Phase 1 : T005 à T010 ensemble, après T003 et T004.
- Phase 2 : T011, T012, T013, T017, T018, T020, T022, T023 ensemble ; puis T024 et T025.
- US1 : tests T027 à T030 ensemble ; T031 à T035 ensemble ; T040 et T041 ensemble une fois T039 fait ; T045 et T048 à tout moment après T039.
- Après US1 : US4 et US5 peuvent avancer en parallèle de US2 (fichiers distincts, sauf `ThumbGrid.qml` et `FolderList.qml`, à faire l'un après l'autre).

---

## Parallel Example: User Story 1

```text
# Tests de US1 ensemble :
Task: "Test d'intégration de la découverte dans tests/integration/tst_discovery.cpp"
Task: "Test d'intégration des dossiers surveillés dans tests/integration/tst_roots.cpp"
Task: "Test d'intégration de la reprise dans tests/integration/tst_resume.cpp"
Task: "Test unitaire du modèle de grille dans tests/unit/tst_gridrowmodel.cpp"

# Couche plateforme et décodeurs de US1 ensemble :
Task: "Parcours Win32 dans src/core/platform/win/DirectoryWalker.cpp"
Task: "Identité de fichier dans src/core/platform/win/FileIdentity.cpp"
Task: "Emplacements par défaut dans src/core/platform/win/KnownFolders.cpp"
Task: "Décodeur HEIC dans src/core/media/HeifDecoder.cpp"
Task: "Aperçu RAW dans src/core/media/RawPreview.cpp"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Phase 1 (Setup), puis Phase 2 (Foundational).
2. Phase 3 (US1).
3. **STOP and VALIDATE** : test indépendant de US1, `picazoo-bench --items 20000`, mesure de fluidité à 100 000 éléments (risque R9).
4. Démo : Picazoo trouve et montre toutes tes photos.

### Incremental Delivery

1. US1 → bibliothèque visible (MVP).
2. US2 → on regarde ses photos en grand : c'est déjà un « Picasa » utilisable.
3. US3 → vidéos.
4. US4 → mise à jour automatique.
5. US5 → confort de navigation.
6. Polish → mesures complètes et dossier autonome.

---

## Notes

- [P] = fichiers différents, pas de dépendance non terminée.
- Aucune tâche n'écrit dans les dossiers de l'utilisateur ; les tests travaillent dans des dossiers temporaires (`TempTree`) et contrôlent l'invariant SC-006.
- Faire un commit après chaque tâche ou groupe logique.
- S'arrêter à chaque checkpoint pour valider la story indépendamment.

---

## Notes d'implémentation (2026-11-22)

- **T053** : le décodage par tuiles des images de plus de 50 Mpx n'est pas fait.
  L'image « pleine résolution » est décodée entière, plafonnée à 16 384 px sur le
  grand côté (limite des textures). À reprendre si de très grands panoramas posent
  problème.
- **T061** : la mesure du coût d'une vignette vidéo (`picazoo-bench --videos <dossier>`)
  attend les échantillons vidéo de `tests/fixtures/` ; non mesurée.
- **T076** : mesures dans research.md (« Mesures ») ; le nombre de threads de calcul tient
  compte des processeurs hybrides (`platform::recommendedWorkerThreads`). Seul écart :
  SC-001 à +2 % sur des photos de 24 Mpx de pire cas, justifié dans plan.md.
- **T077** : dossier autonome vérifié (`cmake --install` puis `bin\picazoo.exe` lancé sans
  Qt dans le PATH).
- **T080** : les étapes 4 à 6 du quickstart (copie depuis un téléphone, clé USB débranchée
  puis rebranchée, réglages après relance) restent à dérouler à la main ; elles sont
  couvertes par `tst_watcher`, `tst_reconcile` et `tst_browsing`.
- **Checkpoint US1 (risque R9)** : la fluidité du défilement à 100 000 éléments n'a pas
  été mesurée en images par seconde ; l'ouverture d'une telle bibliothèque l'a été (SC-002).
- **SC-006, tests** : l'instantané avant/après compare chaque fichier (chemin, taille,
  date, hachage) et l'existence des dossiers, mais pas la date des dossiers, que NTFS met
  à jour en différé après la création de leur contenu (faux positif observé).
- **Limite connue** : surveiller un dossier sur une clé USB garde un handle ouvert sur ce
  dossier ; Windows peut refuser « Éjecter » tant que Picazoo est ouvert.
- **Options de développement** : `PICAZOO_DATA_DIR` (bibliothèque séparée),
  `PICAZOO_SCREENSHOT=<png>;<ms>` (capture puis fermeture), `PICAZOO_DEV_OPEN=<n>`
  (ouvre la visionneuse), `PICAZOO_CRASH_TRACE=1` (pile d'appels en cas de plantage).

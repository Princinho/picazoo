# Tasks: Visionneuse autonome

**Input**: Design documents from `/specs/005-visionneuse-autonome/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/

**Tests**: demandés par le plan et par la constitution (lecture seule vérifiée
sur dossiers de test) : listage, ordre, parcours, zoom, registre, instance
unique.

**Organization**: par user story (spec.md), dans l'ordre des priorités.

## Format: `[ID] [P?] [Story] Description`

- **[P]** : peut être fait en parallèle (fichiers différents, sans dépendance)
- **[Story]** : US1 à US6 de la spec

---

## Phase 1: Setup

- [ ] T001 Créer la cible `picazoo-viewer` (Qt Quick, module QML `PicazooViewer`, `WIN32`, icône) dans src/viewer/CMakeLists.txt et l'ajouter au CMakeLists.txt racine ; ajouter `Network` à `find_package(Qt6 …)` ; règles d'installation et script de déploiement Qt comme pour `picazoo`
- [ ] T002 [P] Créer src/viewer/qml/Theme.qml (singleton, couleurs et polices du thème « Atelier sombre » de src/app/qml/Theme.qml, opacités du voile 0,78 / 1)
- [ ] T003 [P] Ajouter les fichiers de test tests/unit/tst_folderlisting.cpp, tests/unit/tst_browseorder.cpp, tests/unit/tst_zoommath.cpp, tests/integration/tst_folderbrowser.cpp, tests/integration/tst_associations.cpp, tests/integration/tst_singleinstance.cpp (squelettes Qt Test) ; lier `Qt6::Network` et les sources de src/viewer/SingleInstance.* pour tst_singleinstance dans tests/CMakeLists.txt

---

## Phase 2: Foundational

- [ ] T004 Implémenter `FolderListing` (FindFirstFileExW basique + LARGE_FETCH, dossier seul, fichiers HIDDEN/SYSTEM, dossiers et liens écartés, formats d'images seulement via `kindOf(format) == Kind::Image`, taille et date de modification) et `naturalLess` (StrCmpLogicalW) dans src/core/browse/FolderListing.h/.cpp
- [ ] T005 [P] Implémenter `ZoomMath` (`fitScale = min(1, vue/image)`, `fitScale ≤ scale ≤ 16`, zoom ancré `t = a − z·p`, bornage du déplacement, centrage d'une image plus petite) dans src/core/browse/ZoomMath.h/.cpp
- [ ] T006 [P] Implémenter `SystemPrefs::animationsEnabled()` (SPI_GETCLIENTAREAANIMATION) et `transparencyEnabled()` (`HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize\EnableTransparency`) dans src/core/platform/win/SystemPrefs.h/.cpp
- [ ] T007 Implémenter `FileImageProvider` (`image://file/<gen>/<screen|full|thumb>/<chemin encodé>`, pool de 2 threads, `Decoder::decode` avec orientation de `Decoder::readMetadata`, LRU de 8 aperçus écran, 1 pleine résolution plafonnée à 16 384 px, 64 vignettes de 160 px, `preload()`) dans src/viewer/FileImageProvider.h/.cpp
- [ ] T008 Tests de T004 et T005 dans tests/unit/tst_folderlisting.cpp et tests/unit/tst_zoommath.cpp (ordre `2` avant `10`, fichiers cachés, vidéo et texte écartés ; point sous le curseur fixe à 2 px près, bornes 1/16, bornage du déplacement)

---

## Phase 3: User Story 1 - Ouvrir une photo depuis l'Explorateur en un instant (P1) 🎯 MVP

**Goal**: la photo donnée par Windows s'affiche dans le voile en moins d'une seconde, sans bibliothèque, une seule instance.

**Independent Test**: « Ouvrir avec › picazoo-viewer.exe » sur un JPEG de 24 Mpx : voile et photo en < 1 s ; aucune bibliothèque créée ; dossier inchangé.

- [ ] T009 [P] [US1] Implémenter `SingleInstance` (mutex `Local\Picazoo.Viewer`, `QLocalServer` `Picazoo.Viewer.<session>`, envoi UTF-8 + `\n`, `AllowSetForegroundWindow(ASFW_ANY)`, réessai 2 s, regroupement des chemins reçus à moins de 500 ms) dans src/viewer/SingleInstance.h/.cpp
- [ ] T010 [P] [US1] Test d'élection et de transmission dans tests/integration/tst_singleinstance.cpp
- [ ] T011 [P] [US1] Implémenter `ViewerSettings` (`viewer.ini` dans `PICAZOO_DATA_DIR` ou `%LOCALAPPDATA%\Picazoo`, clés `overlay/opaque`, `strip/visible`, `assoc/answer` ∈ {none, later, never, accepted}, `assoc/askedAt`, `assoc/formats`, écriture seulement quand une valeur change) dans src/viewer/ViewerSettings.h/.cpp
- [ ] T012 [US1] Implémenter `ViewerSession` (photo courante, infos de la 001 : nom, chemin, date de prise de vue ou du fichier, dimensions, poids, appareil ; état affichable/illisible/vidéo/non pris en charge ; `openPath`, `revealInExplorer`, `openWith`) dans src/viewer/ViewerSession.h/.cpp
- [ ] T013 [US1] Écrire src/viewer/main.cpp : `QML_DISABLE_DISK_CACHE=1`, fenêtre au premier plan notée, instance unique avant l'interface, `--register`/`--unregister` sans interface, décodage anticipé de la photo, `setDefaultAlphaBuffer(true)`, fournisseur, chargement du QML, `PICAZOO_SCREENSHOT`, `PICAZOO_VIEWER_TIMING`
- [ ] T014 [US1] Écrire src/viewer/qml/Overlay.qml : fenêtre sans bordure plein écran sur l'écran de la souris, couleur transparente, voile noir 78 % / opaque avec fondu de 150 ms, nom et « 12 sur 87 », bouton ✕, Échap ferme, F11 bascule le voile, transparence de Windows désactivée → opaque, messages (illisible, non pris en charge, vidéo, RAW sans aperçu)

---

## Phase 4: User Story 2 - Parcourir le dossier avec des transitions fluides (P1)

**Goal**: flèches, Début/Fin, bords ; ordre de l'Explorateur ; boucle ; transitions de 250 ms sans retard.

**Independent Test**: fenêtre triée par nom, `photo2.jpg` → `photo3.jpg` ; 8 appuis rapides → `photo10.jpg` < 0,5 s ; tri par date respecté.

- [ ] T015 [P] [US2] Implémenter `mergeOrder(viewOrderNames, listing)` (images de la vue dans leur ordre, puis celles absentes de la vue dans l'ordre naturel ; noms comparés sans casse) dans src/core/browse/BrowseOrder.h/.cpp
- [ ] T016 [P] [US2] Implémenter `ExplorerOrder::query(folder, foregroundHwnd, timeoutMs)` (thread STA, `IShellWindows`, `SID_STopLevelBrowser`, `IFolderView2`, dossier par `IPersistFolder2`, préférence fenêtre au premier plan et vue visible, bureau via `FindWindowSW(SWC_DESKTOP)`, `Items(SVGIO_ALLVIEW | SVGIO_FLAG_VIEWORDER)`, abandon après 2 s) dans src/core/platform/win/ExplorerOrder.h/.cpp
- [ ] T017 [US2] Implémenter `FolderBrowser` (data-model : `open`, listage puis ordre de l'Explorateur sans bouger la photo courante, `next`/`previous` en boucle avec signal `wrapped`, `first`/`last`/`goTo`, `QFileSystemWatcher` regroupé sur 300 ms, `refresh` : restants dans leur ordre, nouveaux insérés ou ajoutés à la fin, photo disparue → `missingCurrent`) dans src/core/browse/FolderBrowser.h/.cpp
- [ ] T018 [P] [US2] Tests de T015 dans tests/unit/tst_browseorder.cpp et de T017 dans tests/integration/tst_folderbrowser.cpp (boucle, ajouts, suppression de la photo courante, instantané avant/après : aucun fichier créé ni modifié)
- [ ] T019 [US2] Brancher `FolderBrowser` dans `ViewerSession` (position, `next`, `previous`, `first`, `last`, `goTo`, préchargement ±2 et +3 dans le sens du parcours, `wrapped`, photo disparue → message puis suivante après 1,5 s) dans src/viewer/ViewerSession.cpp
- [ ] T020 [US2] Écrire src/viewer/qml/Stage.qml : quatre emplacements d'image recyclés, transition 250 ms OutCubic (fondu + glissement de 40 px), attente au plus 120 ms, interruption immédiate en cas de nouvelle demande, animations coupées selon `SystemPrefs`
- [ ] T021 [US2] Ajouter à Overlay.qml les raccourcis ←/→/Début/Fin, les zones de bord (15 %) avec boutons ◀ ▶ au survol, le bandeau « Retour à la première photo »

---

## Phase 5: User Story 3 - Zoomer à la molette là où l'on regarde (P1)

**Goal**: zoom ancré, doux, borné ; déplacement ; double-clic ; pleine résolution au besoin.

**Independent Test**: 5 crans sur un repère : il reste sous le curseur et devient net ; 5 crans arrière → ajusté.

- [ ] T022 [US3] Exposer `ZoomMath` au QML (`Q_INVOKABLE` dans `ViewerSession` ou type QML `ZoomModel`) dans src/viewer/ViewerSession.h/.cpp
- [ ] T023 [US3] Ajouter à Stage.qml : `WheelHandler` (×1,25 par cran de 120, proportionnel pour les pavés tactiles, animation 120 ms ancrée), `DragHandler` borné, double-clic ajuster ⇄ 100 %, `PinchHandler`, Ctrl +/−/0, image pleine résolution demandée au-delà de l'aperçu, retour à l'ajustement au changement de photo, image plus petite que l'écran à sa taille réelle
- [ ] T024 [US3] Écrire src/viewer/qml/Controls.qml : barre de commandes (◀ ▶ − % + Ajuster 100 % Informations Explorateur Ouvrir dans Picazoo Voile opaque) qui s'estompe après 2,5 s sans mouvement

---

## Phase 6: User Story 4 - Association avec Windows, jamais imposée (P2)

**Goal**: inscription « Ouvrir avec » à l'installation, proposition unique, réglage de Windows, état consultable.

**Independent Test**: installer : défauts inchangés, Picazoo dans « Ouvrir avec » ; « Ne plus demander » : rien ne change.

- [ ] T025 [P] [US4] Implémenter `FileAssociations` (`registerApp(scope)`, `unregisterApp(scope)` selon contracts/storage-and-registry.md, jamais `UserChoice` ni la valeur par défaut de `.<ext>`, `SHChangeNotify(SHCNE_ASSOCCHANGED)`, `isRegistered()`, `status()` via `IApplicationAssociationRegistration::QueryCurrentDefault`, `openDefaultAppsSettings()`, racine de test `PICAZOO_REGISTRY_ROOT`) dans src/core/platform/win/FileAssociations.h/.cpp
- [ ] T026 [P] [US4] Tests dans tests/integration/tst_associations.cpp (sous une clé de test : clés créées puis retirées exactement, valeur `OpenWithProgids` d'une autre application conservée, valeur par défaut de l'extension jamais touchée)
- [ ] T027 [US4] Écrire src/viewer/qml/AssocCard.qml et brancher la proposition dans `ViewerSession` (une fois ; pas si Picazoo ouvre déjà les JPEG ; « Plus tard » → au plus tôt 7 jours ; « Ne plus demander » ; « Choisir dans Windows… » inscrit si besoin puis ouvre `ms-settings:defaultapps?registeredAppUser=Picazoo`) ; menu « ⋯ » avec associations actuelles et inscription manquante
- [ ] T028 [US4] Appeler `--register` (`--machine` en installation pour tous) dans [Run] et `--unregister` dans [UninstallRun] de installer/picazoo.iss ; raccourci inchangé

---

## Phase 7: User Story 5 - Passer à l'application complète (P2)

**Goal**: « Ouvrir dans Picazoo », informations, Explorateur.

**Independent Test**: photo d'un dossier surveillé → bibliothèque sur la photo ; hors bibliothèque → proposition, refus sans effet.

- [ ] T029 [US5] Ajouter à picazoo.exe l'option `--show <chemin>` et l'instance unique (`Local\Picazoo.App`, `QLocalServer`) dans src/app/main.cpp, en réutilisant src/viewer/SingleInstance.* 
- [ ] T030 [US5] Implémenter `AppController::showPath(path)` (dossier trouvé via `Library::folders` puis élément par nom → signal `showItem(id)` ; dossier sous une racine surveillée non encore indexé → `showMessage` ; sinon → `askAddFolder(dossier)`) dans src/app/controllers/AppController.h/.cpp
- [ ] T031 [US5] Gérer `showItem`, `showMessage` et `askAddFolder` (dialogue « Ajouter ce dossier à la bibliothèque ? ») dans src/app/qml/Main.qml
- [ ] T032 [US5] Écrire src/viewer/qml/InfoCard.qml (informations de la 001, touche I) et brancher « Ouvrir dans Picazoo » (lance `picazoo.exe --show` puis ferme) et « Afficher dans l'Explorateur » (Ctrl+Maj+E) dans `ViewerSession`

---

## Phase 8: User Story 6 - Bande des photos voisines (P3)

**Goal**: vignettes voisines cliquables, masquage automatique et sur demande, sans disque.

**Independent Test**: 15e de 30 photos : vignettes de part et d'autre, clic sur la 18e, masquage après inactivité.

- [ ] T033 [US6] Exposer les voisines (±8) dans `ViewerSession` et écrire src/viewer/qml/Strip.qml (vignettes `image://file/…/thumb/…`, photo courante mise en évidence, clic → `goTo`, masquage avec les commandes, case « Afficher la bande de vignettes » retenue)

---

## Phase 9: Polish & Cross-Cutting Concerns

- [ ] T034 Ajouter `picazoo-viewer.exe` au paquet (cmake --install) et vérifier le déploiement Qt (plugins Quick, Network) dans src/viewer/CMakeLists.txt
- [ ] T035 Compiler, lancer `ctest --preset windows-release`, corriger
- [ ] T036 Valider à l'écran (PICAZOO_SCREENSHOT) : voile translucide, photo, commandes ; mesurer le temps d'ouverture (PICAZOO_VIEWER_TIMING)
- [ ] T037 [P] Mettre à jour README.md (visionneuse autonome, `--register`) et docs/feuille-de-route.md si besoin

---

## Dependencies & Execution Order

- Setup (T001–T003) → Foundational (T004–T008) → US1 (T009–T014, MVP).
- US2 dépend de US1 (fenêtre, session) ; US3 dépend de Stage.qml (T020).
- US4 ne dépend que de T001 et T013 ; US5 de T012 ; US6 de T019.
- Polish après les stories voulues.

## Parallel Example

```text
T004, T005, T006 en même temps (fichiers distincts du cœur)
T009, T010, T011 en même temps (US1)
T015, T016, T018 en même temps (US2)
T025, T026 en même temps (US4)
```

## Implementation Strategy

1. MVP : Setup + Foundational + US1 → une photo s'ouvre dans le voile.
2. US2 + US3 : le cœur de la demande (parcours, transitions, zoom).
3. US4 + US5 : intégration à Windows et à la bibliothèque.
4. US6 et finitions.

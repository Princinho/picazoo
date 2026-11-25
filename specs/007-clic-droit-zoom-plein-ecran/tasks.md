# Tasks: Ergonomie des visionneuses et clic droit

**Input**: documents de conception de `specs/007-clic-droit-zoom-plein-ecran/`

**Prerequisites**: [plan.md](plan.md), [spec.md](spec.md), [research.md](research.md), [data-model.md](data-model.md), [contracts/core-api.md](contracts/core-api.md), [contracts/ui-contract.md](contracts/ui-contract.md), [quickstart.md](quickstart.md)

**Tests**: demandés par la constitution (principe I : opérations sur les fichiers testées sur des dossiers de test) et nécessaires au zoom (les animations QML ne tournent pas quand Claude lance l'application) ; ils sont inclus et écrits avant le code qu'ils couvrent.

**Organization**: par histoire utilisateur (US1 à US5 de la spec), dans l'ordre de priorité. US2, US3 et US4 reposent sur le moteur de zoom de la phase 2 ; US1 et US5 n'en dépendent pas pour l'essentiel.

## Format: `[ID] [P?] [Story] Description`

- **[P]** : parallélisable (fichiers différents, pas de dépendance en cours)
- **[Story]** : histoire utilisateur concernée (US1…US5)

Rappels pour l'implémentation :
- Compiler depuis un « Developer PowerShell for VS 2026 » : `cmake --build --preset windows-release` ; tests : `ctest --preset windows-release` ; la sortie Qt Test se lit avec `-o fichier,txt`.
- Les sources du cœur, les tests et les fichiers QML sont pris par des `file(GLOB …)` : un nouveau fichier n'a pas à être déclaré, sauf les migrations SQL (ressource `picazoo_core_sql`).
- Erreurs QML : `PICAZOO_LOG_FILE` ; captures : `PICAZOO_SCREENSHOT`, `PICAZOO_DEV_SCREEN`, `PICAZOO_VIEWER_SCRIPT`.
- Pas de barre oblique inverse dans des scripts Python passés par l'outil Bash (elles sont perdues).

---

## Phase 1: Setup

- [x] T001 Créer les squelettes qui compilent : `zoom::ZoomMotion` (classe vide, constantes `kTauMs = 75.0`, `kGestureGapMs = 300.0`, `kCeilingThreshold = 0.9`, `kCeiling = 1.0`, `enum class Source { Wheel, Key, Button, Pinch }`) dans src/core/browse/ZoomMotion.h et src/core/browse/ZoomMotion.cpp ; `ZoomController : QObject` vide dans src/core/browse/ZoomController.h et src/core/browse/ZoomController.cpp ; test vide `tst_zoommotion` dans tests/unit/tst_zoommotion.cpp
- [x] T002 Enregistrer `ZoomController` comme type QML `Picazoo.Zoom 1.0` / `ZoomController` (`qmlRegisterType`) avant le chargement du QML dans src/app/main.cpp et src/viewer/main.cpp

---

## Phase 2: Foundational — moteur de zoom commun (bloque US2, US3, US4)

**But** : remplacer les deux copies QML du zoom (`Viewer.qml`, `Stage.qml`) par `ZoomMotion`, avec la poursuite exponentielle (research Z1, Z2, Z5) ; à la fin de la phase, les deux visionneuses zooment comme aujourd'hui, en plus doux, sans butée ni élan.

- [x] T003 [P] Tests de `ZoomMotion` (temps injecté, sans fenêtre) dans tests/unit/tst_zoommotion.cpp, garanties 2, 6 (moitié « un pas isolé atteint 98 % de son chemin entre 250 et 350 ms »), 7, 8 et 9 de contracts/core-api.md : la cible n'est jamais dépassée et elle est atteinte exactement à l'arrêt ; `stop()` et `reset()` arrêtent tout mouvement dans le même appel ; le point de l'image sous l'ancre reste sous l'ancre à ±0,5 px pendant un zoom avant ; `setImmediate(true)` place l'échelle sur la cible dans le même appel ; `setTarget(1)` puis `setTarget(fit)` ; changement de taille de vue en `fitMode`
- [x] T004 Implémenter `ZoomMotion` dans src/core/browse/ZoomMotion.h et src/core/browse/ZoomMotion.cpp : état (`imageSize`, `viewSize`, `margin`, `scale`, `offset`, `target`, ancrage `anchor` / échelle et offset de départ / offset de fin, `fitMode`, `moving`, `immediate`), `setImageSize` (conserve la taille affichée comme `onFullWChanged` aujourd'hui), `setView`, `reset`, `setTarget`, `panTo` (borné par `zoom::clampPan`), `stop`, `advance(dtMs)` avec poursuite du logarithme de l'échelle `log s += (log cible − log s)·(1 − e^(−dt/τ))`, τ = 75 ms, arrêt exact quand l'écart < 0,05 % ou < 0,5 px de taille affichée ; à chaque changement de cible, ancrage recalculé avec `zoom::anchorOnImage` et `zoom::zoomEndOffset`, offset intermédiaire avec `zoom::zoomStepOffset` ; cible bornée par `zoom::clampScale` ; `wheel()` et `step()` provisoires = un pas de `kWheelStep` (sans geste ni accélération) ; `pinchBegin/Update/End` directs (sans poursuite)
- [x] T005 Façade `ZoomController` dans src/core/browse/ZoomController.h et src/core/browse/ZoomController.cpp : `Q_PROPERTY` `scale`, `offsetX`, `offsetY`, `percent`, `fitScale`, `fitMode`, `moving`, `immediate`, `imageWidth`, `imageHeight`, `viewWidth`, `viewHeight`, `margin` (avec signaux), `Q_INVOKABLE` `wheel(angleDeltaY, x, y)`, `step(direction, source, autoRepeat, x, y)`, `pinchBegin()`, `pinchUpdate(factor, x, y)`, `pinchEnd()`, `setTarget(scale, x, y)`, `fit()`, `actualSize(x, y)`, `panTo(x, y)`, `stop()`, `reset()`, `advance(frameTimeSeconds)` ; horloge `QElapsedTimer` interne ; un seul signal groupé `changed()` émis après chaque modification pour limiter les liaisons recalculées
- [x] T006 Brancher la visionneuse intégrée sur `ZoomController` dans src/app/qml/Viewer.qml : supprimer `zoom`, `fitMode`, `ox`, `oy`, `anchoring`, `ax…ey`, `zoomTo`, `zoomBy`, `clampNow`, `onZoomChanged` et le `NumberAnimation zoomAnim` ; instancier `ZoomController { id: zc }` (taille d'image `fullW/fullH`, vue = scène, `margin: stageMargin`, `immediate: !viewer.animations`) et une `FrameAnimation { running: zc.moving; onTriggered: zc.advance(frameTime) }` ; image positionnée sur `zc.offsetX/offsetY`, taille `fullW*zc.scale` ; molette → `zc.wheel`, pincement → `pinchBegin/Update/End`, glisser → `zc.stop()` au début puis `zc.panTo`, double-clic → `setTarget(1 ou fit)`, touches 0 / 1 / Ctrl+ / Ctrl− et boutons du pied → `fit()`, `actualSize()`, `step()` ; `onItemIdChanged` → `zc.reset()` ; `needFull`, `canPan` et l'indicateur de zoom lisent `zc.scale` / `zc.percent`
- [x] T007 [P] Brancher la visionneuse autonome sur `ZoomController` dans src/viewer/qml/Stage.qml : un `ZoomController` par emplacement du `Repeater` (remplace `s`, `ox`, `oy`, `fitMode`, `anchoring`, `s0…ey`, `zoomTo`, `zoomAnim`), une `FrameAnimation` par emplacement active seulement pour `stage.current` en mouvement, `resetView()` → `reset()`, `zoomed`, `canPan`, `zoomPercent`, `zoomBy/fit/actualSize` réécrits sur le contrôleur ; `WheelHandler`, `PinchHandler`, `DragHandler`, `TapHandler` comme en T006 ; `immediate: !session.animations`
- [x] T008 [P] Retirer les `Q_INVOKABLE` de zoom devenus inutiles (`fitScale`, `clampScale`, `wheelFactor`, `zoomAround`, `clampOffset`, `anchorOnImage`, `zoomEndOffset`, `zoomStepOffset`, `clampPan`) de src/app/controllers/ViewerController.h/.cpp et src/viewer/ViewerSession.h/.cpp s'ils ne sont plus appelés par aucun QML (vérifier par recherche avant de supprimer)
- [x] T009 Compiler, lancer tous les tests (`tst_zoommath` et `tst_zoommotion` verts) et charger les deux visionneuses avec `PICAZOO_LOG_FILE` (application avec `PICAZOO_DEV_OPEN`, visionneuse autonome avec `PICAZOO_VIEWER_SCRIPT`) : aucune erreur QML ; capture `PICAZOO_SCREENSHOT` de chacune à l'ajustement

**Checkpoint** : le zoom des deux visionneuses passe par `ZoomMotion`.

---

## Phase 3: User Story 1 — Quitter le plein écran à la souris (Priority: P1) 🎯 MVP

**Goal** : en plein écran, un bouton « Quitter le plein écran » apparaît au mouvement de la souris et ramène à la visionneuse en fenêtre (FR-001 à FR-005).

**Independent Test** : quickstart étapes 1 à 5.

Peut se faire avant la phase 2 (ne touche pas au zoom) ; si la phase 2 est faite d'abord, la conservation du zoom (FR-002) se vérifie sur `ZoomController`.

- [x] T010 [P] [US1] Ajouter l'icône `iconExitFullScreen` (glyphe Segoe Fluent Icons « BackToWindow », U+E73F) à côté de `iconFullScreen` dans src/app/qml/Theme.qml
- [x] T011 [US1] Bouton flottant dans src/app/qml/Viewer.qml : `RoundButton` 48 px, ancré en haut à droite à 16 px, même fond `#D9121418` et bordure `Theme.lineStrong` que les boutons précédent / suivant, `Accessible.name` et info-bulle « Quitter le plein écran (F11) », `visible: root.fullScreen && opacity > 0` ; propriété `exitShown` mise à vrai par un `HoverHandler` sur toute la visionneuse (mouvement du pointeur), `Timer` de 2000 ms qui la remet à faux sauf si le bouton est survolé ou qu'un menu est ouvert ; `Behavior on opacity { NumberAnimation { duration: 250 } }` ; au-dessus de la vidéo (`z`) sans recouvrir ses commandes ; `onClicked: root.toggleFullScreen()`
- [x] T012 [US1] Vérifier dans src/app/qml/Viewer.qml que `toggleFullScreen()` rend `prevVisibility` (fenêtrée ou agrandie) et que l'élément courant, l'échelle et l'offset sont conservés à la sortie du plein écran (en `fitMode`, l'image se réajuste à la nouvelle taille ; sinon l'échelle reste et l'offset est seulement borné) ; corriger si besoin
- [x] T013 [US1] Capture de contrôle de la visionneuse intégrée en plein écran avec le bouton visible (forcer `exitShown` par une variable de développement lue au démarrage, par exemple `PICAZOO_DEV_SCREEN=viewer-fullscreen` dans src/app/main.cpp) et lecture du journal QML

**Checkpoint** : US1 utilisable et démontrable seule.

---

## Phase 4: User Story 2 — Butée à 100 % au zoom avant (Priority: P2)

**Goal** : un geste de zoom avant commencé sous 90 % s'arrête exactement à 100 % (FR-010 à FR-016).

**Independent Test** : quickstart étapes 6 à 8 ; `tst_zoommotion`.

- [x] T014 [P] [US2] Tests du geste dans tests/unit/tst_zoommotion.cpp, garanties 1, 3, 4 et 10 de contracts/core-api.md : depuis 0,3, 30 crans à 50 ms d'intervalle → échelle finale exactement 1,0 ; des crans au plafond ne changent rien ; pause de 300 ms puis un cran → > 1,0 ; depuis 0,8 → arrêt à 1,0 (seuil évalué au début du geste) ; depuis 0,9 → 1,125 au premier cran ; zoom arrière sans plafond ; `step(+1, Key, autoRepeat=true)` répété depuis 0,5 → 1,0, puis `autoRepeat=false` → au-delà ; un pincement commencé à 0,5 et poussé à ×4 reste à 1,0 jusqu'à `pinchEnd()` ; changement de sens = nouveau geste ; petite image dont l'ajustement vaut 0,95 → pas de plafond ; `setImageSize()` pendant un geste plafonné garde le plafond à « 100 % de la photo »
- [x] T015 [US2] Implémenter le geste dans src/core/browse/ZoomMotion.h et src/core/browse/ZoomMotion.cpp selon data-model.md (`Gesture { source, direction, startScale, ceiling, lastInputMs }`) : nouveau geste si la source change, si le sens s'inverse, si plus de 300 ms depuis le dernier cran (molette), à chaque appui « non répété » (clavier), à chaque clic (bouton), à chaque `pinchBegin` ; `startScale` = échelle **affichée** ; plafond `1.0` si `direction = +1` et `startScale < 0.9`, sinon aucun ; cible bornée par `min(plafond, kMaxScale)` puis `clampScale` ; un cran en sens inverse appelle `stop()` avant de s'appliquer ; `setTarget()` et `reset()` terminent le geste ; `ceiling()` exposé pour les tests
- [x] T016 [US2] Transmettre la source et la répétition automatique depuis le QML : `event.isAutoRepeat` pour Ctrl+ / Ctrl− dans src/app/qml/Viewer.qml et src/viewer/qml/Overlay.qml, `Source.Button` pour les boutons de zoom du pied de src/app/qml/Viewer.qml et de src/viewer/qml/Controls.qml ; l'indicateur affiche « 100 % » à la butée (`Math.round(percent)`)

**Checkpoint** : US2 démontrable dans les deux visionneuses.

---

## Phase 5: User Story 3 — Zoomer avec ↑ / ↓, changer de photo avec ← / → (Priority: P2)

**Goal** : ↑ / ↓ zooment d'un cran au centre, ← / → changent de photo, dans les deux visionneuses (FR-017 à FR-019).

**Independent Test** : quickstart étape 12.

- [x] T017 [US3] Dans `Keys.onPressed` de src/app/qml/Viewer.qml : `Qt.Key_Up` / `Qt.Key_Down` → `zc.step(+1 / −1, Key, event.isAutoRepeat, centre de la scène)`, ignorées si `root.isVideo` (événement accepté sans effet) ; `Key_Left` / `Key_Right` inchangées
- [x] T018 [P] [US3] Même chose dans `Keys.onPressed` de src/viewer/qml/Overlay.qml (via `stage.current`), ignorées si `session.currentKind` n'est pas une image ; vérifier que, menu « Plus » ou fenêtre surgissante ouverte, les flèches ne zooment pas (garde sur `chromeLoader.item.popupOpen`)
- [x] T019 [P] [US3] Mettre à jour les info-bulles et noms accessibles des boutons de zoom : « Zoom avant (↑, Ctrl +) », « Zoom arrière (↓, Ctrl −) » dans src/app/qml/Viewer.qml et src/viewer/qml/Controls.qml

**Checkpoint** : US3 démontrable dans les deux visionneuses.

---

## Phase 6: User Story 4 — Zoom plus doux, avec inertie, comme Picasa (Priority: P2)

**Goal** : élan après un geste de molette rapide, interruptible, respectant la butée (FR-040 à FR-047).

**Independent Test** : quickstart étapes 9 à 11 ; `tst_zoommotion`.

- [x] T020 [P] [US4] Tests de l'élan dans tests/unit/tst_zoommotion.cpp, garanties 5 et 6 de contracts/core-api.md : un cran isolé multiplie la cible par 1,25 exactement ; 10 crans à 20 crans/s mènent plus loin que 10 crans à 4 crans/s ; après le dernier cran, `moving()` faux en moins de 600 ms d'`advance()` cumulés ; clavier et boutons jamais accélérés ; avec accélération, un geste plafonné s'arrête quand même exactement à 1,0 ; `stop()` pendant l'élan fige l'échelle affichée
- [x] T021 [US4] Accélération selon la cadence dans src/core/browse/ZoomMotion.cpp (research Z3) : moyenne glissante des crans par seconde (demi-vie 100 ms, crans équivalents `angleDelta / 120` pour les pavés tactiles), gain `g = 1` jusqu'à 8 crans/s, linéaire jusqu'à `g = 2` à 20 crans/s, plafonné à 2 ; chaque cran de molette multiplie la cible par `1,25^(g · crans)` ; la cadence repart de zéro à chaque nouveau geste ; aucun gain pour `Key`, `Button`, `Pinch` et `setTarget`
- [x] T022 [US4] Interrompre l'élan dans les deux visionneuses (FR-042) : `stop()` au début d'un glisser, à l'appui d'un clic gauche sur l'image (`TapHandler.onPressedChanged`), au clic droit, et `reset()` au changement de photo, dans src/app/qml/Viewer.qml et src/viewer/qml/Stage.qml ; le glisser reste sans élan (FR-047)
- [x] T023 [US4] Fluidité pendant le chargement de la pleine résolution (FR-046) : calculer `needFull` sur la **cible** (`zc.target`) plutôt que sur l'échelle affichée, pour lancer le décodage dès le début du zoom et ne pas basculer d'image au milieu du mouvement, dans src/app/qml/Viewer.qml et src/viewer/qml/Stage.qml (exposer `target` dans src/core/browse/ZoomController.h si besoin)
- [ ] T024 [US4] Caler τ, la demi-vie de la cadence et les seuils du gain à l'œil, avec l'utilisateur, contre une vidéo de la Visionneuse de photos Picasa ; reporter les valeurs retenues dans src/core/browse/ZoomMotion.h et dans research.md (Z2, Z3) ; ajuster les bornes des tests T003 / T020 si les valeurs changent

**Checkpoint** : US2 + US3 + US4 livrées ensemble sur le moteur commun.

---

## Phase 7: User Story 5 — Menus contextuels partout où l'on clique (Priority: P3)

**Goal** : clic droit, touche Menu et Maj+F10 sur toutes les surfaces de FR-060, avec copier, copier le chemin, Propriétés, fond d'écran et mise à la corbeille annulable (FR-060 à FR-076).

**Independent Test** : quickstart étapes 13 à 21.

### Cœur : actions Windows (parallélisables)

- [x] T025 [P] [US5] `copyFilesToClipboard(paths, image)` et `copyTextToClipboard(text)` dans src/core/platform/win/ClipboardActions.h et src/core/platform/win/ClipboardActions.cpp : `QMimeData` avec `setUrls` (fichiers, converti en `CF_HDROP` par Qt) et le format `Preferred DropEffect` à `DROPEFFECT_COPY` (DWORD 1, 4 octets), plus `setImageData(image)` si une image est fournie ; texte : un chemin Windows natif (`QDir::toNativeSeparators`) par ligne, séparateur `\r\n`
- [x] T026 [P] [US5] Fond d'écran dans src/core/platform/win/Wallpaper.h et src/core/platform/win/Wallpaper.cpp (research W1) : `wallpaperSupported(path)` (extensions `.jpg .jpeg .jfif .png .bmp .gif .tif .tiff`, insensible à la casse), `snapshotWallpaper()` (`IDesktopWallpaper` : pour chaque écran `GetMonitorDevicePathAt` + `GetWallpaper`, `GetPosition`, `GetStatus` pour le diaporama ; `restorable` faux si diaporama actif ou si un chemin n'existe plus), `setWallpaper(path)` (`SetWallpaper(nullptr, path)`, disposition inchangée), `restoreWallpaper(snapshot)` (écran par écran) ; COM initialisé (STA) sur le fil appelant ; à appeler hors du fil de l'interface
- [x] T027 [P] [US5] `showProperties(paths, owner)` dans src/core/platform/win/ShellActions.h et src/core/platform/win/ShellActions.cpp : un fichier → `SHObjectProperties(owner, SHOP_FILEPATH, path, nullptr)` ; plusieurs → PIDL absolus (`SHParseDisplayName`), `SHCreateShellItemArrayFromIDLists`, `BindToHandler(nullptr, BHID_DataObject, IID_PPV_ARGS(&dataObject))`, `SHMultiFileProperties(dataObject, 0)`
- [x] T028 [P] [US5] `volumeHasRecycleBinCached(path)` dans src/core/platform/win/RecycleBin.h et src/core/platform/win/RecycleBin.cpp : cache `QHash<racine du volume, bool>` protégé par un mutex, alimenté par `volumeHasRecycleBin`

### Cœur : corbeille hors doublons

- [x] T029 [US5] Migration src/core/index/migrations/004_origine_operations.sql exactement : `ALTER TABLE file_operation ADD COLUMN source TEXT NOT NULL DEFAULT 'duplicates' CHECK (source IN ('duplicates', 'library', 'viewer'));` puis `UPDATE schema_version SET version = 4` (même forme que 003) ; la déclarer dans la ressource `picazoo_core_sql` de src/core/CMakeLists.txt ; `kLibrarySchemaVersion = 4` et `case 4` dans src/core/index/Migrator.h et src/core/index/Migrator.cpp
- [x] T030 [P] [US5] Test de migration 3 → 4 dans tests/unit/tst_database.cpp : une opération existante prend `source = 'duplicates'` ; une valeur hors de `'duplicates', 'library', 'viewer'` est refusée
- [x] T031 [P] [US5] Tests de `FileTrash` dans tests/integration/tst_filetrash.cpp (dossiers temporaires, corbeille réelle, nettoyage par tests/support/RecycleBinCleanup.h) : base neuve créée et migrée jusqu'à 4 ; journal écrit (`pending`) avant que le fichier ne parte ; `trashed` après ; fichier modifié (taille ou date) depuis la préparation → `skipped_changed` et laissé en place ; `item_id` NULL accepté ; `restore()` remet le fichier et écrit une opération `restore` de même `source` ; écriture pendant qu'une seconde connexion `Database` tient la base (WAL)
- [x] T032 [US5] `FileTrash` dans src/core/fileops/FileTrash.h et src/core/fileops/FileTrash.cpp selon contracts/core-api.md (`OperationSource`, `LooseTrashEntry`, `trash()`, `restore()`) : opération `kind = 'trash'` et entrées `pending` écrites dans une transaction **avant** le premier fichier, revérification existence + taille + date, `platform::moveToRecycleBin`, résultat par entrée (`trashed`, `skipped_changed`, `skipped_no_recycle_bin`, `skipped_locked`, `skipped_unavailable`, `failed`), état final (`done`, `partial`, `failed`), `finished_at` ; restauration par `platform::restoreFromRecycleBin` (`restored`, `restore_missing`, `restore_conflict`) avec `undoes_id`
- [x] T033 [P] [US5] Compléter tests/integration/tst_trash.cpp : `planItems` sur 3 éléments sans groupe → plan sans copie gardée ; exécution avec `OperationSource::Library` → `source = 'library'` au journal et éléments retirés de la bibliothèque ; `undo` → fichiers restaurés et réindexés ; `recoverInterrupted` laisse intacte une opération `running` créée il y a 10 s et reprend une opération créée il y a 2 min ; tests 002 / 006 existants inchangés et verts
- [x] T034 [US5] Compléter src/core/fileops/TrashService.h et src/core/fileops/TrashService.cpp : `planItems(QList<ItemId>)` (entrées `keeper = 0`, `group = 0`, fichiers bloqués si disque sans corbeille), `execute(plan, OperationSource source = Duplicates)` qui écrit `source`, `runTrash` délégué à `FileTrash` pour le passage à la corbeille (la comparaison d'empreinte avec la copie gardée reste faite par `TrashService` pour les entrées de doublons, absente pour `keeper = 0`), `recoverInterrupted` limité aux opérations créées depuis plus de 60 s, restauration qui reprend la `source` de l'opération annulée, `Operation.source` lu par `journal()` / `operation()`

### Application

- [x] T035 [US5] Actions de l'application dans src/app/controllers/AppController.h et src/app/controllers/AppController.cpp : `copyItems(ids)` (une photo → image « écran » décodée hors du fil de l'interface + fichier ; sinon fichiers seulement), `copyPaths(ids)`, `showProperties(ids)` (fenêtre principale comme propriétaire), `canSetWallpaper(id)`, `setWallpaper(id)` (cliché puis changement hors du fil de l'interface, signal `wallpaperChanged(restorable)`), `undoWallpaper()`, `requestRecycleInfo(ids)` → signal `recycleInfo(ids, available)` calculé par `volumeHasRecycleBinCached` hors du fil de l'interface
- [x] T036 [US5] Mise à la corbeille d'éléments dans src/app/controllers/TrashController.h et src/app/controllers/TrashController.cpp : `prepareItems(ids, source)` (`planItems`, sans le récapitulatif des doublons), `itemsPlan` (nom du fichier ou nombre + total en octets), `confirmItems()` → `execute(plan, Library | Viewer)`, signal `itemsTrashed(operationId, count)` pour le message « Annuler » ; titres du journal par `source` (« … depuis la bibliothèque », « … depuis la visionneuse », titres actuels pour les doublons)
- [x] T037 [P] [US5] Dialogue src/app/qml/TrashConfirm.qml (contracts/ui-contract.md, « Mise à la corbeille ») : titre « Mettre « nom » à la corbeille ? » ou « Mettre N fichiers à la corbeille ? » + total, texte « Le fichier part dans la corbeille de Windows. Vous pourrez l'annuler. », boutons « Annuler » (par défaut, focus, Entrée) et « Mettre à la corbeille »
- [x] T038 [US5] Menu d'élément src/app/qml/ItemContextMenu.qml selon contracts/ui-contract.md (un élément et N éléments, ligne d'en-tête non cliquable « N éléments sélectionnés », raccourcis affichés, entrées grisées avec info-bulle : vidéo, format non accepté comme fond d'écran, disque sans corbeille, pas de doublons via `dups.duplicateCount(id)`), « Voir les doublons de cette photo » → `dups.showGroup(dups.groupOf(id))`
- [x] T039 [US5] Brancher la grille dans src/app/qml/ThumbGrid.qml : remplacer `contextMenu` par `ItemContextMenu` ; `TapHandler { acceptedButtons: Qt.RightButton }` sur le fond (zone vide) → menu de vue (Agrandir / Réduire les vignettes, Tout sélectionner, Ouvrir le dossier dans l'Explorateur) ; `Keys.onPressed` : `Qt.Key_Menu` et Maj+F10 (menu près de la vignette courante), Suppr → confirmation, Ctrl+C, Ctrl+Maj+C, Alt+Entrée sur la sélection
- [x] T040 [P] [US5] Compléter le menu de dossier de src/app/qml/FolderList.qml : « Afficher dans l'Explorateur », « Copier le chemin », séparateur, « Voir les doublons de ce dossier » (existant) ; pas de menu sur une ligne d'année
- [x] T041 [US5] (fait dans src/app/qml/Main.qml, pour qu'ils restent visibles dans la visionneuse intégrée) Messages « Annuler » de la bibliothèque dans src/app/qml/Library.qml (même forme que le message de src/app/qml/Duplicates.qml, 8 s, Ctrl+Z tant qu'il est visible) : « N fichiers mis à la corbeille · Annuler » → `trash.undo(operationId)` ; « Fond d'écran changé · Annuler » → `app.undoWallpaper()`, ou phrase sans « Annuler » si non rétablissable
- [x] T042 [P] [US5] Afficher l'origine de l'opération dans src/app/qml/OperationLog.qml (titre fourni par `TrashController`, T036)
- [x] T043 [US5] Suite après mise à la corbeille dans src/app/controllers/ViewerController.h et src/app/controllers/ViewerController.cpp : quand l'élément courant est retiré de la bibliothèque, passer au suivant, sinon au précédent, sinon signaler `emptied()` pour fermer la visionneuse
- [x] T044 [US5] Menu de l'image de la visionneuse intégrée src/app/qml/ImageContextMenu.qml (contracts/ui-contract.md) et branchement dans src/app/qml/Viewer.qml : `TapHandler` droit sur la scène (appelle `zc.stop()`, ne zoome ni ne déplace), `Qt.Key_Menu` / Maj+F10 (menu au centre), Suppr, Ctrl+C, Ctrl+Maj+C, Alt+Entrée ; « Plein écran » / « Quitter le plein écran » selon l'état (FR-004) ; vidéo : zoom et fond d'écran grisés, « Copier » au lieu de « Copier l'image » ; Échap ferme d'abord un menu ou un dialogue ouvert, jamais la visionneuse en même temps ; `TrashConfirm` et message « Annuler » ; `onEmptied` → `root.close()`

### Visionneuse autonome

- [x] T045 [US5] Retrait immédiat d'un chemin de la liste dans src/core/browse/FolderBrowser.h et src/core/browse/FolderBrowser.cpp (`removePath(path)` : retire l'entrée, l'élément courant devient le suivant, sinon le précédent ; sans attendre la surveillance du dossier) et test dans tests/integration/tst_folderbrowser.cpp
- [x] T046 [US5] Actions de la visionneuse autonome dans src/viewer/ViewerSession.h et src/viewer/ViewerSession.cpp : `trashCurrent()` (ouvre `library.db` du dossier de données à la demande avec `Database` + `Migrator`, `FileTrash::trash` avec `OperationSource::Viewer` sur un fil de travail, `item_id` NULL, puis `FolderBrowser::removePath` et affichage du suivant ; signal `trashed(operationId)` ou `trashFailed(message)` ; fermeture s'il ne reste rien), `undoTrash(operationId)` (`FileTrash::restore`, puis réouverture du dossier sur le fichier restauré), `copyImage()`, `copyPath()`, `showProperties()`, `canSetWallpaper`, `setWallpaper()` / `undoWallpaper()`, `requestRecycleInfo()` → `recycleAvailable`
- [x] T047 [US5] Menu de l'image src/viewer/qml/ImageContextMenu.qml et dialogue src/viewer/qml/TrashConfirm.qml (contracts/ui-contract.md : raccourcis propres à cette visionneuse, « Ouvrir dans Picazoo », « Fermer (Échap) », pas de « Plein écran ») ; branchement : `TapHandler` droit dans src/viewer/qml/Stage.qml, touches Menu / Maj+F10 / Suppr / Ctrl+C / Ctrl+Maj+C / Alt+Entrée dans src/viewer/qml/Overlay.qml ; menu et dialogue inclus dans `closePopups()` / `popupOpen` de src/viewer/qml/Chrome.qml ; message « Annuler » dans src/viewer/qml/Chrome.qml (même forme que le toast existant) ; les commandes restent affichées tant qu'un menu est ouvert

**Checkpoint** : US5 complète ; toutes les histoires fonctionnent.

---

## Phase 8: Polish & Cross-Cutting Concerns

- [x] T048 [P] Ajouter la ligne 007 au tableau et mettre à jour l'état dans docs/feuille-de-route.md
- [x] T049 [P] Mentionner dans README.md les nouveaux gestes (clic droit, ↑ / ↓, Suppr, bouton de sortie du plein écran, zoom avec élan et butée à 100 %)
- [x] T050 Compiler, `ctest --preset windows-release` complet (002, 005, 006 et 007 verts), et chargement des deux exécutables sans erreur QML (`PICAZOO_LOG_FILE`), captures des menus ouverts (`PICAZOO_SCREENSHOT`)
- [ ] T051 Validation manuelle du quickstart (étapes 1 à 21) avec l'utilisateur, sur un dossier de test, dont la mesure SC-004 (1 000 éléments sélectionnés) machine au repos, hors compilation ; consigner les écarts dans specs/007-clic-droit-zoom-plein-ecran/quickstart.md

---

## Dependencies & Execution Order

### Phases

- **Setup (1)** → **Foundational (2)** → US2 (4) → US3 (5) et US4 (6).
- **US1 (3)** ne dépend que de la phase 1 en pratique (bouton et plein écran) ; T012 se vérifie mieux après la phase 2.
- **US5 (7)** : les sous-parties « Cœur : actions Windows » et « Cœur : corbeille » ne dépendent de rien d'autre ; T044 et T047 utilisent `ZoomController` (phase 2) pour `stop()` et les entrées de zoom du menu.
- **Polish (8)** après les histoires livrées.

### À l'intérieur des histoires

- Les tests (T003, T014, T020, T030, T031, T033) avant le code qu'ils couvrent.
- US5 : T029 → T032 → T034 → T036 → (T037, T038, T044) ; T025–T028 → T035 / T046 ; T045 → T046 → T047.

### Parallel Opportunities

- Phase 2 : T003 en même temps que T005 ; T007 et T008 en parallèle de T006 une fois T004 / T005 faits.
- US1 entière en parallèle de la phase 2.
- US5 : T025, T026, T027, T028, T030, T031 et T033 en même temps ; T037, T040, T042 en parallèle de T038 / T039.

### Parallel Example: User Story 5

```text
Ensemble : T025 ClipboardActions · T026 Wallpaper · T027 showProperties · T028 cache corbeille
Ensemble : T030 test migration 3 → 4 · T031 tests FileTrash · T033 tests TrashService
Puis      : T029 migration → T032 FileTrash → T034 TrashService
```

## Implementation Strategy

### MVP

Phase 1 puis **US1** (T010–T013) : la sortie du plein écran à la souris est
utilisable seule, sans toucher au zoom.

### Livraisons successives

1. US1 — sortie du plein écran.
2. Phase 2 + US2 + US3 + US4 — le nouveau moteur de zoom, sa butée, les
   flèches et l'élan, calés avec l'utilisateur (T024).
3. US5 — menus et actions sans fichiers (T025–T028, T035, T038–T040, T044
   sans la corbeille), puis la corbeille (T029–T034, T036, T037, T041–T047).
4. Polish.

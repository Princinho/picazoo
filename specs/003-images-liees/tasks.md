# Tasks: Images liées

**Input**: documents de conception de `specs/003-images-liees/`

**Prerequisites**: [plan.md](plan.md), [spec.md](spec.md), [research.md](research.md), [data-model.md](data-model.md), [contracts/](contracts/), [quickstart.md](quickstart.md)

**Tests**: demandés par la constitution (principe III : fluide sur une vraie bibliothèque, vérifié par des tests) et par le plan (N1–N3, détection pure testable sans bibliothèque réelle) ; ils sont inclus.

**Organization**: par histoire utilisateur (US1 à US4 de la spec), chaque phase étant testable seule.

## Format: `[ID] [P?] [Story] Description`

- **[P]** : parallélisable (fichiers différents, pas de dépendance en cours)
- **[Story]** : histoire utilisateur concernée (US1…US4)

Rappels pour l'implémentation :
- Compiler depuis un « Developer PowerShell for VS 2026 » : `cmake --build --preset windows-release` ; tests : `ctest --preset windows-release` ; la sortie Qt Test se lit avec `-o fichier,txt`.
- Les sources du cœur et les tests sont pris par des `file(GLOB …)` : un nouveau fichier n'a pas à être déclaré, **sauf** la migration SQL (`picazoo_core_sql`, liste explicite dans src/core/CMakeLists.txt, comme pour 002/003/004) et `kLibrarySchemaVersion`.
- Aucune E/S disque de l'utilisateur n'est faite par cette fonctionnalité : `core/naming/` ne lit que des `QString` déjà en mémoire (noms de fichiers indexés par la 001), jamais le contenu d'un fichier.

---

## Phase 1: Setup

- [x] T001 Déclarer `005_images_liees.sql` dans la ressource `picazoo_core_sql` de src/core/CMakeLists.txt et passer `kLibrarySchemaVersion` à 5 (avec le commentaire de version existant) dans src/core/index/Migrator.h, et ajouter `case 5: return QStringLiteral(":/sql/005_images_liees.sql");` dans src/core/index/Migrator.cpp
- [x] T002 [P] Écrire la migration src/core/index/migrations/005_images_liees.sql (voir data-model.md) : table `naming_group` (`root_key` TEXT UNIQUE COLLATE NOCASE, `display_name` TEXT, `confidence` ∈ `regular_series|same_root|generic_root`, `member_count INTEGER CHECK (member_count >= 2)`, `state` ∈ `to_review|confirmed|rejected` DEFAULT `to_review`, `renamed INTEGER CHECK (renamed IN (0,1)) DEFAULT 0`, `updated_at`), index `(confidence)` et `(state)` ; table `naming_member` (`group_id` FK → `naming_group.id` ON DELETE CASCADE, `item_id` FK → `item.id` ON DELETE CASCADE UNIQUE, `detected_root` TEXT, `manual INTEGER CHECK (manual IN (0,1)) DEFAULT 0`, PK `(group_id, item_id)`) ; table `naming_decision` (`root_key` TEXT PRIMARY KEY COLLATE NOCASE, `kind` ∈ `rejected|merged_into`, `target_root_key` TEXT NULL, `decided_at`) ; `UPDATE schema_version SET version = 5` (aucune ligne insérée dans `settings`, comme les migrations précédentes : le défaut de `naming_include_generic_roots` vit dans `Settings::defaultValue`)

---

## Phase 2: Foundational (bloquant pour toutes les histoires)

**But** : le détecteur pur de racine existe et se teste sans bibliothèque réelle ; les tables existent ; le réglage `naming_include_generic_roots` est lu par les mêmes mécanismes que les réglages existants.

- [x] T003 [P] Types partagés (`RootShape`, `DetectedRoot`, `Confidence`, `NamingCandidate`, `NamingInput`) dans src/core/naming/NamingTypes.h et src/core/naming/NamingTypes.cpp
- [x] T004 [P] Ajouter `naming_include_generic_roots` (défaut `0`, booléen comme `duplicates_enabled`) dans la table `defaults()` et la liste de clés booléennes de `Settings::sanitize` dans src/core/library/Settings.cpp
- [x] T005 [US-fondation] Tests unitaires de `detectRoot()` (contracts/core-api.md) dans tests/unit/tst_naming_detector.cpp, table de noms (`QTest::addColumn`) couvrant les 4 formes de N1 (séparateur + chiffres, parenthèses, suffixe « copie », chiffres collés ≥ 3), la casse ignorée pour la racine normalisée, les racines génériques de N2 (`IMG_`, `DSC_`, `PXL_`, `VID_`, `MVI_`, `GOPR`, `Capture`, `Screenshot`, `Image`, `Photo`, `Video`, suivies d'un numéro ou horodatage), et les racines trop courtes (< 3 caractères alphabétiques significatifs)
- [x] T006 [P] [US-fondation] Implémenter `detectRoot()` dans src/core/naming/NamingDetector.h et src/core/naming/NamingDetector.cpp selon les 4 formes et les règles de généricité de N1/N2 ; faire passer T005
- [x] T007 [US-fondation] Tests unitaires de `groupByRoot()` dans tests/unit/tst_naming_detector.cpp : racines regroupées correctement (`vacances_bali_001`..`_005` → 1 groupe « regular_series » de 5 éléments), racines génériques exclues par défaut et incluses si `includeGeneric = true` (avec confiance `generic_root`), aucun groupe d'un seul élément (FR-004), numérotation irrégulière → confiance `same_root` (N4), photo + vidéo de même racine regroupées ensemble
- [x] T008 [US-fondation] Implémenter `groupByRoot()` dans src/core/naming/NamingDetector.cpp (fonction pure, aucune E/S) selon N1/N2/N4 ; faire passer T007
- [x] T009 Compiler et lancer `ctest --preset windows-release -R tst_naming_detector` : tous les cas de T005/T007 verts, aucune régression des tests existants (migration non destructive, base inchangée pour les tests qui n'ouvrent pas une bibliothèque en version 5)

**Checkpoint** : le détecteur de racine existe, se teste et produit des groupes candidats corrects, même sans bibliothèque ni moteur.

---

## Phase 3: User Story 1 — Voir les groupes proposés par nom, avec leur confiance (Priority: P1) 🎯 MVP

**Goal** : une entrée « Images liées » apparaît dans la liste des dossiers dès qu'un groupe existe ; l'écran de revue montre chaque groupe avec son nom suggéré, ses vignettes et sa confiance.

**Independent Test** : quickstart.md étapes 1–2 ; 5 photos à racine commune + 5 `IMG_*` + 10 sans rapport → 1 groupe de 5, les `IMG_*` absents.

### Tests

- [x] T010 [P] [US1] Test d'intégration dans tests/integration/tst_naming_engine.cpp (nouveau fichier, sur le modèle de tests/support/LibraryFixture.h) : après `addRoot` et `waitForIdle` sur un dossier de 5 photos à racine commune (`vacances_bali_001.jpg`..`_005.jpg`), 5 `IMG_0001.jpg`..`IMG_0005.jpg` et 10 photos sans rapport, `NamingStore::summary().groupCount == 1` et le groupe trouvé contient exactement les 5 éléments à racine commune

### Implementation

- [x] T011 [US1] Dépôt des tables 005 : lecture des groupes (`groups()`, `group()`, `groupOf()`, `summary()`) dans src/core/naming/NamingStore.h et src/core/naming/NamingStore.cpp
- [x] T012 [US1] Moteur : repérage complet sur `Library::scanFinished` (premier repérage seulement, voir N3), appelé via `Library::post` (pas de fil dédié), lit tous les noms de fichiers déjà indexés, appelle `groupByRoot()`, écrit les groupes en base (en respectant les décisions déjà présentes dans `naming_decision`, vides au premier repérage), notifie `groupsChanged`, dans src/core/naming/NamingEngine.h et src/core/naming/NamingEngine.cpp ; faire passer T010
- [x] T013 [US1] Contrôleur QML (résumé, liste des groupes, membres avec vignette/nom) dans src/app/controllers/NamingController.h et src/app/controllers/NamingController.cpp, branché dans src/app/main.cpp (création après `library`, comme `DuplicatesController`)
- [x] T014 [US1] Entrée « Images liées » sous l'entrée « Doublons » (ou seule si la 002 n'a pas de groupe) dans src/app/qml/FolderList.qml et bascule grille ↔ écran dans src/app/qml/Library.qml
- [x] T015 [US1] Écran src/app/qml/RelatedImages.qml et ligne de groupe src/app/qml/RelatedGroupRow.qml (nom suggéré, badge de confiance, vignettes des membres, message « Aucune image liée trouvée pour l'instant. »)

**Checkpoint** : US1 démontrable seule — les groupes se voient, avec leur confiance, sans action possible dessus.

---

## Phase 4: User Story 2 — Valider ou corriger un groupe avant toute action (Priority: P1)

**Goal** : ajouter/retirer un élément, fusionner, scinder, renommer, exclure un groupe — jamais de changement sur le disque.

**Independent Test** : quickstart.md étapes 3–7 ; retirer un élément, renommer, puis rouvrir l'écran : le groupe reflète les changements, aucun fichier modifié sur le disque.

### Tests

- [x] T016 [P] [US2] Tests d'intégration dans tests/integration/tst_naming_engine.cpp : `addToGroup`/`removeFromGroup` persistent et survivent à un nouveau repérage complet (`NamingEngine::rescanAll()`) ; `mergeGroups` réunit deux groupes sous une racine (écrit `naming_decision(merged_into)`), un nouveau repérage ne les sépare pas à nouveau ; `splitGroup` crée un second groupe à partir d'une partie des membres ; `markNotAGroup` empêche la reconstruction après `rescanAll()` et après l'ajout d'un nouvel élément de même racine ; `undoDecision` annule un rejet et laisse le groupe réapparaître au prochain repérage ; aucun de ces appels ne touche au système de fichiers (vérifié via `FsSnapshot`, cf. tests/support/FsSnapshot.h)

### Implementation

- [x] T017 [US2] Décisions dans src/core/naming/NamingStore.cpp : `addToGroup` (`manual = 1`), `removeFromGroup`, `mergeGroups` (écrit `naming_decision(kind = merged_into, target_root_key)`, fusionne les membres sous le groupe cible), `splitGroup` (nouveau `naming_group` avec `root_key` synthétique dérivée, déplace les membres sélectionnés), `rename` (`display_name`, `renamed = 1`), `markNotAGroup` (écrit `naming_decision(kind = rejected)`, supprime le groupe), `undoDecision` (supprime la ligne `naming_decision`) ; faire passer T016
- [x] T018 [US2] Prise en compte des décisions par le moteur lors d'un repérage complet ou incrémental (une racine `rejected` ne reforme pas de groupe ; une racine `merged_into` rejoint directement le groupe cible) dans src/core/naming/NamingEngine.cpp
- [x] T019 [US2] Méthodes invokable du contrôleur (ajouter, retirer, fusionner, scinder, renommer, exclure, annuler une décision, sélecteur d'éléments à ajouter parmi ceux non présents dans le groupe) dans src/app/controllers/NamingController.h et src/app/controllers/NamingController.cpp
- [x] T020 [US2] Menus et édition en place dans src/app/qml/RelatedGroupRow.qml (menu du groupe : Renommer, Fusionner avec…, Scinder…, Ce n'est pas un groupe ; menu d'un élément : Retirer de ce groupe, Ouvrir, Afficher dans l'Explorateur) et bouton « Ajouter des éléments… » dans src/app/qml/RelatedImages.qml

**Checkpoint** : US1 + US2 démontrables ensemble — les groupes se corrigent sans jamais toucher un fichier.

---

## Phase 5: User Story 3 — Un repérage discret qui suit la bibliothèque (Priority: P2)

**Goal** : le repérage se met à jour tout seul après un ajout, sans ralentir l'indexation ni le défilement.

**Independent Test** : quickstart.md étape 8 ; ajouter une photo prolongeant une racine existante dans le dossier surveillé → elle rejoint son groupe en moins d'une minute, sans réanalyser le reste.

### Tests

- [x] T021 [P] [US3] Test d'intégration dans tests/integration/tst_naming_engine.cpp : sur une bibliothèque déjà repérée (US1), ajouter un fichier de racine existante dans le dossier surveillé, attendre `fx.lib->waitForIdle()` puis l'apparition du membre dans le groupe (polling borné, cf. le motif `waitFor` de tests/integration/tst_watcher.cpp) ; vérifier qu'un compteur de repérages complets n'augmente pas (un seul repérage complet, au premier `scanFinished`, voir N3) alors que la mise à jour incrémentale a eu lieu

### Implementation

- [x] T022 [US3] Mode incrémental dans src/core/naming/NamingEngine.cpp : sur `Library::itemsChanged`/éléments nouvellement créés (signal déjà émis par l'indexation), ne recalcule la racine que des éléments concernés via `detectRoot()` seul (pas `groupByRoot()` sur toute la bibliothèque), les rattache à un groupe existant de même `root_key` (ou à une racine `rejected`, auquel cas ils restent hors groupe) ; faire passer T021
- [x] T023 [US3] Vérifier (mesure, pas de nouveau code a priori) qu'aucun appel du mode incrémental ne bloque le thread de l'indexation au-delà de quelques millisecondes sur une bibliothèque de 100 000 éléments ; si une régression apparaît, limiter la fréquence de recalcul (regroupement des éléments touchés sur la même fenêtre que `Library::flushNotifications`, 250 ms) dans src/core/naming/NamingEngine.cpp

**Checkpoint** : US1 + US2 + US3 démontrables ensemble — le repérage suit la bibliothèque sans ralentir quoi que ce soit.

---

## Phase 6: User Story 4 — Des groupes prêts pour la réorganisation à venir (Priority: P3)

**Goal** : les décisions survivent aux redémarrages et aux renommages/déplacements de fichiers ; chaque groupe confirmé a un nom clair.

**Independent Test** : quickstart.md étapes 10 ; renommer un groupe, fermer et relancer Picazoo, renommer un fichier du groupe dans l'Explorateur → le groupe garde son nom et son nombre d'éléments.

### Tests

- [x] T024 [P] [US4] Test d'intégration dans tests/integration/tst_naming_engine.cpp : renommer un groupe (`rename`), fermer la `Library` (`fx.reopen()`, cf. tests/support/LibraryFixture.h), rouvrir, vérifier que `display_name` et `renamed` sont inchangés ; renommer un fichier membre sur le disque entre-temps (simulant une détection de renommage de la 001, `item.id` stable) et vérifier que le membre reste dans son groupe après réouverture

### Implementation

- [x] T025 [US4] Vérifier que `naming_member.item_id` (FK vers `item.id`, stable aux renommages/déplacements par la 001) suffit à garder un membre dans son groupe sans action supplémentaire ; si un cas y échappe (par exemple un remplacement complet de ligne `item` plutôt qu'une mise à jour), corriger dans src/core/naming/NamingStore.cpp ou src/core/naming/NamingEngine.cpp selon le cas observé ; faire passer T024
- [x] T026 [US4] Afficher le nom prêt pour un sous-dossier (`display_name` du groupe confirmé, FR-007) dans le panneau d'informations de l'élément (mention « Fait partie du groupe "<nom>" (n éléments) — Voir le groupe », distincte de la mention équivalente des doublons 002) dans src/app/qml/InfoPanel.qml et src/app/controllers/NamingController.h/.cpp (`groupOf`, `duplicateCount`-like helper)

**Checkpoint** : les quatre histoires utilisateur sont livrées ; les groupes confirmés sont stables et prêts pour la 004.

---

## Phase 7: Polish & Cross-Cutting

- [x] T027 [P] Option de mesure du temps de premier repérage (SC-002) dans bench/picazoo-bench.cpp, fonction `benchScan` : relever le temps entre l'ajout de la racine et `NamingController`/`NamingStore::summary()` stable après le premier `scanFinished`, afficher via `report("Premier repérage des images liées", seconds, "s", 60)`
- [x] T028 [P] Case à cocher « Inclure les noms donnés par l'appareil (IMG_, DSC_…) dans les images liées » dans le menu Outils (src/app/qml/AppHeader.qml ou équivalent), liée à `naming_include_generic_roots` via `AppController` ou `NamingController`
- [x] T029 Mettre à jour README.md (fonctionnalité 003) et docs/feuille-de-route.md (ligne 003, état au jour de la livraison)
- [x] T030 Compilation Release sans avertissement et `ctest --preset windows-release` intégralement vert
- [ ] T031 Validation manuelle : quickstart.md étapes 1 à 11

---

## Dependencies & Execution Order

- **Setup (1)** : T001 avant T002 (la ressource doit exister avant que `Migrator` ne la référence au runtime, mais les deux fichiers sont indépendants à l'écriture ; T002 peut être écrit en parallèle, le lien se vérifie à la compilation).
- **Foundational (2)** : bloque toutes les user stories. T003 indépendante. T004 indépendante. T005 avant T006 (test avant code). T007 avant T008 (test avant code). T006 doit exister avant que T007/T008 ne s'exécutent utilement (mais peuvent s'écrire en parallèle). T009 clôt la phase.
- **US1 (Phase 3)** : dépend de la Phase 2 complète. T010 avant T012 (test avant code). T011 indépendante (fichier différent), nécessaire à T012. T013 dépend de T011/T012. T014/T015 après T013.
- **US2 (Phase 4)** : dépend de US1 (les groupes doivent exister pour les corriger). T016 avant T017 (test avant code). T018 dépend de T017. T019 dépend de T017. T020 dépend de T019.
- **US3 (Phase 5)** : dépend de US1 (le moteur de repérage complet doit exister). Indépendante de US2 dans son code (fichier `NamingEngine.cpp` partagé avec T018 : coordonner). T021 avant T022 (test avant code). T023 après T022.
- **US4 (Phase 6)** : dépend de US1 et US2 (groupes et renommage). T024 avant T025 (test avant code). T026 dépend de T025.
- **Polish (Phase 7)** : après toutes les user stories.

### Parallel Opportunities

- Phase 2 : T003, T004 en parallèle ; T005/T006 et T007/T008 peuvent être menés par deux agents différents (fichiers différents à l'intérieur de `NamingDetector`, mais coordonner si les deux touchent le même fichier `.cpp`).
- US1 : T010 et T011 en parallèle (fichiers différents), puis T012 dépend des deux.
- US3 : T021 indépendante du reste de US2 (fichiers différents : `tst_naming_engine.cpp` partagé, coordonner les ajouts).
- US4 : T024 indépendante du reste.

## Implementation Strategy

**MVP = US1 seule** (Phases 1–3) : les groupes se voient avec leur confiance, sans pouvoir encore les corriger. US2 (Phase 4) ajoute la correction, point sensible du principe IV (le logiciel propose, l'utilisateur décide) : sans elle, la fonctionnalité resterait une simple suggestion non actionnable. US3 (Phase 5) ajoute le suivi automatique, indispensable sur une vraie bibliothèque qui continue de recevoir des photos (principe III), mais un repérage manuel ponctuel suffit à valider US1/US2. US4 (Phase 6) confirme la stabilité dans le temps, en vue de la 004. Livrer US1 + US2 ensemble est raisonnable ici (contrairement à la 002) car aucune des deux n'a de coût ni de risque propre (aucune E/S) qui justifierait de les séparer en production.

---

## Notes de livraison (2026-11-26)

- `ctest --preset windows-release` : 29 sur 29 verts, compilation Release sans
  avertissement.
- Mesures (`picazoo-bench --items 100000 --index-only --naming`) : premier
  repérage 3,3 s (SC-002, < 60 s) ; relecture de la liste 113 ms ; ajout d'une
  photo : 5 à 7 ms sur le thread d'indexation (T023). « Découverte complète »
  (001) : 121 s pour un seuil de 60 s, mesurée avant la création du moteur de la
  003 ; non comparée à `main`.
- Écarts par rapport à la conception, reportés dans data-model.md et les
  contrats : table `naming_exclusion` (les retraits survivent à un repérage),
  colonne `naming_decision.label`, éléments déplacés par une scission ou une
  fusion marqués `manual`, bouton « Confirmer » et liste « Décisions ».
- Limite connue : annuler l'exclusion d'un groupe issu d'une scission rend ses
  éléments à leur racine d'origine, sans recréer le groupe scindé.
- **T031 reste à faire à la main** : les écrans et les corrections ont été
  exercés dans l'application par des appels depuis le QML (captures), pas à la
  souris ni au clavier ; SC-003 (fluidité) et SC-005 (5 personnes) ne sont pas
  mesurés.


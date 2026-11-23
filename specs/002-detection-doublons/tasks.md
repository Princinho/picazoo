# Tasks: Détection des doublons

**Input**: documents de conception de `specs/002-detection-doublons/`

**Prerequisites**: [plan.md](plan.md), [spec.md](spec.md), [research.md](research.md), [data-model.md](data-model.md), [contracts/](contracts/), [quickstart.md](quickstart.md)

**Tests**: demandés par la constitution (principe I : opérations sur les fichiers testées sur des dossiers de test) et par le plan (D12) ; ils sont inclus.

**Organization**: par histoire utilisateur (US1 à US5 de la spec), chaque phase étant testable seule.

## Format: `[ID] [P?] [Story] Description`

- **[P]** : parallélisable (fichiers différents, pas de dépendance en cours)
- **[Story]** : histoire utilisateur concernée (US1…US5)

---

## Phase 1: Setup

- [x] T001 Généraliser le chargement des migrations à une liste de fichiers numérotés (`001_initial.sql`, `002_doublons.sql`) et passer `kLibrarySchemaVersion` à 2 dans src/core/index/Migrator.cpp et src/core/index/Migrator.h
- [x] T002 Déclarer `002_doublons.sql` dans la ressource `picazoo_core_sql` de src/core/CMakeLists.txt
- [x] T003 Ajouter `tests/accuracy/tst_*.cpp` au glob des tests dans tests/CMakeLists.txt

---

## Phase 2: Foundational (bloquant pour toutes les histoires)

- [x] T004 Écrire la migration src/core/index/migrations/002_doublons.sql : tables `item_fingerprint` (`content_state` ∈ `not_candidate|pending|done|skipped_online|failed`, `visual_state` ∈ `pending|done|low_detail|not_applicable|failed`, FK `item_id → item.id ON DELETE CASCADE`), `dup_group` (`level` ∈ `identical|same_image|similar`, `state` ∈ `to_review|kept_all|resolved`, `member_count ≥ 2`), `dup_member` (`item_id` UNIQUE, FK cascade), `dup_choice`, `dup_decision` (`item_a < item_b`, `kind` ∈ `not_duplicate|keep_all`), `file_operation`, `file_operation_entry` (`item_id` **sans** clé étrangère), index de data-model.md, `UPDATE schema_version SET version = 2`
- [x] T005 [P] Types de la 002 (`DupLevel`, `KeeperReason`, `GroupState`, `DuplicateCopy`, `DuplicateGroup`, `DuplicateProgress`, conversions texte ↔ base) dans src/core/duplicates/DuplicateTypes.h et src/core/duplicates/DuplicateTypes.cpp
- [x] T006 [P] Régulateur d'E/S adaptatif (seau à jetons 2–32 Mo/s, départ 16 Mo/s, latence de référence glissante, division par 2 au-delà de 3× la référence, +10 %/s au calme, horloge injectable) dans src/core/platform/win/IoGovernor.h et src/core/platform/win/IoGovernor.cpp
- [x] T007 [P] Lecteur régulé : refus des fichiers « en ligne seulement » (`FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS`, `RECALL_ON_OPEN`, `OFFLINE`), lecture `FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN` par blocs alignés de 1 Mo, XXH3-128, annulation coopérative, dans src/core/platform/win/ThrottledReader.h et src/core/platform/win/ThrottledReader.cpp
- [x] T008 [P] État d'alimentation (secteur, batterie, économiseur) et mode d'arrière-plan du fil courant dans src/core/platform/win/PowerState.h et src/core/platform/win/PowerState.cpp
- [x] T009 Ajouter `Library::forgetItems(ids)` (retrait de l'index et des vignettes, sur le fil d'indexation, notifications) et `LibraryDb::removeItems(ids)` dans src/core/library/Library.h, src/core/library/Library.cpp, src/core/index/LibraryDb.h, src/core/index/LibraryDb.cpp
- [x] T010 [P] Tests unitaires du régulateur (horloge simulée) et de la migration 1 → 2 dans tests/unit/tst_duplicates_core.cpp

**Checkpoint**: la base est en version 2, les briques Windows existent.

---

## Phase 3: User Story 1 — Copies identiques regroupées (Priority: P1) 🎯 MVP

**Goal**: retrouver les fichiers au contenu identique et les montrer en groupes.

**Independent Test**: un original + deux copies renommées dans d'autres dossiers + 10 photos différentes → 1 groupe de 3 « Identique ».

### Tests

- [x] T011 [P] [US1] Test d'intégration : copies identiques, tailles égales mais contenus différents, liens physiques non regroupés, vidéos identiques, invariant SC-008, dans tests/integration/tst_duplicates_engine.cpp

### Implementation

- [x] T012 [P] [US1] Règle de la copie à garder (RAW, pixels, poids, date EXIF, date, chemin) et raison dans src/core/duplicates/KeeperRule.h et src/core/duplicates/KeeperRule.cpp
- [x] T013 [P] [US1] Regroupement en étoile autour de la meilleure copie (aucune chaîne), à partir de paires qualifiées, dans src/core/duplicates/Grouper.h et src/core/duplicates/Grouper.cpp
- [x] T014 [US1] Dépôt des tables 002 : candidats `(size_bytes, quick_hash)`, empreintes, groupes, membres, résumé, lectures pour l'interface, dans src/core/duplicates/DuplicateStore.h et src/core/duplicates/DuplicateStore.cpp
- [x] T015 [US1] Moteur : fil d'arrière-plan, attente de la Library inactive, étage « contenu » (candidats → XXH3-128 régulé, `not_candidate` / `pending` / `done` / `skipped_online` / `failed`), fusion des liens physiques, reconstruction des groupes, progression et notifications regroupées, dans src/core/duplicates/DuplicateEngine.h et src/core/duplicates/DuplicateEngine.cpp
- [x] T016 [US1] Contrôleur QML (résumé, liste des groupes, copies, progression) dans src/app/controllers/DuplicatesController.h et src/app/controllers/DuplicatesController.cpp, branché dans src/app/main.cpp
- [x] T017 [US1] Entrée « Doublons » en tête de src/app/qml/FolderList.qml et bascule grille ↔ écran dans src/app/qml/Library.qml
- [x] T018 [US1] Écran src/app/qml/Duplicates.qml et ligne de groupe src/app/qml/DuplicateGroupRow.qml (vignette, nom, dossier, dimensions, poids, format, date, badge de niveau, place récupérable, message « Aucun doublon trouvé. »)

**Checkpoint**: US1 démontrable seule.

---

## Phase 4: User Story 2 — Garder, mettre à la corbeille, annuler (Priority: P1)

**Goal**: choisir les copies gardées et mettre les autres à la corbeille, sans risque, avec journal et annulation.

**Independent Test**: sur le groupe de 3 copies, accepter la suggestion et confirmer → 2 fichiers dans la corbeille ; « Annuler » → ils reviennent identiques.

### Tests

- [x] T019 [P] [US2] Test d'intégration avec la vraie corbeille, dans un dossier temporaire : mise à la corbeille, absence d'effacement définitif, fichier modifié depuis l'analyse laissé en place (`skipped_changed`), refus si aucune copie ne reste, annulation et retour à l'identique, journal, invariant SC-008, dans tests/integration/tst_trash.cpp

### Implementation

- [x] T020 [P] [US2] Corbeille Windows : volume avec corbeille (`SHQueryRecycleBinW`, `DRIVE_FIXED`), `IFileOperation` + `IFileOperationProgressSink` qui refuse dans `PreDeleteItem` tout élément sans `TSF_DELETE_RECYCLE_IF_POSSIBLE`, énumération de la corbeille et restauration par `undelete`, dans src/core/platform/win/RecycleBin.h et src/core/platform/win/RecycleBin.cpp
- [x] T021 [US2] Service de corbeille : `plan()` (récapitulatif, fichiers bloqués, groupes refusés), `execute()` (journal écrit d'abord, vérification FR-019 identité/taille/date + empreinte pour « Identique », exécution sur un fil dédié, retrait de l'index), `journal()`, `undo()`, reprise d'une opération interrompue, dans src/core/fileops/TrashService.h et src/core/fileops/TrashService.cpp
- [x] T022 [US2] Choix « à garder / à la corbeille » (`dup_choice`) et refus de tout écarter dans src/core/duplicates/DuplicateStore.cpp
- [x] T023 [US2] Contrôleur de la corbeille (plan, exécution, progression, journal, annulation) dans src/app/controllers/TrashController.h et src/app/controllers/TrashController.cpp
- [x] T024 [US2] Cases « À la corbeille », étoile et raison, barre d'action dans src/app/qml/Duplicates.qml et src/app/qml/DuplicateGroupRow.qml
- [x] T025 [US2] Récapitulatif avant confirmation (« Annuler » par défaut) dans src/app/qml/TrashSummary.qml
- [x] T026 [US2] Journal des opérations et annulation dans src/app/qml/OperationLog.qml, menu Outils de src/app/qml/AppHeader.qml

**Checkpoint**: MVP (US1 + US2) complet.

---

## Phase 5: User Story 3 — Même image, autre taille, format ou retouche (Priority: P2)

**Goal**: retrouver les quasi-doublons avec un niveau de confiance en mots.

**Independent Test**: original + copies réduite, recompressée, PNG, tournée, éclaircie → un seul groupe ; les photos d'une rafale n'y sont pas.

### Tests

- [x] T027 [P] [US3] Tests unitaires pHash (rotations, distance), score de détail, SSIM normalisé dans tests/unit/tst_duplicates_core.cpp
- [x] T028 [P] [US3] Test d'exactitude SC-001 / SC-002 sur un jeu généré (copies réduites, recompressées, PNG, tournées, éclaircies ; rafales, photos distinctes, images unies) dans tests/accuracy/tst_duplicate_accuracy.cpp

### Implementation

- [x] T029 [P] [US3] Empreinte visuelle depuis la vignette finale (luminance 32×32, DCT, pHash 64 bits ×4 rotations, score de détail) dans src/core/duplicates/VisualFingerprint.h et src/core/duplicates/VisualFingerprint.cpp
- [x] T030 [P] [US3] Vérification des paires : dates EXIF différentes → rejet, SSIM 64×64 normalisé, seuils « Même image » ≥ 0,95 et « Très ressemblante » ≥ 0,85, cache LRU, dans src/core/duplicates/PairVerifier.h et src/core/duplicates/PairVerifier.cpp
- [x] T031 [US3] Étage « visuel » du moteur : photos dont la vignette est `done`, recherche par force brute (Hamming ≤ 12 sur 4 rotations, rapport largeur/hauteur à 2 %), fusion avec les ensembles identiques, RAW + JPEG regroupés, dans src/core/duplicates/DuplicateEngine.cpp
- [x] T032 [US3] Filtre par niveau et tri dans src/app/qml/Duplicates.qml et src/app/controllers/DuplicatesController.cpp
- [x] T033 [US3] Comparaison côte à côte avec zoom et déplacement synchronisés dans src/app/qml/CompareView.qml

---

## Phase 6: User Story 4 — Analyse discrète et incrémentale (Priority: P2)

**Goal**: l'analyse ne gêne jamais l'ordinateur, reprend après fermeture, suit les nouveautés.

**Independent Test**: fermer Picazoo en pleine analyse puis relancer → reprise sans relire ; ajouter une copie → son groupe apparaît en moins d'une minute.

### Tests

- [x] T034 [P] [US4] Tests d'intégration : reprise après fermeture sans relecture (compteur de fichiers lus), copie ajoutée regroupée, pause sans lecture, désactivation, dans tests/integration/tst_duplicates_engine.cpp

### Implementation

- [x] T035 [US4] Déclenchement automatique (FR-033) sur `scanFinished` / `itemUpdated`, réglages `duplicates_enabled` et `duplicates_paused`, pause, reprise, `hold()` / `release()`, pause sur batterie, dans src/core/duplicates/DuplicateEngine.cpp
- [x] T036 [US4] Indicateur « Doublons : n / N » et raison d'attente dans src/app/qml/AppHeader.qml ; réglage « Rechercher les doublons automatiquement » et « Rechercher maintenant » dans le menu Outils
- [x] T037 [US4] Suspension pendant la visionneuse : relayer `hold()` / `release()` depuis src/app/controllers/ViewerController.cpp

---

## Phase 7: User Story 5 — Décisions retenues et ménage rapide (Priority: P3)

**Goal**: « Ce ne sont pas des doublons », « Garder toutes », retrait d'une copie, application en lot, lien depuis la visionneuse.

**Independent Test**: marquer un groupe « Ce ne sont pas des doublons », relancer, réanalyser → il ne revient pas.

### Tests

- [x] T038 [P] [US5] Tests d'intégration des décisions (persistance après réouverture et nouvelle analyse, retrait d'une copie, garder toutes) et de `planAllIdentical()` dans tests/integration/tst_duplicates_engine.cpp

### Implementation

- [x] T039 [US5] Décisions `dup_decision` (`not_duplicate`, `keep_all`), annulation d'une décision, prise en compte par le moteur, dans src/core/duplicates/DuplicateStore.cpp et src/core/duplicates/DuplicateEngine.cpp
- [x] T040 [US5] `planAllIdentical()` dans src/core/fileops/TrashService.cpp et bouton « Appliquer les suggestions à tous les groupes identiques… » dans src/app/qml/Duplicates.qml
- [x] T041 [US5] Menus du groupe et de la copie (Garder celle-ci, Retirer de ce groupe, Garder toutes, Ce ne sont pas des doublons, Revenir à la suggestion) dans src/app/qml/DuplicateGroupRow.qml
- [x] T042 [US5] « n doublons — Voir » dans src/app/qml/InfoPanel.qml

---

## Phase 8: Polish & Cross-Cutting

- [x] T043 [P] Générateur de données de test `picazoo-testdata` (quickstart) dans tools/make-testdata/
- [x] T044 [P] Option `--duplicates` du banc d'essai dans bench/picazoo-bench.cpp
- [x] T045 Mettre à jour README.md (fonctionnalité 002) et docs/feuille-de-route.md
- [x] T046 Compilation Release sans avertissement et `ctest` complet

---

## Dependencies & Execution Order

- **Setup (1)** → **Foundational (2)** → histoires.
- **US1** dépend de la phase 2. **US2** dépend de US1 (les groupes). **US3** dépend de US1 (moteur, groupes). **US4** dépend de US1 (moteur). **US5** dépend de US1 et US2.
- Dans une histoire : tests → types et logique pure → dépôt → moteur/service → contrôleur → QML.

### Parallel Opportunities

- Phase 2 : T005, T006, T007, T008 en parallèle.
- US1 : T012 et T013 en parallèle, puis T014 → T015.
- US2 : T019 et T020 en parallèle.
- US3 : T027, T028, T029, T030 en parallèle.

## Implementation Strategy

1. **MVP** : phases 1, 2, US1, US2 → copies identiques trouvées et mises à la corbeille en toute sécurité.
2. Puis US3 (quasi-doublons), US4 (discrétion et incrémental), US5 (décisions et lot).
3. Valider chaque checkpoint par son test indépendant, puis le quickstart complet.

## Écarts à l'implémentation (2026-11-23)

- T021 : le journal (`OperationJournal`) est intégré à `TrashService` (même fichier), sans changement de comportement.
- T037 : la suspension pendant la visionneuse est relayée par `Main.qml` (`dups.hold()` / `dups.release()`) ; `ViewerController` n'est pas modifié.
- T006 : le correctif disque de la 001 (`4e1178f`, non fusionné) n'apporte qu'une priorité de fil ; `IoGovernor` est donc créé ici (research D5).
- T044 : `picazoo-bench --duplicates` mesure SC-003 ; SC-005 (copie concurrente) reste une mesure manuelle (quickstart).
- Ajouts constatés au banc d'essai : un seul représentant par fichier physique dans les groupes visuels, et une seule lecture par fichier physique (liens physiques).

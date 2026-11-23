# Tasks: Gestion des doublons en lot

**Input**: documents de conception de `specs/006-gestion-doublons-lot/`

**Prerequisites**: [plan.md](plan.md), [spec.md](spec.md), [design.md](design.md), [research.md](research.md), [data-model.md](data-model.md), [contracts/](contracts/), [quickstart.md](quickstart.md)

**Tests**: demandés par la constitution (principe I : opérations sur les fichiers testées sur des dossiers de test) ; ils sont inclus.

**Organization**: par histoire utilisateur (US1 à US5 de la spec), chaque phase étant testable seule.

## Format: `[ID] [P?] [Story] Description`

- **[P]** : parallélisable (fichiers différents, pas de dépendance en cours)
- **[Story]** : histoire utilisateur concernée (US1…US5)

---

## Phase 1: Setup

- [ ] T001 Déclarer `003_doublons_lot.sql` dans la ressource `picazoo_core_sql` de src/core/CMakeLists.txt et passer `kLibrarySchemaVersion` à 3 (`case 3`) dans src/core/index/Migrator.h et src/core/index/Migrator.cpp
- [ ] T002 Ajouter la cible de test tests/integration/tst_duplicates_batch.cpp (glob existant) avec un premier cas vide qui compile

---

## Phase 2: Foundational (bloquant pour toutes les histoires)

- [ ] T003 Écrire la migration src/core/index/migrations/003_doublons_lot.sql : `dup_folder_rule` (`path` UNIQUE COLLATE NOCASE, `kind` ∈ `prefer|avoid`), `dup_batch` (`kind` ∈ `reset|keep_folder|keep_all|not_duplicate`, `label`, `detail`, `group_count`, `created_at`, `undone_at`), `dup_batch_entry` (`batch_id` FK cascade, `item_id` **sans** clé étrangère, `prev_keep`, `new_keep`, PK `(batch_id, item_id)`), colonnes `dup_member.suggested_keep INTEGER NOT NULL DEFAULT 0`, `dup_group.suggestion_reason`, `dup_group.suggestion_detail`, `dup_group.flags INTEGER NOT NULL DEFAULT 0`, `dup_decision.batch_id`, `file_operation.stopped INTEGER NOT NULL DEFAULT 0`, initialisation de `suggested_keep` depuis `center_item_id`, `UPDATE schema_version SET version = 3`
- [ ] T004 [P] Types de la 006 (`GroupQuery`, `RuleKind`, `FolderRule`, `BatchKind`, `BatchResult`, `BatchInfo`, `UndoResult`, `SelectionTotals`, `FolderPair`, `FolderStat`, `SetAsideEntry`, champs `suggested`, `suggestionReason`, `suggestionDetail`, `flags` de `DuplicateGroup`, `isSuggested` de `DuplicateCopy`, libellés des raisons de dossier) dans src/core/duplicates/DuplicateTypes.h et src/core/duplicates/DuplicateTypes.cpp
- [ ] T005 [P] Correspondance des règles (plus long préfixe, insensible à la casse, frontière `\`) dans src/core/duplicates/FolderRules.h et src/core/duplicates/FolderRules.cpp
- [ ] T006 Suggestion : `choose()` pure (préférées, non écartées, `all_avoided`, protection RAW/résolution hors « Identique » → `preferred_worse`) et `applySuggestions(Database&, groupes)` (colonnes de suggestion, place récupérable `COALESCE(c.keep, m.suggested_keep)`, retrait des choix qui ne laisseraient aucune copie) dans src/core/duplicates/Suggestion.h et src/core/duplicates/Suggestion.cpp
- [ ] T007 `kReclaimableSql` et la lecture `group()` sur `COALESCE(c.keep, m.suggested_keep)`, `isSuggested`, raison et drapeaux, dans src/core/duplicates/DuplicateStore.cpp ; `regroup()` appelle `applySuggestions` dans sa transaction dans src/core/duplicates/DuplicateEngine.cpp
- [ ] T008 Copie gardée de référence du plan = copie suggérée gardée, sinon centre gardé, sinon première gardée ; `TrashPlanEntry.level` et `examined` dans src/core/fileops/TrashService.h et src/core/fileops/TrashService.cpp
- [ ] T009 [P] Tests unitaires `FolderRules` et `Suggestion::choose` dans tests/unit/tst_duplicates_core.cpp
- [ ] T010 Test de migration 2 → 3 (base de la 002 avec un groupe et un choix : `suggested_keep` = centre, place inchangée) dans tests/integration/tst_duplicates_batch.cpp

**Checkpoint**: schéma 3 ; sans règle, comportement identique à la 002 (tests de la 002 verts).

---

## Phase 3: User Story 1 — Sélectionner plusieurs groupes et agir en une fois (Priority: P1) 🎯 MVP

**Goal**: sélection de groupes, actions en lot annulables, portée corrigée de la corbeille.

**Independent Test**: 40 groupes « Identique » ; sélection de 20 ; « Garder toutes » ; les 20 sortent de la revue ; « Annuler » les ramène avec leurs choix.

### Tests

- [ ] T011 [P] [US1] Tests `applyBatch` (`reset`, `keep_all`, `not_duplicate` : une transaction, une notification, groupes retirés tout de suite) et `undoBatch` (rétablissement, partie modifiée depuis non rétablie, idempotence) dans tests/integration/tst_duplicates_batch.cpp
- [ ] T012 [P] [US1] Test : un plan préparé pour des groupes donnés ne contient que leurs fichiers (SC-004) et `totals()` = fichiers et octets du plan, dans tests/integration/tst_trash.cpp

### Implementation

- [ ] T013 [US1] `applyBatch`, `undoBatch`, `batches()`, `totals()`, `trashTotals()` (une requête, sans N+1) dans src/core/duplicates/DuplicateStore.h et src/core/duplicates/DuplicateStore.cpp
- [ ] T014 [US1] Sélection dans le modèle (`QSet<GroupId>`, rôle `selected`, `toggle`, `selectRange`, `selectAll`, `clearSelection`, intersection au rechargement et signal `selectionDropped(n)`) dans src/app/models/DuplicateGroupModel.h et src/app/models/DuplicateGroupModel.cpp
- [ ] T015 [US1] Contrôleur : propriétés `selectedCount`, `selectionText`, actions `batchReset`, `batchKeepAll`, `batchNotDuplicates`, `undoBatch`, signal `batchApplied(message, batchId)`, `trashCount` sans N+1, dans src/app/controllers/DuplicatesController.h et src/app/controllers/DuplicatesController.cpp
- [ ] T016 [US1] `prepare(groupIds)` sans repli « tous les groupes » ; retrait de `prepareAllIdentical` de l'interface, dans src/app/controllers/TrashController.h et src/app/controllers/TrashController.cpp
- [ ] T017 [US1] Écran : case de sélection dans l'en-tête de src/app/qml/DuplicateGroupRow.qml ; barre de sélection, barre « groupe courant », message « Annuler » (10 s, Ctrl + Z), Échap vide la sélection, Ctrl + A, Maj + ↑ ↓, Suppr sur la sélection ou le groupe courant, suppression du bouton « Appliquer les suggestions à tous les groupes identiques », dans src/app/qml/Duplicates.qml

**Checkpoint**: US1 démontrable seule ; la corbeille ne prend plus de groupe non choisi.

---

## Phase 4: User Story 2 — Garder ou écarter selon le dossier (Priority: P1)

**Goal**: paires de dossiers, « Garder celles de … », règles durables.

**Independent Test**: `Images\2019` (50) + `Sauvegarde\2019` (50 copies) + 5 doublons internes ; la paire montre 50 groupes ; « Garder celles de Images\2019 » coche les 50 copies de la sauvegarde, les 5 internes ne changent pas.

### Tests

- [ ] T018 [P] [US2] Tests `KeepFolder` (copie du dossier gardée, groupe sans copie dans le dossier ignoré, groupe protégé compté `flagged`), règles (`setFolderRule` change les suggestions sans toucher aux choix faits à la main, suppression, règle introuvable ignorée, `all_avoided`), `folderPairs` (remontée aux racines divergentes) et `internalDuplicates`, dans tests/integration/tst_duplicates_batch.cpp

### Implementation

- [ ] T019 [US2] `KeepFolder` dans `applyBatch` (via `Suggestion::choose` avec une règle « préférer » temporaire), `folderRules`, `setFolderRule`, `removeFolderRule`, `relinkFolderRule`, `folderPairs`, `internalDuplicates` dans src/core/duplicates/DuplicateStore.h et src/core/duplicates/DuplicateStore.cpp
- [ ] T020 [US2] Contrôleur : `batchKeepFolder(folder)`, `selectionFolders()`, `pairs`, `internals`, `rules` (recalcul différé de 500 ms), `keepPairSide`, `addRule`, `removeRule`, `changeRule`, `relinkRule`, dans src/app/controllers/DuplicatesController.h et src/app/controllers/DuplicatesController.cpp
- [ ] T021 [US2] Panneau src/app/qml/FolderPanel.qml (paires, doublons internes, règles, « Ajouter une règle… » avec `FolderDialog`) ouvert par « Dossiers » et la touche D dans src/app/qml/Duplicates.qml ; menu « Garder les copies du dossier… » de la barre de sélection
- [ ] T022 [US2] Raison de dossier et signalements dans la carte (« Suggérée : Dossier préféré : … », « La copie du dossier préféré est moins bonne ») dans src/app/models/DuplicateGroupModel.cpp et src/app/qml/DuplicateGroupRow.qml

**Checkpoint**: une sauvegarde en double se règle en une action.

---

## Phase 5: User Story 3 — Parcourir des milliers de groupes (Priority: P2)

**Goal**: vue compacte, filtres combinés (dossier, paire, type, état, taille, dates), tri, entrée depuis la bibliothèque.

**Independent Test**: 2 000 groupes en vue compacte, ≥ 8 par écran ; filtre dossier `Sauvegarde` → seuls ses groupes.

### Tests

- [ ] T023 [P] [US3] Tests `groups(GroupQuery)` et `count()` : dossier et sous-dossiers (frontière `\`), paire, type (photos, vidéos, RAW + JPEG), état, taille minimale, plage de dates de la copie gardée, tris, dans tests/integration/tst_duplicates_batch.cpp

### Implementation

- [ ] T024 [US3] `groups(const GroupQuery&)` et `count()` en SQL paramétré dans src/core/duplicates/DuplicateStore.h et src/core/duplicates/DuplicateStore.cpp
- [ ] T025 [US3] Modèle sur `GroupQuery` et rôles de la ligne compacte (`keeperName`, `keeperFolder`, `outText`, `flagText`, vignettes) dans src/app/models/DuplicateGroupModel.h et src/app/models/DuplicateGroupModel.cpp
- [ ] T026 [US3] Contrôleur : propriétés de filtre (`folderFilter`, `pairFilter`, `typeFilter`, `stateFilter`, `minBytes`, `yearFrom`, `yearTo`, `sort`, `view`), `shownCount`, `chips`, `clearFilter(key)`, réglages retenus, dans src/app/controllers/DuplicatesController.h et src/app/controllers/DuplicatesController.cpp
- [ ] T027 [P] [US3] Ligne compacte dépliable src/app/qml/DuplicateGroupLine.qml
- [ ] T028 [US3] Écran : bascule Compacte / Détaillée, menu Filtres, menu Tri, puces, « N groupes affichés sur T », en-tête de colonnes, segments avec comptes, dans src/app/qml/Duplicates.qml
- [ ] T029 [US3] Menu contextuel « Voir les doublons de ce dossier » dans src/app/qml/FolderList.qml, relayé par src/app/qml/Library.qml

**Checkpoint**: la liste se lit et se filtre d'un coup d'œil.

---

## Phase 6: User Story 4 — Récapitulatif et exécution des gros lots (Priority: P2)

**Goal**: récapitulatif par niveau et par dossier, « Très ressemblante » non vérifiées à part, préparation hors du fil de l'interface, arrêt.

**Independent Test**: 300 groupes dont 20 « Très ressemblante » non modifiés → 3 sections, la dernière non incluse ; arrêt d'une opération de 2 000 fichiers ; annulation.

### Tests

- [ ] T030 [P] [US4] Tests : `TrashPlan::filtered`, entrées `examined`, arrêt (`requestStop` : `stopped`, entrées `pending`, état `partial`, annulation des seuls fichiers partis), dans tests/integration/tst_trash.cpp

### Implementation

- [ ] T031 [US4] `requestStop()`, colonne `stopped`, `Operation.stopped`, `filtered()`, libellés « Arrêtée » et « Non traité : opération arrêtée » dans src/core/fileops/TrashService.h et src/core/fileops/TrashService.cpp
- [ ] T032 [US4] Préparation sur un fil de travail (`preparing`, `planReady`), résumé par niveau et par dossier (200 fichiers par dossier au plus), `setLevelIncluded`, `confirm()` du plan filtré, `stop()`, progression, dans src/app/controllers/TrashController.h et src/app/controllers/TrashController.cpp
- [ ] T033 [US4] Récapitulatif refait (niveaux à cocher, dossiers dépliables, « Ne seront pas touchés », préparation, bouton avec le nombre de fichiers) dans src/app/qml/TrashSummary.qml
- [ ] T034 [US4] Barre de progression et « Arrêter » dans src/app/qml/Duplicates.qml ; état « Arrêtée » et « Arrêter » dans src/app/qml/OperationLog.qml

**Checkpoint**: un lot de milliers de fichiers se vérifie et s'arrête.

---

## Phase 7: User Story 5 — Revenir sur des décisions prises en lot (Priority: P3)

**Goal**: « Écartés de la revue » : lots annulables, groupes remis dans la revue.

**Independent Test**: 15 groupes « Ce ne sont pas des doublons » en lot ; redémarrage ; le lot est dans « Écartés » ; « Remettre dans la revue » les ramène après regroupement.

### Tests

- [ ] T035 [P] [US5] Tests `setAside()` (groupes `kept_all`, composantes `not_duplicate`) et `restore()` (seules les paires entre les éléments de l'entrée), survie au redémarrage, dans tests/integration/tst_duplicates_batch.cpp

### Implementation

- [ ] T036 [US5] `setAside()` et `restore()` dans src/core/duplicates/DuplicateStore.h et src/core/duplicates/DuplicateStore.cpp
- [ ] T037 [US5] Contrôleur : `setAsideMode`, `setAsideEntries`, `batchList`, `restoreSetAside(indexes)`, `undoBatchFromList(id)` dans src/app/controllers/DuplicatesController.h et src/app/controllers/DuplicatesController.cpp
- [ ] T038 [US5] Vue src/app/qml/SetAsideView.qml (lots, groupes écartés, sélection, « Remettre dans la revue ») et segment « Écartés » dans src/app/qml/Duplicates.qml

**Checkpoint**: toute décision en lot se défait.

---

## Phase 8: Polish & Cross-Cutting

- [ ] T039 Écran de développement `PICAZOO_DEV_SCREEN=dossiers|recap` pour vérifier les nouveaux écrans sans souris, dans src/app/qml/Main.qml et src/app/main.cpp
- [ ] T040 Compiler en Release, faire passer toute la suite CTest, vérifier les écrans par capture (`PICAZOO_SCREENSHOT`) et corriger les erreurs QML (`PICAZOO_LOG_FILE`)
- [ ] T041 [P] Mettre à jour le README (doublons en lot) et cocher la feuille de route dans docs/feuille-de-route.md

---

## Dependencies & Execution Order

- Phase 1 → Phase 2 → histoires. US1 et US2 dépendent de la phase 2 ; US2
  réutilise `applyBatch` de US1 (T013) ; US3 réutilise la sélection de US1 ;
  US4 dépend de T016 ; US5 dépend de T013.
- Ordre conseillé : US1 → US2 → US3 → US4 → US5 → Polish (celui de
  design.md § 2.12).

## Parallel Opportunities

- Phase 2 : T004, T005, T009 en parallèle ; T006 après T004 et T005.
- Dans chaque histoire, le test [P] s'écrit en parallèle de l'implémentation
  du cœur ; T027 (QML seul) en parallèle de T024–T026.

## Implementation Strategy

- **MVP** : phases 1–3 (US1) : sélection, lots annulables, portée corrigée.
- Puis US2 (le plus gros gain de temps), US3, US4, US5, chacune livrable et
  testable seule.

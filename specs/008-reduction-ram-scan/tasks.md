# Tasks: Réduction de la consommation de RAM au scan initial

**Input**: documents de conception de `specs/008-reduction-ram-scan/`

**Prerequisites**: [plan.md](plan.md), [spec.md](spec.md), [research.md](research.md), [data-model.md](data-model.md), [contracts/core-api.md](contracts/core-api.md), [quickstart.md](quickstart.md)

**Tests**: inclus et écrits avant le code qu'ils couvrent, pour la logique pure (`MemoryBudget`) et les changements de comportement observables (`HeifDecoder` via le pipeline, réévaluation du pipeline) ; nécessaire pour vérifier les garanties non-fonctionnelles de la spec sans dépendre d'une machine physique à 8 Go de RAM.

**Organization**: par histoire utilisateur (US1 à US3 de la spec), dans l'ordre de priorité. US1 et US2 partagent la même fondation (budget mémoire, phase 2) ; US3 est une non-régression vérifiée après coup.

## Format: `[ID] [P?] [Story] Description`

- **[P]** : parallélisable (fichiers différents, pas de dépendance en cours)
- **[Story]** : histoire utilisateur concernée (US1…US3)

Rappels pour l'implémentation :
- Compiler depuis un « Developer PowerShell for VS 2026 » : `cmake --build --preset windows-release` ; tests : `ctest --preset windows-release` ; la sortie Qt Test se lit avec `-o fichier,txt`.
- Les sources du cœur et les tests sont pris par des `file(GLOB …)` : un nouveau fichier n'a pas à être déclaré, sauf s'il touche `picazoo_core_sql` (non concerné ici : aucune migration de schéma).
- `Psapi.lib` doit être ajouté au lien de `picazoo_core` pour `GetProcessMemoryInfo` (T001).
- Aucune E/S disque de l'utilisateur n'est modifiée par cette fonctionnalité ; le scan reste lecture seule.

---

## Phase 1: Setup

- [X] T001 Lier `Psapi.lib` dans `target_link_libraries(picazoo_core ...)` de src/core/CMakeLists.txt (nécessaire à `GetProcessMemoryInfo`, voir research.md B1)
- [X] T002 [P] Créer le squelette qui compile de src/core/platform/win/MemoryInfo.h et src/core/platform/win/MemoryInfo.cpp : `struct MemorySnapshot { qint64 totalPhysicalBytes; qint64 availablePhysicalBytes; }`, déclarations de `memorySnapshot()`, `processPeakWorkingSetBytes()`, `processWorkingSetBytes()` (corps provisoires renvoyant des valeurs fixes)
- [X] T003 [P] Créer le squelette qui compile de src/core/jobs/MemoryBudget.h et src/core/jobs/MemoryBudget.cpp : `struct MemoryBudgetInputs { qint64 totalPhysicalBytes; qint64 availablePhysicalBytes; int recommendedCpuThreads; }`, `struct MemoryBudget { int thumbnailThreads; qint64 sqliteCacheBytesPerConnection; qint64 thumbnailLruCacheBytes; }`, déclaration de `computeMemoryBudget(const MemoryBudgetInputs&)` (corps provisoire renvoyant `{1, 0, 0}`) ; test vide `tst_memorybudget` dans tests/unit/tst_memorybudget.cpp

---

## Phase 2: Foundational — budget mémoire et branchement dans les caches (bloque US1, US2)

**But** : un budget mémoire calculé une fois à l'ouverture de la bibliothèque, qui pilote le nombre de threads de vignettes et les plafonds de cache (SQLite, LRU des vignettes) ; à la fin de la phase, le budget existe, se teste et s'applique, même si HEIC n'est pas encore optimisé (phase 3).

- [X] T004 [P] Implémenter `platform::memorySnapshot()` dans src/core/platform/win/MemoryInfo.cpp (`GlobalMemoryStatusEx` sur un `MEMORYSTATUSEX` initialisé avec `dwLength`, retourne `ullTotalPhys`/`ullAvailPhys` ; en cas d'échec de l'appel, renvoie une valeur optimiste équivalente à 16 Go disponibles pour ne jamais brider par erreur)
- [X] T005 [P] Implémenter `platform::processPeakWorkingSetBytes()` et `platform::processWorkingSetBytes()` dans src/core/platform/win/MemoryInfo.cpp (`GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))`, retourne `PeakWorkingSetSize` / `WorkingSetSize` ; 0 si l'appel échoue)
- [X] T006 [US-fondation] Tests de `computeMemoryBudget` dans tests/unit/tst_memorybudget.cpp, les 5 garanties de contracts/core-api.md : (1) `thumbnailThreads >= 1` même avec `availablePhysicalBytes = 0` ; (2) `thumbnailThreads <= recommendedCpuThreads` pour un large éventail d'entrées (`QTest::addColumn` en table) ; (3) avec `totalPhysicalBytes = 32 Go`, `availablePhysicalBytes = 24 Go`, `recommendedCpuThreads = 8` → `thumbnailThreads == 8` et les caches ne sont pas réduits en dessous des valeurs actuelles (`sqliteCacheBytesPerConnection >= 32 Mo` ou `thumbnailLruCacheBytes >= 256 Mo`, selon la formule choisie à l'implémentation) ; (4) avec `totalPhysicalBytes = 8 Go`, `availablePhysicalBytes` entre 3 et 5 Go, `recommendedCpuThreads = 8` → la combinaison `thumbnailThreads × coût mémoire pic estimé par décodage (documenté en commentaire à côté de la fonction) + sqliteCacheBytesPerConnection × 5 + thumbnailLruCacheBytes` reste sous ~1,3 Go (marge sous les 1,5 Go de SC-001/002, le reste du budget couvrant Qt et le reste du processus) ; (5) monotonie : pour `totalPhysicalBytes` fixé, une `availablePhysicalBytes` plus faible ne produit jamais un `thumbnailThreads`, un `sqliteCacheBytesPerConnection` ou un `thumbnailLruCacheBytes` plus élevé qu'une `availablePhysicalBytes` plus forte
- [X] T007 Implémenter `computeMemoryBudget` dans src/core/jobs/MemoryBudget.cpp selon les 5 garanties de T006 : formule documentée en commentaire (coût mémoire pic supposé par décodage selon le nombre de threads, fraction de `availablePhysicalBytes` allouée aux caches SQLite et au cache LRU, bornes mini/maxi) ; faire passer T006
- [X] T008 [US-fondation] Ajouter le paramètre `qint64 cacheBytes = 0` à `Database::open` dans src/core/index/Database.h et src/core/index/Database.cpp : si `cacheBytes <= 0`, comportement actuel (`PRAGMA cache_size=-32768`) ; sinon `PRAGMA cache_size=-<cacheBytes / 1024>` (pages négatives = kibioctets, cohérent avec la valeur actuelle)
- [X] T009 [P] [US-fondation] Ajouter le paramètre `qint64 cacheBytesPerConnection = 0` au constructeur de `ThumbStore` dans src/core/index/ThumbStore.h et src/core/index/ThumbStore.cpp, transmis aux deux appels `Database::open` internes (`write_`, `read_`)
- [X] T010 [P] [US-fondation] Ajouter le paramètre `qint64 maxCostBytes = 0` au constructeur de `ThumbnailImageProvider` dans src/app/providers/ThumbnailImageProvider.h et src/app/providers/ThumbnailImageProvider.cpp : si `maxCostBytes <= 0`, comportement actuel (256 Mo) ; sinon `cache_.setMaxCost(maxCostBytes)`
- [X] T011 Calculer le budget dans `Library::open` (src/core/library/Library.cpp) juste avant la création de `thumbs_` et `pipeline_` : `platform::memorySnapshot()` + `platform::recommendedWorkerThreads()` → `computeMemoryBudget` ; passer `budget.sqliteCacheBytesPerConnection` aux trois `Database::open` (`writeDb_`, `uiDb_`, `workerDb_`) et au constructeur de `ThumbStore` ; remplacer `const int threads = platform::recommendedWorkerThreads();` par `budget.thumbnailThreads` pour `pipeline_->start(...)` ; exposer le `MemoryBudget` calculé via une méthode `Library::memoryBudget() const` (pour T015 et pour que `main.cpp` configure `ThumbnailImageProvider`)
- [X] T012 Passer `library->memoryBudget().thumbnailLruCacheBytes` au constructeur de `ThumbnailImageProvider` dans src/app/main.cpp (ligne `engine.addImageProvider(...)`)
- [X] T013 Compiler et lancer `ctest --preset windows-release` : `tst_memorybudget` vert, `tst_database`, `tst_duplicates_core`, `tst_resume`, `tst_browsing` inchangés et verts (le budget par défaut des outils/tests qui n'appellent pas `Library::open` avec RAM réduite reste celui d'aujourd'hui) — 26/26 tests verts

**Checkpoint** : le budget mémoire existe, se calcule à l'ouverture de la bibliothèque et pilote déjà threads + caches SQLite + cache LRU, même si le coût par décodage HEIC n'est pas encore réduit.

---

## Phase 3: User Story 1 — Premier lancement sans asphyxier une machine modeste (Priority: P1) 🎯 MVP

**Goal** : sur une machine à 8 Go de RAM, le pic de mémoire pendant le scan initial (premier lancement, bibliothèque mixte JPEG/HEIC/RAW/vidéos) reste sous 1,5 Go, sans avertissement Windows ni ralentissement perceptible des autres logiciels (FR-001 à FR-003, FR-008).

**Independent Test** : quickstart.md, validation manuelle étapes 1 à 4, et mesure `picazoo-bench` (étapes « Mesurer le pic de mémoire »).

La phase 2 est un prérequis direct (threads + caches). Cette phase ajoute la réduction du coût par décodage HEIC (la plus grosse source de pic individuel identifiée en research H1) et l'instrumentation de mesure (FR-008).

- [X] T014 [P] [US1] Test de la miniature HEIC comme vignette finale dans tests/unit/tst_decoding.cpp : nouveau test `heicThumbnailBecomesFinal` qui `QSKIP`e si `tests/fixtures/sample.heic` est absent (comme les autres formats réels, cf. tests/fixtures/README.md) ; sinon vérifie qu'un décodage via le chemin vignette (`Decoder::embeddedThumbnail` + logique `goodEnough`, reproduite ou appelée directement selon l'implémentation choisie en T015) produit une image dont `max(w,h) >= kThumbSize - 20` sans jamais appeler `HeifDecoder::decode` à pleine résolution — vérifier en chronométrant (un `sample.heic` haute résolution doit décoder sa miniature largement plus vite que sa pleine résolution) ou, si possible, en instrumentant un compteur d'appels pour le test
- [X] T015 [US1] Dans `ThumbnailPipeline::process()` (src/core/jobs/ThumbnailPipeline.cpp, étape « 1) vignette provisoire »), vérifier que le bloc `else if (item->format == Format::Heic) { embedded = Decoder::embeddedThumbnail(path, Format::Heic); }` est bien atteint pour **tout** job HEIC en attente de vignette, pas seulement `job.priority == Priority::Background` (la condition actuelle `item->thumbState == ThumbState::Pending && job.priority == Priority::Background` limite l'étape 1 aux jobs de fond ; un job prioritaire — visible à l'écran — doit profiter du même raccourci HEIC, sinon la grille déclenche quand même un décodage pleine résolution pour les photos visibles en premier) ; ajuster la condition pour que HEIC passe par l'étape 1 quelle que soit la priorité, tout en gardant JPEG inchangé si la distinction était intentionnelle pour une autre raison (vérifier `005`/`006` avant de changer le comportement JPEG) ; faire passer T014
- [X] T016 [US1] Dans src/core/media/HeifDecoder.cpp, vérifier que `HeifDecoder::thumbnail()` ne matérialise pas elle-même une image plus grande que nécessaire (elle décode déjà la miniature intégrée, pas l'image pleine résolution — confirmé par lecture de code : `decodeHandle(th)` décode un `heif_image_handle` distinct de la primaire, jamais la pleine résolution ; aucun changement nécessaire, `makeThumb` réduit déjà via `fitInto` si besoin)
- [X] T017 [P] [US1] Ajouter la mesure du pic de mémoire dans bench/picazoo-bench.cpp, fonction `benchScan` : relever `platform::processPeakWorkingSetBytes()` juste après le bloc `lib->waitForIdle(24 * 3600 * 1000, true);` existant (phase SC-001) et l'afficher via `report("Pic de mémoire pendant le scan", peakBytes / (1024.0 * 1024.0), "Mo", 1500)`
- [X] T018 [US1] Compiler et exécuter `.\build\windows-release\picazoo-bench.exe --items 100000` : noter le pic de mémoire rapporté et le temps de « Analyse complète avec vignettes » ; si un relevé d'avant cette fonctionnalité est disponible (ou en rejouant sur `git stash`), comparer et consigner l'écart dans quickstart.md ou research.md — deux exécutions consignées dans quickstart.md (« Mesures effectuées (T018) ») : pic de mémoire 142,68 Mo puis 159,89 Mo, très sous le seuil de 1,5 Go (OK) ; seuils de temps (découverte, analyse complète) dépassés sur cette machine de développement partagée avec peu de RAM disponible (~1,6-4,3 Go), cohérent avec le budget qui réduit `thumbnailThreads` dans ce cas — mesure de temps fiable à refaire sur machine dédiée (T019/T024)
- [ ] T019 [US1] Validation manuelle : quickstart.md étapes 1 à 4 (machine à 8 Go de RAM ou VM limitée, bibliothèque mixte avec photos HEIC haute résolution, Gestionnaire des tâches ouvert pendant tout le scan, navigateur et traitement de texte ouverts en parallèle)

**Checkpoint** : US1 démontrable seule — pic de mémoire mesuré et réduit, HEIC ne provoque plus de pic disproportionné.

---

## Phase 4: User Story 2 — Les vignettes continuent d'apparaître vite (Priority: P1)

**Goal** : le budget mémoire ne ralentit pas l'apparition des vignettes de plus de 30 % par rapport à aujourd'hui ; les dossiers visibles restent prioritaires ; le budget s'adapte à la RAM réellement disponible et ne bride pas inutilement une machine généreuse, et se réduit si la RAM disponible chute pendant le scan (FR-002, FR-004, FR-005, FR-007).

**Independent Test** : quickstart.md étape 6 (chronométrage comparatif) et étape 9 (machine généreuse) ; `tst_thumbnailpipeline` pour la réévaluation dynamique.

- [X] T020 [P] [US2] Ajouter `applyMemoryBudget(const MemoryBudget&)` à `ThumbnailPipeline` (src/core/jobs/ThumbnailPipeline.h) : signature et stockage du budget courant (`MemoryBudget currentBudget_` protégé par un mutex existant ou nouveau, cohérent avec `holdMutex_`) — implémenté via `activeThreads_` atomique plutôt qu'un mutex dédié
- [X] T021 [US2] Test d'intégration dans tests/integration/tst_thumbnailpipeline.cpp (nouveau fichier, sur le modèle de tests/support/LibraryFixture.h) : (1) démarrer avec `thumbnailThreads = 4`, enqueue 200 éléments, appeler `applyMemoryBudget` avec `thumbnailThreads = 1` en cours de traitement, vérifier que le scan se termine quand même (tous les éléments atteignent un état terminal, `idle()` devient vrai) sans élément dupliqué ni manquant en base ; (2) appeler `applyMemoryBudget` avec un `thumbnailThreads` plus élevé que celui de démarrage pendant qu'un scan tourne, vérifier qu'aucun nouveau fil n'est démarré avant la fin du scan courant (research B3, pas d'oscillation) — 4/4 passant ; le second scénario attend `isWatching()` puis le résultat observable plutôt que `idle()` seul, pour ne pas manquer le fichier écrit avant que le debounce (500 ms) + stabilité (1000 ms) du `Watcher` ne l'ait mis en file
- [X] T022 [US2] Implémenter la réduction progressive dans `ThumbnailPipeline::workerLoop()` et `applyMemoryBudget()` (src/core/jobs/ThumbnailPipeline.cpp) : quand `currentBudget_.thumbnailThreads` diminue, marquer les fils excédentaires (par exemple par index, les `workers_` au-delà du nouveau compte) pour qu'ils sortent de leur boucle après leur tâche en cours au lieu de rappeler `queue_.pop()` ; ne jamais descendre sous 1 fil actif ; faire passer T021
- [X] T023 [US2] Relire périodiquement la RAM disponible et réappliquer le budget pendant un scan actif : dans `Library` (src/core/library/Library.cpp), un `QTimer` (ou réutilisation de `notifyTimer_` à une cadence plus lente, par exemple toutes les 10 s, voir research B3) qui, tant qu'un scan avec vignettes est en cours (`progress().phase != ScanProgress::Phase::Idle`), relit `platform::memorySnapshot()`, recalcule `computeMemoryBudget` et appelle `pipeline_->applyMemoryBudget(...)` si `thumbnailThreads` a changé
- [ ] T024 [US2] Validation manuelle : quickstart.md étape 6 (chronométrer l'apparition complète des vignettes sur la bibliothèque de référence, comparer à une mesure d'avant cette fonctionnalité, écart ≤ 30 %) et étape 9 (machine à nettement plus de 8 Go de RAM, pas de dégradation du temps de scan, `picazoo-bench --items 100000`)

**Checkpoint** : US1 + US2 livrées ensemble — le budget protège la RAM sans dégrader excessivement la vitesse, et réagit si la RAM disponible change pendant le scan.

---

## Phase 5: User Story 3 — Le scan reste fiable dans la durée (Priority: P2)

**Goal** : non-régression — reprise exacte d'un scan interrompu, bibliothèque finale identique, mêmes limites appliquées à un nouveau dossier ajouté après coup (FR-006).

**Independent Test** : quickstart.md étapes 7 et 8 ; `tst_resume` existant, toujours vert.

- [X] T025 [US3] Exécuter `ctest --preset windows-release -R tst_resume` et vérifier que `resumesPendingThumbnails` et `resumesInterruptedDiscovery` (tests/integration/tst_resume.cpp) restent verts avec le budget mémoire actif (ils passent par `Library::open`, donc par T011 automatiquement) ; si un échec apparaît lié au nombre réduit de threads (par exemple un test qui suppose un parallélisme minimum), ajuster le test ou le budget pour garantir au moins le parallélisme nécessaire à la reprise, sans réintroduire le problème de RAM — confirmé vert dans la passe T013 (26/26, `tst_resume` inclus), sans changement requis
- [ ] T026 [US3] Validation manuelle : quickstart.md étape 7 (scan interrompu par fermeture normale à mi-parcours, relancé, bibliothèque finale complète et sans doublon) et étape 8 (nouveau dossier ajouté après le premier scan, mêmes limites de mémoire observées au Gestionnaire des tâches)

**Checkpoint** : les trois histoires utilisateur sont livrées ; aucune régression sur la fiabilité du scan.

---

## Phase 6: Polish

- [X] T027 [P] Relire et compléter research.md (B2, M1) avec les valeurs numériques finalement retenues dans `computeMemoryBudget` (fractions, bornes, coût mémoire pic supposé par décodage) si elles ont changé pendant l'implémentation par rapport aux hypothèses de T006/T007 — B2 complété avec les constantes réelles de MemoryBudget.cpp (0,75 Gio/thread, référence 8 Gio pour les caches, planchers/plafonds 4-32 Mo et 32-256 Mo) ; B3/B4/H1 déjà fidèles à l'implémentation, aucun changement nécessaire ; M1 inchangé (le mécanisme de mesure décrit correspond au code)
- [X] T028 [P] Vérifier que `tests/fixtures/README.md` mentionne toujours correctement `sample.heic` comme fichier optionnel utilisé par le nouveau test T014 (déjà listé ; confirmer que la description reste exacte) — entrée déjà présente et exacte, aucun changement nécessaire
- [ ] T029 Relecture finale : `ctest --preset windows-release` intégralement vert, `picazoo-bench --items 100000` exécuté une dernière fois avec les valeurs consignées dans quickstart.md ou research.md pour servir de référence à une régression future (FR-008)

---

## Dependencies & Execution Order

- **Phase 1 (Setup)** : T001-T003, indépendantes entre elles, aucune dépendance.
- **Phase 2 (Foundational)** : bloque toutes les user stories. T004-T005 dépendent de T002 ; T006-T007 dépendent de T003 ; T008-T010 indépendantes entre elles ; T011 dépend de T004, T007, T008, T009 ; T012 dépend de T011 ; T013 clôt la phase.
- **US1 (Phase 3)** : dépend de la Phase 2 complète. T014 avant T015-T016 (tests avant code). T017 indépendante du reste de la phase (fichier différent). T018-T019 après T015-T017.
- **US2 (Phase 4)** : dépend de la Phase 2 complète ; indépendante de US1 dans son code (fichiers différents : `ThumbnailPipeline` réévaluation vs `HeifDecoder`/bench), mais partage le même budget. T020 avant T021 (squelette avant test). T022 dépend de T021. T023 dépend de T020/T022. T024 après T022-T023.
- **US3 (Phase 5)** : dépend de la Phase 2 (et profite de US1/US2 déjà faites pour tester le comportement complet, mais peut aussi se vérifier dès la fin de la Phase 2 si on veut avancer en parallèle).
- **Polish (Phase 6)** : après toutes les user stories.

## Parallel Example

Après la Phase 2 (checkpoint atteint), US1 (T014-T019) et US2 (T020-T024) peuvent être menées en parallèle par deux agents différents : elles touchent des fichiers distincts (`HeifDecoder`/`ThumbnailPipeline::process` pour US1, `ThumbnailPipeline` réévaluation/`Library` timer pour US2), à l'exception de `ThumbnailPipeline.cpp` partagé — coordonner T015 (US1) et T022 (US2) s'ils tombent sur les mêmes lignes de `process()`/`workerLoop()`.

## Implementation Strategy

**MVP = US1 seule** (Phases 1-3) : réduit déjà le pic de mémoire mesurable (budget + HEIC), avec une instrumentation pour le prouver (`picazoo-bench`). US2 (Phase 4) ajoute la robustesse dynamique (RAM qui varie pendant le scan, non-régression de vitesse) ; US3 (Phase 5) confirme qu'aucune garantie de fiabilité existante n'a été cassée. Livrer US1 en premier permet de mesurer un vrai avant/après sur machine réelle avant d'affiner le compromis vitesse/mémoire de US2.

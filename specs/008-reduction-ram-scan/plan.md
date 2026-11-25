# Implementation Plan: Réduction de la consommation de RAM au scan initial

**Branch**: `008-reduction-ram-scan` | **Date**: 2026-11-25 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/008-reduction-ram-scan/spec.md`

## Summary

Le scan initial (et tout scan avec génération de vignettes) peut faire monter
Picazoo à 2,5 Go de RAM. Trois causes combinées, relevées dans le code
existant :

1. **HEIC décodé à pleine résolution avant réduction** (`HeifDecoder::decode`)
   alors que JPEG (TurboJPEG) et RAW (LibRaw + TurboJPEG) réduisent déjà
   pendant le décodage. Une photo iPhone 48 Mpx alloue donc temporairement
   ~150-200 Mo par décodage HEIC, multiplié par le nombre de threads de
   vignettes actifs en même temps.
2. **Nombre de threads de vignettes dimensionné sur le CPU, jamais sur la
   RAM** (`recommendedWorkerThreads`) : une machine à beaucoup de cœurs mais
   peu de RAM peut lancer autant de décodages pleine résolution en parallèle
   qu'une machine bien pourvue en RAM.
3. **Mémoire fixe cumulée indépendante de la RAM de la machine** : trois
   connexions SQLite dans `Library` (`writeDb_`, `uiDb_`, `workerDb_`) et deux
   dans `ThumbStore` (`write_`, `read_`), chacune à 32 Mo de cache
   (`PRAGMA cache_size=-32768`), plus un cache LRU de vignettes
   (`ThumbnailImageProvider`) plafonné à 256 Mo quel que soit le contexte.

Le plan retient trois idées :

1. **Un budget mémoire du scan, calculé une fois à l'ouverture de la
   bibliothèque et réévalué périodiquement**, à partir de la RAM
   physiquement disponible (`GlobalMemoryStatusEx`). Il remplace
   `recommendedWorkerThreads()` (renommée et étendue) pour fixer le nombre de
   threads de vignettes, et réduit ce nombre (jamais en dessous de 1) si la
   RAM disponible chute pendant le scan. Les budgets SQLite (`cache_size`) et
   le cache LRU des vignettes sont des fractions fixes de ce même budget, pas
   des constantes indépendantes.
2. **Le décodage HEIC réduit pendant le décodage, pas après.** `libheif`
   n'offre pas de réduction native au décodage (contrairement à TurboJPEG) ;
   la solution est de décoder la **miniature intégrée** du conteneur HEIC
   quand elle est assez grande pour la vignette (la plupart des photos
   iPhone en ont une d'environ 1600×1200), et de ne décoder l'image pleine
   résolution que si elle manque ou est trop petite — symétrique à ce que
   `ThumbnailPipeline::process` fait déjà pour JPEG (étape 1 « vignette
   provisoire » / étape 2 « vignette définitive »), étendu pour que HEIC
   utilise sa miniature comme résultat **final** quand elle suffit, au lieu
   d'un décodage pleine résolution systématique.
3. **Mesurer pour vérifier.** `picazoo-bench` gagne une mesure du pic de
   mémoire du processus (`GetProcessMemoryInfo`, `PeakWorkingSetSize`) autour
   du scénario SC-001/SC-002 déjà présent, pour chiffrer avant/après et
   détecter une régression future (FR-008).

## Technical Context

**Language/Version**: C++20 (MSVC, Visual Studio Build Tools 2026), comme
001–007 ; aucun changement QML requis (comportement interne).

**Primary Dependencies**: celles des fonctionnalités précédentes, **sans
ajout de bibliothèque** : Qt 6.12 LTS (Core, Gui), libheif (déjà lié, miniature
intégrée déjà exposée côté API — `heif_image_handle_get_list_of_thumbnail_IDs`
/ `heif_image_handle_get_thumbnail`, déjà utilisés par
`HeifDecoder::thumbnail`), SQLite 3, API Windows déjà disponibles dans le SDK
(`GlobalMemoryStatusEx` de `<windows.h>`, `GetProcessMemoryInfo` de `psapi.h`,
déjà lié indirectement via `shell32`/`ole32` ; `Psapi.lib` à ajouter).

**Storage**: `library.db` et `thumbs.db`, inchangés (pas de migration de
schéma). Le changement porte sur le réglage des connexions SQLite déjà
ouvertes (`cache_size`), pas sur leur structure.

**Testing**: Qt Test + CTest. Nouveaux tests unitaires pour le budget mémoire
(`tst_memorybudget`, logique pure, pas de vraie RAM lue : la fonction qui
calcule le budget à partir d'une RAM disponible donnée est testée en
injectant la valeur) et pour la miniature HEIC comme résultat final
(`tst_decoding`, complété, avec un fichier HEIC de test — `QSKIP` si absent
comme aujourd'hui pour les formats réels). `picazoo-bench` complété pour
mesurer le pic de mémoire (vérification manuelle des seuils, pas un test
automatisé : la RAM disponible dépend de la machine).

**Target Platform**: Windows 10 et 11 x64. Le nouveau code propre à Windows
(lecture de la RAM disponible, pic de mémoire du processus) reste dans
`core/platform/win`, comme `CpuInfo`.

**Project Type**: application de bureau : cœur `picazoo_core`, application
`picazoo`, visionneuse autonome `picazoo-viewer` (celle-ci ne scanne pas :
hors scope, voir Assumptions de la spec).

**Performance Goals**: pic de mémoire du scan ≤ 1,5 Go sur la bibliothèque et
la machine de référence (SC-001, SC-002) ; ralentissement de l'apparition des
vignettes ≤ 30 % sur la même bibliothèque (SC-003) ; pas de dégradation sur
une machine à nettement plus de 8 Go de RAM (SC-006).

**Constraints**: aucun nouveau réglage utilisateur (le budget se calcule
seul, FR-001) ; les garanties de reprise et d'exactitude du scan (001, 002,
006) restent inchangées (FR-006) ; le budget doit s'adapter à la baisse si la
RAM disponible diminue pendant le scan (FR-002), jamais en dessous de
1 thread de vignette actif.

**Scale/Scope**: une fonction de calcul de budget (threads + fractions de
cache), un changement ciblé dans `HeifDecoder::decode` et
`ThumbnailPipeline::process`, un réglage dynamique du `cache_size` SQLite et
du plafond du cache LRU de vignettes, une mesure ajoutée au banc d'essai.
Aucun nouvel écran, aucune nouvelle table.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principe | Vérification | Statut |
|---|---|---|
| I. Les fichiers de l'utilisateur sont sacrés | Aucune action sur les fichiers de l'utilisateur ; le scan continue à ne faire que lire. Le budget mémoire ne touche à aucune opération de corbeille, déplacement ou copie. | ✅ |
| II. Local et privé | Rien ne quitte la machine ; la RAM disponible est lue localement (`GlobalMemoryStatusEx`), jamais transmise. | ✅ |
| III. Fluide sur une vraie bibliothèque | Objectif direct de cette fonctionnalité : la grille reste navigable et se remplit vite pendant le scan sous le nouveau budget (FR-004, FR-005, FR-007) ; non-régression vérifiée par SC-003 et SC-006. | ✅ |
| IV. Le logiciel propose, l'utilisateur décide | Pas de nouvelle suggestion ni de nouvelle décision demandée à l'utilisateur ; le comportement est interne (Assumptions de la spec). | ✅ |
| V. Simple comme Picasa | Aucun nouvel écran ni réglage visible ; rien à apprendre pour l'utilisateur. | ✅ |
| Contraintes produit | Le nouveau code dépendant de Windows (lecture de la RAM, pic mémoire du processus) reste dans `core/platform/win`, derrière des fonctions sans type Windows dans leur en-tête, comme `CpuInfo` et `PowerState` ; une autre plateforme fournirait sa propre implémentation (`GetMemInfo`/`sysinfo` sur Linux, `host_statistics64` sur macOS). | ✅ |
| Cycle de travail | Spec en français, clarifiée (plafond de RAM, machine de référence, compromis vitesse, mesure de référence) ; pas d'opération sur fichiers donc pas de nouveau test « dossiers temporaires » requis par ce point, mais le comportement non-fonctionnel reste vérifié par banc d'essai et tests unitaires. | ✅ |

**Re-check après la phase 1** : le budget mémoire est une fonction pure
(entrées : RAM disponible, RAM totale, nombre de cœurs ; sortie : nombre de
threads de vignettes et fractions de cache), testée sans dépendance Windows
réelle ; seule la lecture de la RAM disponible est spécifique à Windows et
isolée dans `core/platform/win/MemoryInfo`. Le changement sur `HeifDecoder`
est un raffinement local sans nouvelle dépendance. Aucune violation, donc
rien dans Complexity Tracking.

## Project Structure

### Documentation (this feature)

```text
specs/008-reduction-ram-scan/
├── plan.md              # ce fichier
├── research.md          # phase 0 : décisions B1–B4, H1–H2, M1
├── data-model.md        # phase 1 : Budget mémoire du scan (pas de table SQL)
├── quickstart.md        # phase 1 : mesures avant/après, validation manuelle
├── contracts/
│   └── core-api.md      # MemoryInfo, MemoryBudget, changements Decoder/ThumbnailPipeline/ThumbStore/Database
├── checklists/
│   └── requirements.md
└── tasks.md             # phase 2 (/speckit-tasks)
```

### Source Code (repository root)

Fichiers nouveaux, et existants modifiés signalés par « ~ » :

```text
src/
├── core/
│   ├── platform/win/
│   │   ├── MemoryInfo               # B1 : RAM totale/disponible (GlobalMemoryStatusEx), pic du processus (GetProcessMemoryInfo)
│   │   └── ~CpuInfo                 # renommage/extension : recommendedWorkerThreads() devient une entrée du budget
│   ├── jobs/
│   │   ├── MemoryBudget             # B2 : fonction pure RAM dispo + cœurs -> threads, cache_size SQLite, plafond cache vignettes
│   │   └── ~ThumbnailPipeline       # B3 : réévalue le budget périodiquement, redimensionne le pool de threads
│   ├── media/
│   │   └── ~HeifDecoder             # H1 : décode la miniature intégrée comme résultat final quand elle suffit
│   ├── index/
│   │   ├── ~Database                # B4 : cache_size paramétrable (plus une constante)
│   │   └── ~ThumbStore              # B4 : connexions ouvertes avec le cache_size du budget
│   └── ~CMakeLists.txt              # lien Psapi.lib (GetProcessMemoryInfo)
├── app/
│   └── providers/
│       └── ~ThumbnailImageProvider  # B4 : plafond du cache LRU fourni par le budget, pas une constante
└── ~library/Library.cpp             # calcule le budget à l'ouverture, le passe au pipeline et aux caches

bench/
└── ~picazoo-bench.cpp               # M1 : mesure du pic de mémoire du processus autour de benchScan (SC-001/002)

tests/
├── unit/tst_memorybudget.cpp        # nouveau : fonction pure de calcul du budget
├── unit/~tst_decoding.cpp           # miniature HEIC comme résultat final (QSKIP si sample.heic absent)
└── ~CMakeLists.txt
```

**Structure Decision**: on reste dans les modules existants
(`platform/win`, `jobs`, `media`, `index`). Le calcul du budget est isolé
dans une fonction pure (`MemoryBudget`, dans `core/jobs` à côté de
`ThumbnailPipeline` qui en est le principal consommateur) pour rester
testable sans dépendance Windows ; la seule lecture réellement
plateforme-dépendante (RAM disponible, pic du processus) est confinée à
`core/platform/win/MemoryInfo`, sur le modèle de `CpuInfo` et `PowerState`.
Aucun nouveau composant QML, aucune nouvelle table : la fonctionnalité est
entièrement interne au cœur.

## Ordre de réalisation suggéré

1. `platform/win/MemoryInfo` (RAM disponible/totale, pic du processus) +
   tests. Base factuelle pour tout le reste.
2. `jobs/MemoryBudget` (fonction pure) + `tst_memorybudget`. Peut se
   développer et se tester en parallèle du point 1 (les deux se rejoignent
   dans `Library::open`).
3. Brancher le budget dans `Library::open` : threads du pipeline,
   `cache_size` des cinq connexions SQLite, plafond du cache LRU de
   `ThumbnailImageProvider`. Réévaluation périodique côté
   `ThumbnailPipeline` (US1, US2).
4. `HeifDecoder` : miniature intégrée comme résultat final quand elle
   suffit, branchée dans `ThumbnailPipeline::process` à l'endroit où JPEG le
   fait déjà (US1, edge case « photos iPhone »).
5. `picazoo-bench` : mesure du pic de mémoire autour de `benchScan`, pour
   chiffrer SC-001/SC-002/SC-003/SC-006 avant de clore la fonctionnalité
   (US3, non-régression).

## Complexity Tracking

*Aucune violation de la constitution.*

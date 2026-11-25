# Data Model: Réduction de la consommation de RAM au scan initial

**Feature**: [spec.md](spec.md) · **Date**: 2026-11-25

Cette fonctionnalité n'ajoute et ne modifie **aucune table** de `library.db`
ni de `thumbs.db` : aucune migration de schéma n'est nécessaire. Elle ajoute
une entité **en mémoire seulement**, recalculée à chaque ouverture de la
bibliothèque.

## Budget mémoire du scan (`MemoryBudget`)

Structure en mémoire, non persistée, produite par une fonction pure
(`computeMemoryBudget`, voir [research.md](research.md#b2--budget-mémoire-du-scan--une-fonction-pure)
et [contracts/core-api.md](contracts/core-api.md)).

| Champ | Type | Signification |
|---|---|---|
| `thumbnailThreads` | `int` (≥ 1) | Nombre de fils de calcul actifs dans `ThumbnailPipeline` pendant le scan. Remplace l'usage direct de `recommendedWorkerThreads()` comme valeur finale. |
| `sqliteCacheBytesPerConnection` | `qint64` | Valeur appliquée à `PRAGMA cache_size` (en octets, convertie en pages négatives comme aujourd'hui) pour chacune des cinq connexions ouvertes par `Library`/`ThumbStore`. |
| `thumbnailLruCacheBytes` | `qint64` | Plafond du cache LRU de `ThumbnailImageProvider` (remplace la constante `256 * 1024 * 1024`). |

**Entrées** (`MemoryBudgetInputs`, voir research B1/B2) :

| Champ | Type | Source |
|---|---|---|
| `totalPhysicalBytes` | `qint64` | `MemorySnapshot::totalPhysicalBytes` (`GlobalMemoryStatusEx`). |
| `availablePhysicalBytes` | `qint64` | `MemorySnapshot::availablePhysicalBytes`, relu périodiquement pendant le scan (research B3). |
| `recommendedCpuThreads` | `int` | Sortie actuelle de `platform::recommendedWorkerThreads()` (CpuInfo), conservée comme plafond haut : le budget mémoire ne peut qu'abaisser ce nombre, jamais l'augmenter. |

**Cycle de vie** :

- Calculé une première fois dans `Library::open`, avant `pipeline_->start(...)`.
- Réévalué par `ThumbnailPipeline` à intervalle régulier pendant qu'un scan
  avec vignettes est actif (research B3) ; seul `thumbnailThreads` peut
  changer en cours de route (à la baisse), les caches SQLite et LRU restent
  fixés pour la durée de vie de la bibliothèque ouverte (les rouvrir à chaud
  n'apporterait rien : `cache_size` ne libère pas de mémoire déjà en cache
  de façon utile pour ce scénario).
- Non persisté : recalculé à chaque lancement de Picazoo, à partir de l'état
  courant de la machine (une machine qui gagne de la RAM entre deux
  lancements en profite directement, FR-002).

**Invariants** (vérifiés par `tst_memorybudget`) :

- `thumbnailThreads >= 1` dans tous les cas (jamais de scan totalement
  arrêté faute de budget).
- `thumbnailThreads <= recommendedCpuThreads` (le budget mémoire ne force
  jamais plus de parallélisme que ce que le CPU recommande).
- Sur une machine de référence (8 Go total, ~4-5 Go disponible au lancement,
  cohérent avec un usage courant), le budget produit un pic théorique
  compatible avec l'objectif de 1,2-1,5 Go (SC-001/SC-002), compte tenu de
  H1 (coût réduit par décodage HEIC).
- Sur une machine à beaucoup plus de RAM disponible, le budget ne doit pas
  s'écarter sensiblement du comportement actuel (SC-006) : les fractions
  convergent vers le plafond CPU et vers des caches proches des valeurs
  actuelles quand `availablePhysicalBytes` est grand.

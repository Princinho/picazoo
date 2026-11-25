# Research: Réduction de la consommation de RAM au scan initial

**Feature**: [spec.md](spec.md) · **Date**: 2026-11-25

État des lieux relevé dans le code avant de décider :

- `ThumbnailPipeline::start(int threads)` lance `threads` fils de calcul,
  tous capables de décoder une image à pleine résolution avant réduction
  selon le format (`process()`, étape 2). `threads` vient de
  `platform::recommendedWorkerThreads()` (`CpuInfo.cpp`), qui ne regarde que
  la topologie du processeur (cœurs performants/efficaces), jamais la RAM.
- `HeifDecoder::decode()` appelle `heif_decode_image` sans option de taille :
  l'image RGBA pleine résolution est toujours matérialisée
  (`heif_image_get_plane_readonly2` puis `convertToFormat` — copie
  supplémentaire), et seulement réduite après coup par `fitInto`. Pour une
  photo iPhone 48 Mpx (8064×6048), le buffer RGBA seul pèse
  8064×6048×4 ≈ 185 Mo, avant la copie `Format_RGB32` (autant à nouveau,
  brièvement). `JpegDecoder::decode()` et `RawPreview::decode()` réduisent
  déjà *pendant* le décodage (facteur d'échelle TurboJPEG natif).
- `HeifDecoder::thumbnail()` existe déjà et décode la **miniature intégrée**
  du conteneur HEIC (`heif_image_handle_get_thumbnail`) ; elle n'est
  utilisée aujourd'hui que par `Decoder::embeddedThumbnail`, appelée par
  `ThumbnailPipeline::process` à l'étape 1 (« vignette provisoire ») mais
  seulement pour produire une vignette *provisoire*, jamais retenue comme
  résultat final même quand elle est assez grande — contrairement au même
  mécanisme pour JPEG, où l'embarquée peut devenir la vignette *finale* si
  `goodEnough` (`max(w,h) >= kThumbSize - 20`).
- `Library::open` ouvre cinq connexions SQLite au total : `writeDb_`,
  `uiDb_`, `workerDb_` (dans `Library`) et `write_`, `read_` (dans
  `ThumbStore`, construit par `Library::open`). `Database::open` fixe
  `PRAGMA cache_size=-32768` (32 Mo) pour chacune, sans tenir compte du
  nombre de connexions ni de la RAM de la machine : 5 × 32 Mo = 160 Mo de
  cache SQLite possible rien que pour ça, en plus des pages réellement
  utilisées.
- `ThumbnailImageProvider` (dans `src/app`, utilisé par la grille QML) garde
  un `QCache<QString, QImage>` plafonné à 256 Mo (`cache_.setMaxCost(256 *
  1024 * 1024)`), constante fixe, plus un `QThreadPool` de 4 threads pour
  décompresser du JPEG stocké en servant les vignettes déjà calculées
  (distinct du pipeline de calcul des vignettes).
- Aucune mesure de mémoire n'existe dans le projet aujourd'hui (ni dans
  `picazoo-bench`, ni dans les tests) : seul le temps est mesuré.
  `platform::readCounters()` (`ThrottledReader`) compte les octets *lus au
  disque*, pas la mémoire du processus.
- Le modèle d'E/S de la 002 (`IoGovernor`, seau à jetons qui s'adapte à la
  latence observée) est un précédent direct pour un mécanisme qui
  s'autorégule pendant le scan sans réglage utilisateur — même esprit,
  appliqué ici à la mémoire plutôt qu'au débit disque.

---

## B1 — Lire la RAM disponible et le pic de mémoire du processus

**Decision**: ajouter `core/platform/win/MemoryInfo`, sur le modèle de
`CpuInfo` et `PowerState` (fonctions libres, pas de type Windows dans
l'en-tête) :

```cpp
struct MemorySnapshot { qint64 totalPhysicalBytes; qint64 availablePhysicalBytes; };
MemorySnapshot memorySnapshot();           // GlobalMemoryStatusEx
qint64 processPeakWorkingSetBytes();       // GetProcessMemoryInfo (PeakWorkingSetSize)
qint64 processWorkingSetBytes();           // GetProcessMemoryInfo (WorkingSetSize), pour SC-005
```

**Rationale**: `GlobalMemoryStatusEx` donne `ullTotalPhys` et
`ullAvailPhys`, suffisant pour dimensionner un budget sans sur-ingénierie
(pas besoin de compteurs de performance). `GetProcessMemoryInfo` (Psapi)
donne `PeakWorkingSetSize`, qui correspond à ce qu'affiche le Gestionnaire
des tâches dans la colonne « Mémoire » au pic (clarification de session) —
c'est la même grandeur, pas une approximation différente. Isoler ces deux
lectures dans un fichier dédié garde `MemoryBudget` (la logique, testable)
indépendante de l'API Windows réelle.

**Alternatives considered**:
- *`QProcess`/`QStorageInfo` côté Qt* : Qt n'expose pas la RAM disponible du
  système ni le pic mémoire du processus ; il faudrait de toute façon l'API
  Windows.
- *Working Set Size au lieu de Peak Working Set Size pour le pic* : le
  Gestionnaire des tâches affiche un instantané qui redescend ; pour détecter
  le *pic* pendant un banc d'essai automatisé (FR-008), il faut
  l'échantillonner nous-mêmes ou lire `PeakWorkingSetSize`, qui est la valeur
  maximale atteinte depuis le départ du processus et correspond exactement à
  ce qu'un utilisateur observerait en gardant le Gestionnaire des tâches
  ouvert pendant tout le scan.
- *Private Bytes (`PagefileUsage`)* : écarté par la clarification de session
  (la mesure de référence est le working set du Gestionnaire des tâches).

## B2 — Budget mémoire du scan : une fonction pure

**Decision**: `core/jobs/MemoryBudget`, logique pure (pas de QObject, pas
d'appel Windows direct — les valeurs lui sont passées) :

```cpp
struct MemoryBudgetInputs {
    qint64 totalPhysicalBytes;
    qint64 availablePhysicalBytes;
    int recommendedCpuThreads;   // sortie actuelle de CpuInfo::recommendedWorkerThreads()
};
struct MemoryBudget {
    int thumbnailThreads;        // >= 1
    qint64 sqliteCacheBytesPerConnection; // PRAGMA cache_size
    qint64 thumbnailLruCacheBytes;        // plafond ThumbnailImageProvider
};
MemoryBudget computeMemoryBudget(const MemoryBudgetInputs&);
```

Règle : sur une machine « de référence » (8 Go de RAM totale), le budget
cible un pic total de l'ordre de 1,2 à 1,5 Go pour le scan (SC-001, SC-002) :
cache SQLite réduit (par exemple 8 Mo/connexion au lieu de 32, soit 40 Mo au
lieu de 160 pour les cinq connexions), cache LRU de vignettes réduit en
proportion de la RAM disponible (par exemple un quart de la RAM disponible,
borné entre 32 et 256 Mo), et surtout un nombre de threads de vignettes
plafonné par une estimation du coût mémoire pic d'un décodage (voir H1 : avec
la miniature HEIC comme résultat final, le pire cas redescend nettement).
Sur une machine à beaucoup plus de RAM, le budget doit converger vers le
comportement actuel (ne pas brider inutilement, SC-006) : les fractions sont
calculées à partir de `availablePhysicalBytes`, pas de constantes absolues.

**Rationale**: une fonction pure se teste exhaustivement sans machine réelle
(`tst_memorybudget` injecte des valeurs de RAM totale/disponible et vérifie
les bornes), conformément au principe III (fluide, vérifiable) et à la façon
dont `ZoomMotion` (007) a déjà été isolé pour les mêmes raisons. Elle est le
point unique où les trois sources de surconsommation (threads, cache SQLite,
cache LRU) sont mises en cohérence les unes avec les autres, au lieu de trois
constantes indépendantes comme aujourd'hui.

**Alternatives considered**:
- *Garder des constantes mais plus petites* : ne résout pas le cas « machine
  avec beaucoup de cœurs mais peu de RAM » (edge case de la spec) : le nombre
  de threads resterait lié au seul CPU.
- *Laisser l'utilisateur régler un plafond de RAM dans Outils* : rejeté par
  les Assumptions de la spec (pas de nouveau réglage sauf besoin réel
  identifié ici ; aucun n'est apparu pendant la conception).
- *Mesurer la RAM réellement consommée par un décodage et ajuster a
  posteriori (contrôleur asservi, comme `IoGovernor` pour le débit disque)* :
  plus précis mais plus complexe ; non retenu pour une première version,
  le calcul a priori (B2) combiné à H1 (réduire le coût par décodage) suffit
  aux seuils visés. Le réexamen périodique (B3) couvre le cas « RAM qui
  diminue pendant le scan » sans contrôleur asservi.

## B3 — Réévaluation périodique pendant le scan

**Decision**: `ThumbnailPipeline` relit le budget (RAM disponible) à
intervalle raisonnable (par exemple toutes les 10 secondes, ou à chaque
notification de lot comme le fait déjà `flush()` à 200 ms près, mais à une
cadence plus lente pour cette lecture) et ajuste le nombre de threads actifs
à la baisse si la RAM disponible a significativement diminué, jamais en
dessous de 1. À la hausse, le nombre de threads ne remonte pas
automatiquement au-delà du budget initial pendant le même scan (évite les
oscillations) ; il repart du budget courant au prochain scan.

**Rationale**: FR-002 et l'edge case « RAM disponible qui diminue pendant le
scan » exigent une réaction, pas seulement un calcul figé au démarrage.
Réduire le nombre de threads actifs (sans toucher à `PriorityQueue`, qui
reste la même file à trois niveaux) est le levier le plus direct et le plus
sûr : les threads en trop terminent leur travail en cours puis ne redemandent
plus de nouvel élément tant que le budget réduit n'est pas respecté.

**Alternatives considered**:
- *Tuer et relancer des threads* : plus complexe que de simplement ne pas
  réalimenter certains threads (`ThumbnailPipeline::workerLoop` boucle sur
  `queue_.pop()` ; un thread « excédentaire » peut sortir de sa boucle
  proprement en ajoutant un drapeau d'arrêt ciblé) ; détail d'implémentation
  laissé à `/speckit-tasks`.
- *Ne rien faire pendant le scan (budget figé au démarrage)* : ne couvre pas
  l'edge case explicitement listé dans la spec ; risque réel sur une machine
  à 8 Go si l'utilisateur ouvre un gros logiciel pendant le scan.

## B4 — Caches SQLite et cache LRU de vignettes pilotés par le budget

**Decision**: `Database::open` accepte un `cache_size` en paramètre (valeur
par défaut = comportement actuel, 32 Mo, pour les appels qui ne passent pas
par le scan, comme les outils de test) ; `Library::open` calcule le budget
une fois et l'applique aux cinq connexions (`writeDb_`, `uiDb_`,
`workerDb_`, et les deux de `ThumbStore`, qui reçoit le `cache_size` en
paramètre de construction). `ThumbnailImageProvider` reçoit son plafond de
cache LRU en paramètre de construction (fourni par `Library`/`AppController`
à la création), au lieu de la constante `256 * 1024 * 1024`.

**Rationale**: le budget doit porter sur l'ensemble des caches, pas
seulement sur les threads, pour atteindre 1,2-1,5 Go de pic (SC-001/002) :
160 Mo de cache SQLite à eux seuls représentent plus de 10 % du plafond visé.
Passer ces valeurs en paramètre (plutôt que lire le budget depuis un singleton
global) garde chaque classe testable indépendamment, dans l'esprit du code
existant (`Database`, `ThumbStore`, `ThumbnailImageProvider` ne connaissent
aujourd'hui aucune notion globale).

**Alternatives considered**:
- *Un seul `cache_size` partagé par une variable globale* : casserait la
  testabilité actuelle de `Database` (utilisé seul dans plusieurs tests avec
  son comportement par défaut) et irait à l'encontre du style du projet
  (pas de singletons globaux ailleurs dans `core`).

## H1 — HEIC : la miniature intégrée comme vignette finale quand elle suffit

**Decision**: étendre `ThumbnailPipeline::process()` pour que l'étape « 1)
vignette provisoire » (aujourd'hui réservée à JPEG et à une vignette
*provisoire* pour HEIC) traite HEIC comme JPEG : si
`Decoder::embeddedThumbnail(path, Format::Heic)` renvoie une image dont
`max(w,h) >= kThumbSize - 20`, elle devient directement la vignette
**finale** (`ThumbState::Done`), sans jamais appeler
`HeifDecoder::decode()` à pleine résolution pour cet élément. Le décodage
pleine résolution HEIC (toujours nécessaire pour le zoom dans la
visionneuse, hors scope de cette fonctionnalité) n'est donc affecté que pour
le calcul de la vignette de la grille.

**Rationale**: c'est le point relevé en amont de la spec : une miniature de
conteneur HEIC fait typiquement 1600×1200 ou plus (largement suffisant pour
`kThumbSize = 320`), et son décodage (`HeifDecoder::thumbnail`) matérialise
une image bien plus petite que la pleine résolution (iPhone 48 Mpx ≈ 185 Mo
en RGBA contre ~8 Mo pour 1600×1200 RGBA) : un facteur ~20 sur le pic par
décodage HEIC, pour le cas le plus courant (photos iPhone). C'est
exactement le mécanisme déjà en place pour JPEG (`goodEnough`), simplement
pas encore appliqué à HEIC pour la vignette *finale*.

**Alternatives considered**:
- *Demander à libheif de décoder à une résolution réduite directement* :
  explorée dans le code source de libheif vendu par vcpkg
  (`heif_decoding_options`, `heif_image_scale_image`) : l'API de décodage ne
  propose pas de réduction *pendant* le décodage (contrairement à
  `heif_image_scale_image`, qui réduit une image **déjà décodée**, donc
  après le pic). Utiliser la miniature intégrée est donc la seule voie qui
  évite réellement le pic, pour les fichiers qui en ont une.
- *Scale_image après décodage complet* : ne change rien au pic (l'image
  pleine résolution a déjà été matérialisée avant la réduction) ; rejeté.
- *Décoder seulement une tuile/région HEIC* : HEIC encode en tuiles
  (`heif_tiling.h`) mais l'extraction d'une région précise serait plus
  complexe que d'utiliser une miniature déjà prévue à cet effet par le
  format, pour un gain équivalent dans le cas courant.
- *Fichiers HEIC sans miniature intégrée (rares)* : conservent le
  comportement actuel (décodage pleine résolution puis réduction) ; couverts
  par le budget de threads (B2) qui limite combien de ces décodages coûteux
  tournent en même temps.

## M1 — Mesurer le pic de mémoire dans le banc d'essai

**Decision**: `picazoo-bench --items N` (déjà présent) échantillonne
`processPeakWorkingSetBytes()` (B1) avant d'ouvrir la bibliothèque, puis
l'affiche comme nouveau `Result` (« Pic de mémoire pendant le scan », en Mo,
seuil 1500) une fois `waitForIdle` terminé — `PeakWorkingSetSize` capture déjà
le maximum atteint depuis le départ du processus, donc un seul relevé final
suffit, pas besoin d'échantillonnage périodique dans le banc d'essai.

**Rationale**: répond directement à FR-008 (moyen de mesurer et de détecter
une régression) en réutilisant le scénario SC-001 déjà instrumenté
(`benchScan`), sans scénario de banc d'essai séparé. Permet de vérifier
SC-001/SC-002 (pic ≤ 1,5 Go), SC-003 (temps total, déjà mesuré par
`report("Analyse complète avec vignettes…")`) et de comparer avant/après ce
changement sur la même machine.

**Alternatives considered**:
- *Outil externe (Gestionnaire des tâches, Resource Monitor) uniquement* :
  reste la référence pour la validation manuelle finale (clarification de
  session), mais ne permet pas de vérifier automatiquement une régression
  future ni de chiffrer précisément l'avant/après pendant le développement.
- *Instrumenter `Library` elle-même pour exposer son pic de mémoire* : le pic
  mesuré doit inclure Qt, les threads de décodage et tout le processus, pas
  seulement les structures internes de `Library` ; `GetProcessMemoryInfo` au
  niveau du banc d'essai capture le bon périmètre sans instrumenter le cœur.

# Contrat du cœur : budget mémoire, HEIC, banc d'essai

Signatures indicatives (C++20, `namespace picazoo`) ; les noms définitifs
peuvent bouger à l'implémentation, les garanties non.

## `platform/win/MemoryInfo` (research B1)

```cpp
namespace picazoo::platform {

struct MemorySnapshot {
    qint64 totalPhysicalBytes = 0;
    qint64 availablePhysicalBytes = 0;
};

// GlobalMemoryStatusEx. Jamais nul : en cas d'échec improbable de l'appel
// Windows, renvoie des valeurs optimistes (comportement actuel conservé,
// pas de scan bridé par erreur).
MemorySnapshot memorySnapshot();

// GetProcessMemoryInfo (PeakWorkingSetSize) : même grandeur que la colonne
// « Mémoire » du Gestionnaire des tâches au pic observé depuis le lancement
// du processus (clarification de session, 2026-11-25).
qint64 processPeakWorkingSetBytes();

// GetProcessMemoryInfo (WorkingSetSize) : valeur instantanée (SC-005, retour
// au repos après le scan).
qint64 processWorkingSetBytes();

} // namespace picazoo::platform
```

## `jobs/MemoryBudget` (research B2, B3) — logique pure

```cpp
namespace picazoo {

struct MemoryBudgetInputs {
    qint64 totalPhysicalBytes = 0;
    qint64 availablePhysicalBytes = 0;
    int recommendedCpuThreads = 1; // CpuInfo::recommendedWorkerThreads()
};

struct MemoryBudget {
    int thumbnailThreads = 1;                    // >= 1, <= recommendedCpuThreads
    qint64 sqliteCacheBytesPerConnection = 0;     // PRAGMA cache_size
    qint64 thumbnailLruCacheBytes = 0;            // plafond ThumbnailImageProvider
};

// Fonction pure : aucun appel Windows, aucune E/S. Les bornes et les
// fractions exactes sont un détail d'implémentation calé sur les seuils de
// la spec (SC-001, SC-002, SC-006) et affiné par le banc d'essai (M1).
MemoryBudget computeMemoryBudget(const MemoryBudgetInputs& inputs);

} // namespace picazoo
```

Garanties (testées dans `tests/unit/tst_memorybudget.cpp`) :

1. `thumbnailThreads >= 1` pour toute entrée, y compris une RAM disponible
   très faible ou nulle.
2. `thumbnailThreads <= recommendedCpuThreads` toujours.
3. Pour une RAM disponible « généreuse » (machine à nettement plus de 8 Go),
   `thumbnailThreads == recommendedCpuThreads` et les caches ne sont pas
   réduits en dessous de ce qu'ils valent aujourd'hui (pas de régression sur
   une machine confortable, SC-006).
4. Pour une RAM disponible compatible avec la machine de référence (8 Go
   total, quelques Go disponibles), les caches et le nombre de threads
   combinés restent sous un budget cohérent avec 1,2-1,5 Go de pic pour un
   scan HEIC/JPEG/RAW mixte (le calcul exact de ce budget théorique est
   documenté en commentaire à côté de `computeMemoryBudget`, avec le détail
   des hypothèses de coût par décodage utilisées).
5. `computeMemoryBudget` est monotone : une RAM disponible plus faible ne
   produit jamais un budget plus généreux (threads, caches) qu'une RAM
   disponible plus élevée, à RAM totale égale.

## `ThumbnailPipeline` — réévaluation périodique (research B3)

```cpp
class ThumbnailPipeline {
public:
    // ... API existante inchangée ...

    // Nouveau : le budget est fourni à la construction (calculé par Library::open)
    // et peut être mis à jour en cours de route (RAM disponible relue).
    void applyMemoryBudget(const MemoryBudget& budget);
};
```

Garanties (testées dans `tests/integration/tst_thumbnailpipeline.cpp`,
complété, ou test dédié) :

1. `applyMemoryBudget` avec un `thumbnailThreads` plus bas que le nombre de
   fils actuellement démarrés réduit progressivement le travail en parallèle
   (les fils en excès ne redemandent plus de nouvel élément) sans perdre ni
   dupliquer aucun élément en file (`PriorityQueue` inchangée).
2. `applyMemoryBudget` avec un `thumbnailThreads` plus haut que le nombre
   actuel n'ajoute pas de nouveaux fils tant que le scan en cours n'est pas
   terminé (évite les oscillations, research B3) ; le nombre plus élevé
   s'applique au prochain `start()`.
3. Le scan continue de se terminer (tous les éléments atteignent un état
   terminal) quel que soit le nombre de fils actifs, y compris 1.

## `index/Database` — `cache_size` paramétrable (research B4)

```cpp
class Database {
public:
    // cacheBytes <= 0 : comportement actuel (32 Mo). Les appels existants
    // (tests, outils) qui ne passent pas ce paramètre ne changent pas de
    // comportement.
    static std::unique_ptr<Database> open(const QString& path, bool readOnly = false,
                                           qint64 cacheBytes = 0);
};
```

## `index/ThumbStore` — `cache_size` paramétrable (research B4)

```cpp
class ThumbStore {
public:
    explicit ThumbStore(const QString& path, qint64 cacheBytesPerConnection = 0);
};
```

## `ThumbnailImageProvider` — plafond de cache paramétrable (research B4)

```cpp
class ThumbnailImageProvider : public QQuickAsyncImageProvider {
public:
    // maxCostBytes <= 0 : comportement actuel (256 Mo).
    explicit ThumbnailImageProvider(picazoo::Library& library, qint64 maxCostBytes = 0);
};
```

## `media/HeifDecoder` — miniature comme résultat final (research H1)

Pas de changement de signature : `HeifDecoder::decode` et
`HeifDecoder::thumbnail` existent déjà avec les bonnes capacités. Le
changement est dans l'appelant, `ThumbnailPipeline::process()` : l'étape 1
(aujourd'hui `if (item->format == Format::Jpeg && !md.embeddedThumb.isEmpty())
... else if (item->format == Format::Heic) { embedded =
Decoder::embeddedThumbnail(path, Format::Heic); }`) produit déjà `embedded`
pour HEIC ; le changement est que le test `goodEnough` qui suit s'applique
identiquement à HEIC (il s'applique déjà dans le code actuel — à vérifier
précisément à l'implémentation si un cas particulier empêche aujourd'hui HEIC
d'atteindre `ThumbState::Done` directement, et le corriger si c'est le cas).

Garanties (testées dans `tests/unit/tst_decoding.cpp`, complété — `QSKIP` si
`tests/fixtures/sample.heic` absent, comme les autres formats réels) :

1. Un fichier HEIC avec une miniature intégrée d'au moins `kThumbSize - 20`
   produit une vignette `ThumbState::Done` sans que `HeifDecoder::decode`
   (pleine résolution) ait été appelé pour cet élément (vérifiable par un
   compteur d'appels injecté en test, ou par le temps d'exécution si un
   fichier de test volontairement très grand est utilisé).
2. Un fichier HEIC sans miniature intégrée, ou avec une miniature trop
   petite, continue de produire une vignette finale via
   `HeifDecoder::decode` (comportement actuel conservé).

## `picazoo-bench` — mesure du pic de mémoire (research M1)

```cpp
// Dans benchScan(), autour du bloc qui ouvre la Library et attend waitForIdle :
const qint64 peakBytes = platform::processPeakWorkingSetBytes();
report("Pic de mémoire pendant le scan", peakBytes / (1024.0 * 1024.0), "Mo", 1500);
```

Pas de nouveau test automatisé : le seuil est vérifié à l'exécution manuelle
du banc d'essai sur la machine de référence (quickstart.md), comme les
seuils de temps existants.

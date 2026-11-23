# Contrat du cœur — 006

Complète le [contrat de la 002](../../002-detection-doublons/contracts/core-api.md).
Tout ce qui n'est pas cité ici est inchangé.

## Types (`duplicates/DuplicateTypes.h`)

```cpp
struct GroupQuery {
    LevelFilter level = LevelFilter::All;
    QString folder;                       // vide = pas de filtre ; sinon copie dans ce dossier ou dessous
    QString pairA, pairB;                 // les deux non vides = copie dans A et copie dans B
    enum class Type { Any, Photos, Videos, RawJpeg } type = Type::Any;
    enum class State { Any, Untouched, Modified, Flagged } state = State::Any;
    qint64 minBytes = 0;                  // 0 = pas de filtre
    qint64 dateFrom = 0, dateTo = 0;      // epoch ms, 0 = ouvert ; date de la copie gardée
    enum class Sort { Reclaimable, Copies, Date, Folder } sort = Sort::Reclaimable;
};

enum class RuleKind { Prefer, Avoid };
struct FolderRule { qint64 id; QString path; RuleKind kind; qint64 createdAt; bool found; int groups; };

enum class BatchKind { Reset, KeepFolder, KeepAll, NotDuplicate };
struct BatchResult { qint64 batchId; int changed; int skipped; int flagged; QString message; };
struct BatchInfo { qint64 id; BatchKind kind; QString label; int groups; qint64 createdAt; bool undone; };
struct UndoResult { int restored; int notRestored; };

struct SelectionTotals { int groups; int files; qint64 bytes; };
struct FolderPair { QString a, b; int groups; qint64 bytes; int subPairs; };
struct FolderStat { QString folder; int groups; qint64 bytes; };      // doublons internes
struct SetAsideEntry { QString kind; QList<ItemId> items; qint64 decidedAt; GroupId group; };

// DuplicateGroup gagne : suggested (ItemId), suggestionReason, suggestionDetail, flags.
// DuplicateCopy gagne : isSuggested.
```

## `FolderRules` (pur, sans base)

- `FolderRules(QList<FolderRule>)` ; `const FolderRule* match(const QString& folderPath) const` :
  règle du plus long préfixe à une frontière de segment, insensible à la casse.

## `Suggestion` (pur + application)

- `SuggestionOutcome choose(DupLevel level, ItemId center, const QList<CopyFacts>&, const FolderRules&)`
  → `{ keeper, reason (optionnelle), ruleDetail, flags }` ; voir research R2.
- `void applySuggestions(Database& write, const QList<GroupId>* only)` : met à
  jour `suggested_keep`, `suggestion_reason`, `suggestion_detail`, `flags`,
  `reclaimable_bytes`, et retire les choix d'un groupe qui n'aurait plus de
  copie gardée. À appeler dans une transaction ouverte.

## `DuplicateStore` (ajouts)

| Méthode | Garantie |
|---|---|
| `QList<GroupId> groups(const GroupQuery&) const` | groupes « à revoir » qui satisfont tous les critères, dans l'ordre demandé |
| `int count(const GroupQuery&) const` | même filtre, compte seul |
| `SelectionTotals totals(const QList<GroupId>&) const` | fichiers effectivement « à la corbeille » et leurs octets, en une requête par tranche de 500 |
| `SelectionTotals trashTotals() const` | idem pour tous les groupes « à revoir » (barre de la 002, sans N+1) |
| `BatchResult applyBatch(BatchKind, const QList<GroupId>&, const QString& folder = {})` | une transaction, un lot, une notification ; rien n'est fait si la liste est vide ; `KeepFolder` ne touche pas un groupe sans copie dans le dossier ni un groupe protégé par R2 (compté dans `flagged`) |
| `UndoResult undoBatch(qint64 batchId)` | rétablit seulement ce qui n'a pas changé depuis (R4) ; idempotent |
| `QList<BatchInfo> batches(int limit = 50) const` | lots récents, le plus récent d'abord |
| `QList<FolderRule> folderRules() const` | avec `found` et le nombre de groupes orientés |
| `qint64 setFolderRule(const QString& path, RuleKind)` / `void removeFolderRule(qint64)` / `void relinkFolderRule(qint64, const QString& path)` | recalcule les suggestions de tous les groupes dans la même transaction ; ne touche à aucun choix fait à la main |
| `QList<FolderPair> folderPairs(int limit = 50) const` / `QList<FolderStat> internalDuplicates() const` | research R6 |
| `QList<SetAsideEntry> setAside() const` / `void restore(const QList<SetAsideEntry>&)` | « Écartés de la revue » ; `restore` supprime seulement les décisions entre les éléments de chaque entrée |

## `TrashService` (ajouts et changements)

- `TrashPlanEntry` gagne `DupLevel level` (niveau du groupe) et `bool examined`.
- Copie gardée de référence : la copie suggérée si elle est gardée, sinon le
  centre gardé, sinon la première gardée.
- `void requestStop()` : l'opération en cours s'arrête entre deux fichiers ;
  `Operation` gagne `bool stopped`. Sans opération en cours : sans effet.
- `TrashPlan filtered(const TrashPlan&, std::function<bool(const TrashPlanEntry&)>)`
  (statique) : sous-plan cohérent (octets recalculés).
- Inchangé : rien sans plan confirmé, au moins une copie reste, vérification
  juste avant d'agir, jamais d'effacement définitif, journal avant l'action.

# Contrat du cœur — 003

Ce que le module `naming/` expose à l'application, en complément du
[contrat de la 001](../../001-bibliotheque-visionneuse/contracts/core-api.md).
Les signatures sont indicatives ; les noms et les garanties sont
contractuels. Contrairement à la 002, aucune de ces classes ne lit un
fichier de l'utilisateur ni ne crée de fil dédié (voir
[research.md N3](../research.md)).

## `NamingDetector` — fonction pure, aucune E/S

```cpp
struct NamingInput { ItemId item; QString fileName; }; // sans extension
enum class RootShape { None, SeparatorDigits, Parenthesized, CopySuffix, GluedDigits };
struct DetectedRoot {
    QString normalizedKey;   // racine normalisée (casse ignorée), identifie le groupe candidat
    QString displayRoot;     // racine telle que rencontrée (casse d'origine, pour le nom suggéré)
    RootShape shape;
    bool generic;            // N2 : préfixe d'appareil ou racine trop courte
};

// Analyse un seul nom (sans lire la bibliothèque) ; std::nullopt si aucune racine reconnue (N1).
std::optional<DetectedRoot> detectRoot(const QString& fileNameWithoutExtension);

struct NamingCandidate {
    QString rootKey;
    QList<ItemId> items;
    Confidence confidence;   // regular_series | same_root | generic_root (N4)
};

// Regroupe un ensemble d'éléments par racine détectée (N1/N2/N4). Pure : aucune lecture de
// fichier, aucun accès à library.db. `includeGeneric` reflète le réglage (N2).
QList<NamingCandidate> groupByRoot(const QList<NamingInput>& items, bool includeGeneric);
```

## `NamingEngine` — appelé sur le thread d'indexation, sans fil propre

```cpp
class NamingEngine : public QObject {
public:
    // S'abonne à Library::scanFinished et Library::itemsChanged ; tout le travail passe par
    // Library::post (thread d'indexation), jamais de fil dédié (N3).
    NamingEngine(Library& library, NamingStore& store, QObject* parent = nullptr);

    void rescanAll();                      // repérage complet forcé (« Rechercher maintenant »)
    NamingStore& store();
    int fullScanCount() const;             // tests : un seul repérage complet au démarrage

signals:
    void groupsChanged();                  // après toute reconstruction ou mise à jour incrémentale
};
```

Garanties :
- Un `naming_decision(kind = rejected)` empêche durablement la
  reconstruction d'un groupe pour cette `root_key`, y compris après
  `rescanAll()` ou l'ajout d'un nouvel élément (FR-016).
- Un élément retiré manuellement d'un groupe (`naming_exclusion`) n'y est pas
  réinséré par un recalcul tant que la racine détectée ne change pas pour lui ;
  un élément ajouté manuellement (`manual = 1`) n'est jamais retiré par un
  recalcul, et un groupe confirmé garde tous ses membres.
- Aucune méthode de cette classe ne bloque le thread de l'interface (tout
  passe par `Library::post`, comme le reste de l'indexation).

## `NamingStore` — lectures et décisions pour l'interface

```cpp
struct NamingMember { ItemId item; QString detectedRoot; bool manual; };
struct NamingGroup {
    qint64 id; QString displayName; Confidence confidence; GroupState state;
    bool renamed; QList<NamingMember> members;
};
struct NamingSummary { int groupCount; };

class NamingStore {
public:
    // Thread de l'interface ; connexion de lecture, n'attend jamais le moteur.
    QList<qint64> groups(ConfidenceFilter filter = ConfidenceFilter::All) const;
    QList<NamingGroupRow> rows(ConfidenceFilter filter) const;  // liste de l'écran
    QList<NamingDecision> decisions() const;                    // annulables (FR-009)
    QList<NamingItem> searchItems(const QString& text, qint64 excludeGroup, int limit) const;
    std::optional<NamingGroup> group(qint64 groupId) const;
    std::optional<qint64> groupOf(ItemId item) const;        // mention dans la visionneuse (optionnel)
    NamingSummary summary() const;                            // entrée « Images liées » (FR-011)

    // Décisions (FR-008, FR-009) — écrites ici, effet immédiat sur les lectures ;
    // préviennent le moteur (setOnChanged). Toute correction confirme le groupe.
    void addToGroup(qint64 groupId, ItemId item);
    void removeFromGroup(qint64 groupId, ItemId item);
    qint64 mergeGroups(qint64 intoGroupId, qint64 fromGroupId);
    qint64 splitGroup(qint64 groupId, const QList<ItemId>& toSplitOff, const QString& newName);
    void rename(qint64 groupId, const QString& displayName);
    void confirm(qint64 groupId);                             // garde le groupe tel quel
    void markNotAGroup(qint64 groupId);                       // écrit naming_decision(rejected)
    void undoDecision(const QString& rootKey);                 // efface la ligne naming_decision
};
```

## Réglage

- `naming_include_generic_roots` (bool, défaut faux) : lu par
  `NamingEngine` avant chaque repérage complet ou incrémental (N2).

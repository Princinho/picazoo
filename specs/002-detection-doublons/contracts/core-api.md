# Contrat du cœur — 002

Ce que le module `duplicates/` et le service de corbeille `fileops/`
exposent à l'application, en complément du
[contrat de la 001](../../001-bibliotheque-visionneuse/contracts/core-api.md).
Les signatures sont indicatives ; les noms et les garanties sont
contractuels.

## `DuplicateEngine` — analyse en arrière-plan

```cpp
class DuplicateEngine : public QObject {
public:
    // S'abonne aux signaux de la Library (scanFinished, itemUpdated, itemsChanged).
    DuplicateEngine(Library& library, QObject* parent = nullptr);

    void setEnabled(bool);                    // duplicates_enabled (FR-033)
    bool isEnabled() const;
    void pause();                             // FR-029 : plus aucune lecture
    void resume();
    void rescanAll();                         // « Rechercher les doublons maintenant »
    void hold();                              // visionneuse active (cumulable)
    void release();
    DuplicateProgress progress() const;

signals:
    void progressChanged();                   // regroupé, au plus toutes les 250 ms
    void groupsChanged();                     // idem
};
```

## `DuplicateStore` — lectures pour l'interface

```cpp
struct DuplicateCopy {
    ItemId item; bool isCenter; bool keep; DupLevel level;
    std::optional<double> similarity; int rotation; bool available;
};
struct DuplicateGroup {
    GroupId id; DupLevel level; KeeperReason reason; GroupState state;
    qint64 reclaimableBytes; QList<DuplicateCopy> copies;
};
struct DuplicateSummary { int groups; qint64 reclaimableBytes; };

class DuplicateStore {
public:
    // Thread de l'interface ; connexion de lecture, n'attend jamais le moteur.
    QList<GroupId> groups(LevelFilter, GroupSort = GroupSort::ReclaimableDesc) const;
    std::optional<DuplicateGroup> group(GroupId) const;
    std::optional<GroupId> groupOf(ItemId) const;          // FR-015
    DuplicateSummary summary() const;                      // entrée « Doublons » (FR-010)

    // Décisions (FR-013, FR-014) — écrites par le moteur, effet immédiat sur les lectures
    void setKeep(ItemId, bool keep);
    void resetChoice(GroupId);                              // revenir à la suggestion
    void markNotDuplicates(GroupId);
    void removeFromGroup(GroupId, ItemId);
    void keepAll(GroupId);
    void undoDecision(GroupId);
};
```

## `TrashService` — mise à la corbeille, journal, annulation

```cpp
struct TrashPlanEntry { ItemId item; QString path; qint64 bytes; QString keeperPath; };
struct Blocked       { ItemId item; QString path; BlockReason reason; };   // no_recycle_bin, unavailable, locked
struct TrashPlan {
    QList<TrashPlanEntry> toTrash; QList<Blocked> blocked;
    QList<GroupId> refused;      // groupes dont aucune copie ne resterait (FR-017)
    qint64 bytes;
};

class TrashService : public QObject {
public:
    // Calcule le récapitulatif ; ne touche à rien (FR-016).
    TrashPlan plan(const QList<GroupId>& groups) const;
    TrashPlan planAllIdentical() const;                     // FR-022

    // Exécute un plan confirmé, sur un fil dédié. Renvoie l'identifiant de l'opération.
    OperationId execute(const TrashPlan& confirmed);

    QList<Operation> journal() const;                       // FR-020
    OperationId undo(OperationId);                          // FR-021

signals:
    void operationProgress(OperationId, int done, int total);
    void operationFinished(OperationId, OperationState);
};
```

## Garanties transverses

1. **Jamais d'effacement définitif** : `TrashService` est le **seul** code de
   Picazoo qui appelle une API de suppression ; il ne confie un fichier au
   système qu'après contrôle du volume, et refuse dans `PreDeleteItem` tout
   élément que le système n'enverrait pas à la corbeille (research D8). Un
   test d'intégration le vérifie.
2. **Rien sans plan confirmé** : `execute()` n'accepte qu'un `TrashPlan`
   produit par `plan()` ou `planAllIdentical()` ; il refait la vérification
   de FR-019 sur chaque fichier et ne touche que ceux qui la passent.
3. **Au moins une copie reste** : un plan qui écarterait toutes les copies
   d'un groupe est refusé par `plan()` **et** par `execute()`.
4. **Journal d'abord** : l'opération et ses entrées sont écrites avant le
   premier fichier traité ; chaque résultat est écrit dès qu'il est connu.
5. **Lecture seule ailleurs** : `DuplicateEngine` n'ouvre les fichiers qu'en
   lecture partagée, par le lecteur régulé (research D5), et ne lit jamais
   un fichier « disponible uniquement en ligne » (research D6).
6. **Discrétion** : le moteur ne lit un fichier que si la Library est
   inactive (indexation et vignettes), hors pause, hors batterie et hors
   visionneuse active ; son débit est plafonné et adaptatif.
7. **Reprise** : fermer l'application à tout moment ne perd que le lot en
   cours ; l'état est déduit des tables au redémarrage.
8. **Pas d'attente sur l'interface** : `DuplicateStore` répond depuis une
   connexion de lecture ; `plan()` peut lire l'index mais pas le contenu des
   fichiers ; la vérification par empreinte de `execute()` tourne sur le fil
   de l'opération.

# Implementation Plan: Gestion des doublons en lot

**Branch**: `006-gestion-doublons-lot` | **Date**: 2026-11-23 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/006-gestion-doublons-lot/spec.md`, proposition de conception [design.md](design.md) et maquettes interactives (lien dans design.md).

## Summary

Donner à l'écran « Doublons » de la 002 les moyens de traiter des centaines
de groupes d'un coup : sélection de groupes et actions en lot annulables,
règles et paires de dossiers, vue compacte et filtres combinés (dont taille et
dates), récapitulatif structuré par niveau et par dossier, arrêt d'une
opération, et vue « Écartés de la revue ». La 006 corrige aussi la portée de
« Mettre à la corbeille… », qui prenait jusqu'ici tous les groupes.

Trois idées portent le plan :

1. **Séparer le centre et la suggestion.** Le centre d'un groupe reste celui
   de la 002 (tous les membres lui ressemblent) ; la copie suggérée devient
   une colonne matérialisée (`dup_member.suggested_keep`) calculée par la
   règle de qualité de la 002 **et** les règles de dossier, avec la
   protection de la qualité tranchée en clarification (R1, R2). Tous les
   comptes restent en SQL.
2. **Une action en lot = une transaction, un lot journalisé, une
   notification.** Les décisions de la 002 restent unitaires ; les actions en
   lot passent par `DuplicateStore::applyBatch`, qui enregistre l'état d'avant
   et permet l'annulation partielle sûre (R4).
3. **Rien de plus lourd que nécessaire.** Pas de nouveau fil pour les paires
   de dossiers (une requête et un passage en mémoire, R6) ; le récapitulatif
   se prépare sur un fil de travail (accès disque, R7) ; l'arrêt d'une
   opération n'exige pas de reconstruire les tables du journal (R8).

Le détail est dans [research.md](research.md) (R1–R11).

## Technical Context

**Language/Version**: C++20 (MSVC, Visual Studio Build Tools 2026), QML (Qt Quick), comme la 001 et la 002

**Primary Dependencies**: celles de la 002, **sans ajout** : Qt 6.12 LTS (Core, Gui, Quick, QuickControls2, Dialogs), SQLite 3.

**Storage**: `library.db`, migration non destructive vers le schéma 3
(`003_doublons_lot.sql` : règles de dossier, lots de décisions, colonnes
ajoutées ; voir [data-model.md](data-model.md)). Rien dans les dossiers de
l'utilisateur.

**Testing**: Qt Test + CTest. Unitaire (règles de dossier, suggestion),
intégration sur une vraie bibliothèque de test (actions en lot et
annulation, filtres, paires, migration 2 → 3), corbeille réelle sur dossiers
temporaires (portée de la sélection, arrêt, annulation).

**Target Platform**: Windows 10 et 11 x64 ; aucun nouveau code propre à Windows.

**Project Type**: application de bureau (cœur `picazoo_core` + écrans Qt Quick dans `picazoo`).

**Performance Goals**: SC-002 (tout sélectionner + totaux < 200 ms sur
10 000 groupes, récapitulatif < 2 s), SC-003 (≥ 8 groupes par écran en vue
compacte, défilement fluide), SC-006 (arrêt < 2 s).

**Constraints**: aucune E/S disque sur le fil de l'interface pour le
récapitulatif ; une action en lot = une transaction ; aucune action sur les
fichiers sans récapitulatif ; interface en français.

**Scale/Scope**: jusqu'à 100 000 éléments et 10 000 groupes ; 1 écran revu
(Doublons : vue compacte, sélection, filtres, Écartés), 1 panneau
(Dossiers), 1 fenêtre refaite (Récapitulatif), 1 fenêtre complétée (Journal).

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principe | Vérification | Statut |
|---|---|---|
| I. Les fichiers de l'utilisateur sont sacrés | Aucune nouvelle opération sur les fichiers : la mise à la corbeille de la 002 reste la seule, toujours après récapitulatif et confirmation ; sa portée est **réduite** (sélection ou groupe courant, jamais un groupe caché, FR-015) ; « Très ressemblante » non vérifiées exclues par défaut (FR-018) ; l'arrêt laisse un journal exact et une opération annulable (R8) ; les règles de dossier ne changent que des suggestions (FR-007). | ✅ |
| II. Local et privé | Rien ne quitte la machine ; les règles sont dans `library.db`. | ✅ |
| III. Fluide sur une vraie bibliothèque | Sélection et totaux en SQL agrégé (R5) ; correction du N+1 de la 002 ; récapitulatif préparé hors du fil de l'interface (R7) ; une notification par lot au lieu d'une par groupe (R4) ; vue compacte virtualisée. | ✅ |
| IV. Le logiciel propose, l'utilisateur décide | Les règles orientent la suggestion, un choix fait à la main l'emporte (FR-008) ; la qualité est protégée (R2) ; toute action en lot est annulable (FR-022) ; la suppression de l'ancien « tout appliquer » et la portée par sélection rendent chaque lot explicite. | ✅ |
| V. Simple comme Picasa | Même écran et même fenêtre ; le panneau « Dossiers » et les filtres avancés restent cachés tant qu'on ne les ouvre pas ; textes en français. | ✅ |
| Contraintes produit | Aucun code propre à Windows ajouté. | ✅ |
| Cycle de travail | Spec en français, relue et clarifiée ; tests automatisés sur dossiers de test, corbeille réelle comprise. | ✅ |

**Re-check après la phase 1** : le modèle de données n'ajoute que des
colonnes et des tables (migration non destructive, R9) ; le contrat du cœur
garde `TrashService` comme seul code qui touche aux fichiers et lui ajoute
seulement `requestStop()` ; les actions en lot ne passent que par le
`DuplicateStore`. Aucune violation, donc rien dans Complexity Tracking.

## Project Structure

### Documentation (this feature)

```text
specs/006-gestion-doublons-lot/
├── plan.md              # ce fichier
├── design.md            # proposition de conception relue (entrée du plan)
├── research.md          # phase 0 : décisions R1–R11
├── data-model.md        # phase 1 : migration 003 de library.db
├── quickstart.md        # phase 1 : tests, mesures, validation manuelle
├── contracts/
│   ├── core-api.md      # DuplicateStore (requêtes, lots, règles, paires), TrashService
│   └── ui-contract.md   # écran Doublons, Dossiers, Récapitulatif, Écartés, clavier
├── checklists/
│   └── requirements.md
└── tasks.md             # phase 2 (/speckit-tasks)
```

### Source Code (repository root)

Ajouts à l'arborescence de la 002 (fichiers existants modifiés signalés
par « ~ ») :

```text
src/
├── core/
│   ├── duplicates/
│   │   ├── FolderRules            # R1 : correspondance chemin → règle (préfixe, casse, dossier le plus précis)
│   │   ├── Suggestion             # R2 : copie suggérée (qualité 002 + règles + protection), applySuggestions()
│   │   ├── ~DuplicateTypes        # GroupQuery, FolderRule, BatchKind, BatchInfo, FolderPair, SetAsideEntry
│   │   ├── ~DuplicateStore        # requêtes filtrées, totaux, applyBatch/undoBatch, règles, paires, écartés
│   │   └── ~DuplicateEngine       # regroup() applique les suggestions
│   ├── fileops/
│   │   └── ~TrashService          # plan avec niveau et « examiné », keeper = copie gardée suggérée, requestStop()
│   └── index/
│       ├── migrations/003_doublons_lot.sql
│       └── ~Migrator              # kLibrarySchemaVersion = 3
└── app/
    ├── ~models/DuplicateGroupModel # requête, sélection, rôles de la vue compacte
    ├── ~controllers/DuplicatesController  # filtres, actions en lot, règles, paires, écartés
    ├── ~controllers/TrashController       # récapitulatif asynchrone et structuré, arrêt
    └── qml/
        ├── ~Duplicates.qml        # en-tête, puces de filtres, sélection, barre, message « Annuler »
        ├── DuplicateGroupLine.qml # ligne compacte (dépliable)
        ├── ~DuplicateGroupRow.qml # case de sélection, raison de la suggestion
        ├── FolderPanel.qml        # paires, doublons internes, règles
        ├── SetAsideView.qml       # « Écartés de la revue »
        ├── ~TrashSummary.qml      # niveaux, dossiers dépliables, préparation
        ├── ~OperationLog.qml      # état « Arrêtée », arrêt
        └── ~FolderList.qml        # menu « Voir les doublons de ce dossier »

tests/
├── unit/~tst_duplicates_core.cpp       # FolderRules, Suggestion
├── integration/tst_duplicates_batch.cpp # lots, annulation, filtres, paires, règles, migration
└── integration/~tst_trash.cpp          # portée, examinés, arrêt
```

**Structure Decision**: on reste dans les modules de la 002 (`duplicates`,
`fileops`) et dans les écrans existants ; deux petites unités nouvelles du
cœur (`FolderRules`, `Suggestion`) sont isolées pour être testées sans base.

## Complexity Tracking

*Aucune violation de la constitution.*

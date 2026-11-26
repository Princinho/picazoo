# Implementation Plan: Images liées

**Branch**: `003-images-liees` | **Date**: 2026-11-26 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/003-images-liees/spec.md`

## Summary

Repérer, à partir des seuls noms de fichiers déjà indexés par la 001 (jamais
leur contenu), les éléments qui partagent une racine de nommage commune
(`vacances_bali_001`, `vacances_bali_002`…), les présenter en groupes avec un
nom suggéré et un niveau de confiance, et laisser l'utilisateur les
corriger (ajout, retrait, fusion, scission, renommage, exclusion) sans
jamais toucher un fichier sur le disque. Les groupes confirmés servent de
matière première à la future 004 (réorganisation).

Deux idées portent le plan :

1. **Aucune lecture de fichier, aucune lecture disque régulée.** À la
   différence de la 002, la 003 ne travaille que sur des chaînes déjà en
   mémoire (noms de fichiers indexés) : pas de `ThrottledReader`, pas
   d'`IoGovernor`, pas de pause batterie. Le calcul tient dans un passage sur
   les noms d'une racine (tri + regroupement par préfixe commun), largement
   sous la seconde même à 100 000 éléments (SC-002).
2. **Un module indépendant de la 002, qui partage son modèle d'écran.**
   `core/naming/` ajoute une table de groupes à `library.db` (migration 5),
   un détecteur pur (texte en entrée, groupes en sortie, testable sans
   bibliothèque réelle) et un petit moteur qui réagit aux mêmes signaux que
   `DuplicateEngine` (`itemsChanged`, `scanFinished`) sans aucun fil dédié :
   le calcul est assez léger pour tourner sur le thread de l'indexeur,
   via `Library::post`, entre deux tâches d'indexation.

Le détail des choix est dans [research.md](research.md) (N1–N5).

## Technical Context

**Language/Version**: C++20 (MSVC 14.51, Visual Studio Build Tools 2026), comme les 001/002/006/007

**Primary Dependencies**: celles de la 001, **sans ajout** : Qt 6.12 LTS
(Core, Gui, Quick, QuickControls2), SQLite 3. Aucune nouvelle dépendance
d'E/S régulée ni de traitement d'image : la 003 ne lit que des `QString`
déjà en mémoire.

**Storage**: `library.db`, migration non destructive vers le schéma 5
(groupes de nommage, membres, décisions ; voir [data-model.md](data-model.md)).
Rien dans `thumbs.db` ; rien dans les dossiers de l'utilisateur.

**Testing**: Qt Test + CTest ; tests unitaires purs du détecteur de racine
(texte en entrée, jeux de noms variés, aucune bibliothèque) ; tests
d'intégration sur `LibraryFixture` pour le suivi automatique (US3) et la
persistance des décisions (US4) ; `picazoo-bench` pour le temps de premier
repérage sur 100 000 éléments (SC-002).

**Target Platform**: Windows 10 et 11 x64. Aucun code propre à une
plateforme n'est nécessaire (pas d'E/S disque, pas d'API Windows) : tout vit
dans `picazoo_core`, indépendant de `platform/win/`.

**Project Type**: application de bureau (module ajouté à `picazoo_core` +
écran Qt Quick dans `picazoo`).

**Performance Goals**: premier repérage de 100 000 éléments déjà indexés en
moins d'une minute (SC-002) ; élément ajouté rejoint son groupe en moins
d'une minute (SC-004) ; aucune dégradation perceptible du défilement ou de
l'indexation pendant le repérage (SC-003).

**Constraints**: aucune lecture de fichier ; aucune E/S disque hors
`library.db` ; aucune écriture dans les dossiers de l'utilisateur
(principe I) ; interface en français ; un élément peut appartenir à la fois
à un groupe de doublons (002) et à un groupe d'images liées (003), sans
dépendance entre les deux modules.

**Scale/Scope**: jusqu'à 100 000 éléments et plusieurs milliers de groupes ;
1 écran (Images liées) et un réglage (inclure les racines génériques).

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principe | Vérification | Statut |
|---|---|---|
| I. Les fichiers de l'utilisateur sont sacrés | Aucune opération sur les fichiers dans la 003 : ni suppression, ni renommage, ni déplacement, ni écriture dans les dossiers de l'utilisateur (FR-017) ; toutes les décisions vivent dans `library.db`. Pas de corbeille, pas de journal de fichiers à concevoir. | ✅ |
| II. Local et privé | Aucun accès réseau ; travaille uniquement sur l'index déjà local de la 001. | ✅ |
| III. Fluide sur une vraie bibliothèque | Détection en O(n log n) sur les noms déjà en mémoire (tri), appelée sur le thread d'indexation entre deux tâches, jamais sur le thread de l'interface ; pas de blocage possible à 100 000 éléments (N2). | ✅ |
| IV. Le logiciel propose, l'utilisateur décide | Les groupes sont des suggestions avec un niveau de confiance en mots ; l'utilisateur ajoute, retire, fusionne, scinde, renomme ou exclut ; rien n'agit sur un fichier. | ✅ |
| V. Simple comme Picasa | Une entrée « Images liées » dans la liste des dossiers, à côté de « Doublons », sur le modèle déjà connu de l'écran de revue de la 002 ; un réglage rangé dans Outils ; textes en français. | ✅ |
| Contraintes produit | Windows d'abord, mais la 003 ne contient aucun code propre à une plateforme : portable tel quel. | ✅ |
| Cycle de travail | Spec en français, relue et clarifiée (3 choix tranchés en amont) ; aucune opération sur fichier à tester sur dossiers réels (rien n'en fait). | ✅ |

**Re-check après la phase 1** : le modèle de données (`naming_group`,
`naming_member`, `naming_decision`) ne référence que `item.id` de la 001,
sans toucher aux tables de la 002 ; le contrat du cœur expose un détecteur
pur (`NamingDetector`, aucune E/S) distinct du moteur qui l'appelle
(`NamingEngine`, lecture de l'index et écriture des groupes seulement).
Aucune violation, rien dans Complexity Tracking.

## Project Structure

### Documentation (this feature)

```text
specs/003-images-liees/
├── plan.md              # ce fichier
├── research.md          # phase 0 : décisions N1–N5
├── data-model.md         # phase 1 : migration 5 de library.db
├── quickstart.md        # phase 1 : tests, mesures, validation manuelle
├── contracts/
│   └── core-api.md      # NamingDetector, NamingEngine, NamingStore
├── checklists/
│   └── requirements.md
└── tasks.md             # phase 2 (/speckit-tasks)
```

### Source Code (repository root)

Ajouts à l'arborescence existante (fichiers modifiés signalés par « ~ ») :

```text
src/
├── core/
│   ├── naming/                     # nouveau module
│   │   ├── NamingDetector          # N1/N2 : racine commune, pur (QStringList in → groupes)
│   │   ├── NamingTypes             # structures partagées (groupe, niveau, requête)
│   │   ├── NamingStore             # table naming_group/_member/_decision, lectures et décisions
│   │   └── NamingEngine            # N3 : appelé sur le thread d'indexation, incrémental
│   ├── index/
│   │   ├── migrations/005_images_liees.sql
│   │   └── ~Migrator               # kLibrarySchemaVersion = 5
│   └── library/
│       └── ~Library                # N3 : point d'appel du moteur (noteItems/scanFinished), ~Settings (réglage racines génériques)
└── app/
    ├── models/NamingGroupModel
    ├── controllers/NamingController  # pont QML (contracts/core-api.md)
    ├── ~controllers/AppController    # création du moteur, câblage dans main.cpp
    └── qml/
        ├── RelatedImages.qml, RelatedGroupRow.qml   # écran de revue
        ├── ~FolderList.qml                          # entrée « Images liées »
        └── ~InfoPanel.qml                           # (optionnel) mention du groupe sur un élément
tests/
├── unit/                          # tst_naming_detector (pur, N1/N2), tst_naming_core (décisions)
├── integration/                   # tst_naming_engine (suivi incrémental, persistance, LibraryFixture)
└── support/                       # réutilise FixtureFactory existant (pas de nouveau générateur de fichiers)
bench/
└── ~picazoo-bench.cpp             # mesure du temps de premier repérage (SC-002)
```

**Structure Decision**: un seul nouveau module `core/naming/`, sur le
modèle de `core/duplicates/` mais sans rien de ce qui concerne la lecture
de fichiers, l'E/S régulée ou la corbeille (`fileops/`, `platform/win/`
existants ne sont pas touchés). L'application n'ajoute qu'un modèle, un
contrôleur et du QML. Le code de la 001 et de la 002 n'est modifié qu'aux
points d'intégration listés « ~ » ; aucune des deux fonctionnalités ne
dépend de l'autre au niveau du cœur.

## Risques à lever en premier (spike)

| Risque | Mesure de contrôle | Plan B |
|---|---|---|
| Règle de détection de racine (N1) trop permissive ou trop stricte sur de vrais noms de fichiers (appareils variés, renommages manuels, suffixes Windows) | SC-001 sur un jeu de noms réels variés (iPhone, Android, appareils photo, renommages « Vacances (1).jpg ») | ajuster la liste des préfixes génériques et le seuil de spécificité (longueur, présence de lettres) à partir des faux positifs/négatifs observés |
| Détection incrémentale (N3) qui recalcule plus que nécessaire sur une grosse bibliothèque | mesurer le temps passé sur `itemsChanged` avec quelques éléments ajoutés sur une bibliothèque de 100 000 | limiter le recalcul aux racines déjà connues plus une détection ponctuelle du nouvel élément seul, sans repasser sur toute la bibliothèque (voir N3) |

## Complexity Tracking

Aucune violation de la constitution à justifier.

# Implementation Plan: Visionneuse autonome

**Branch**: `005-visionneuse-autonome` | **Date**: 2026-11-22 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/005-visionneuse-autonome/spec.md`

## Summary

Un second exécutable, `picazoo-viewer.exe`, installé à côté de
`picazoo.exe`, ouvre un fichier image donné par Windows dans un voile
semi-transparent plein écran, comme la Visionneuse de photos Picasa. Il ne
charge ni la bibliothèque ni SQLite : il réutilise seulement les décodeurs
du cœur (`media/`), liste le dossier lui-même, reprend l'ordre de la fenêtre
de l'Explorateur d'où la photo a été ouverte, et anime les passages et le
zoom en Qt Quick.

La vitesse d'ouverture guide les choix : exécutable séparé et minimal, une
seule instance (les ouvertures suivantes lui sont transmises par un canal
local), décodage de la première photo lancé avant même le chargement de
l'interface, pool d'images préparées pour que la transition montre
directement une photo nette. L'inscription auprès de Windows (« Ouvrir
avec », « Applications par défaut ») est faite par l'exécutable lui-même,
appelé par l'installateur, et n'impose jamais d'association. Le détail des
choix est dans [research.md](research.md).

## Technical Context

**Language/Version**: C++20 (MSVC 14.51, Visual Studio Build Tools 2026)

**Primary Dependencies**: Qt 6.12 LTS (Core, Gui, Quick, QuickControls2,
Network pour `QLocalServer`) ; décodeurs du cœur de la 001 (libjpeg-turbo,
libheif, LibRaw, TinyEXIF) ; API Win32 et Shell (`IShellWindows`,
`IFolderView2`, `IApplicationAssociationRegistration`, registre).

**Storage**: un seul fichier `viewer.ini` dans `%LOCALAPPDATA%\Picazoo\`
(ou `PICAZOO_DATA_DIR`) ; aucune vignette, aucun cache, aucune base. Le
cache disque du QML est désactivé pour ce processus.

**Testing**: Qt Test + CTest : listage et tri d'un dossier, fusion avec
l'ordre de l'Explorateur, suivi des changements, calcul du zoom ancré,
inscription au registre (dans une clé de test), instance unique, invariant
« aucun fichier créé ni modifié » sur dossiers temporaires.

**Target Platform**: Windows 10 et 11 x64. Le code Shell et registre est
isolé dans `src/core/platform/win/` ; le reste ne dépend que de Qt.

**Project Type**: application de bureau, second exécutable de Picazoo.

**Performance Goals**: double-clic → photo de 24 Mpx affichée < 1 s
(< 2 s à froid) ; transition lancée < 0,1 s après la touche ; rafale de
20 appuis absorbée < 0,5 s ; animations à la fréquence de l'écran ; dossier
de 5 000 images parcourable < 3 s.

**Constraints**: lecture seule et partagée des fichiers de l'utilisateur ;
moins de 100 Ko écrits par session ; aucune ouverture de la bibliothèque ;
mémoire < 500 Mo ; aucune E/S ni décodage sur le thread de l'interface ;
interface en français.

**Scale/Scope**: dossiers jusqu'à 10 000 images ; 1 écran (le voile) avec
une carte de proposition d'association et un panneau d'informations ; une
petite extension de `picazoo.exe` (`--show`).

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principe | Vérification | Statut |
|---|---|---|
| I. Les fichiers de l'utilisateur sont sacrés | Ouverture en lecture avec partage lecture/écriture/suppression ; aucune API d'écriture sur le dossier consulté ; pas de vignette ni de cache ; réglages dans `%LOCALAPPDATA%\Picazoo` ; test avant/après sur dossier temporaire (SC-007). Aucune suppression, rotation ni retouche. | ✅ |
| II. Local et privé | Aucun accès réseau ; le canal d'instance unique est local (tube nommé propre à la session) ; aucune télémétrie. | ✅ |
| III. Fluide sur une vraie bibliothèque | La visionneuse n'interagit pas avec la bibliothèque ; listage et lecture de l'ordre de l'Explorateur en arrière-plan ; décodage sur un pool de threads ; dossiers de 10 000 images testés. | ✅ |
| IV. Le logiciel propose, l'utilisateur décide | Association proposée une fois, « Plus tard » et « Ne plus demander » ; c'est Windows qui confirme ; l'ajout d'un dossier à la bibliothèque passe par une question explicite. | ✅ |
| V. Simple comme Picasa | Reprend le modèle de la Visionneuse de photos Picasa ; second point d'entrée et non seconde application : mêmes décodeurs, même thème, même panneau d'informations. Les réglages se limitent à un menu « ⋯ ». | ✅ (voir note) |
| Contraintes produit | Windows d'abord, code Win32 isolé ; tous les formats d'images de la 001. Vidéos exclues par choix de l'utilisateur. | ✅ |
| Cycle de travail | Spec en français ; la 005 ne dépend que de la 001 et passe avant 002–004 par décision de l'utilisateur (feuille de route mise à jour). | ✅ |

**Note sur le principe V** : « une fenêtre » vise l'application de
bibliothèque. La visionneuse autonome est un mode d'entrée distinct, demandé
explicitement et propre au modèle Picasa ; elle n'ajoute aucune fenêtre à
la bibliothèque.

**Re-check après la phase 1** : le contrat de stockage
([contracts/storage-and-registry.md](contracts/storage-and-registry.md))
liste les seules écritures autorisées (réglages, clés d'inscription) et le
data-model garde le parcours en mémoire. Aucune violation.

## Project Structure

### Documentation (this feature)

```text
specs/005-visionneuse-autonome/
├── plan.md              # ce fichier
├── research.md          # phase 0 : décisions V1–V12
├── data-model.md        # phase 1 : parcours, photo affichée, réglages
├── quickstart.md        # phase 1 : compilation, tests, validation
├── contracts/
│   ├── ui-contract.md            # voile, commandes, raccourcis, messages
│   ├── command-line.md           # picazoo-viewer.exe et picazoo.exe --show
│   └── storage-and-registry.md   # seules écritures permises, clés Windows
├── checklists/
│   └── requirements.md
└── tasks.md             # phase 2 (/speckit-tasks)
```

### Source Code (repository root)

```text
src/
├── core/
│   ├── browse/                  # nouveau, Qt Core seulement, testable
│   │   ├── FolderListing.*      # images d'un dossier, ordre naturel, fichiers cachés écartés
│   │   ├── BrowseOrder.*        # fusion ordre de l'Explorateur / listage
│   │   ├── FolderBrowser.*      # parcours : position, boucle, suivi des changements
│   │   └── ZoomMath.*           # ajustement, zoom ancré, bornes du déplacement
│   └── platform/win/
│       ├── ExplorerOrder.*      # ordre d'affichage de la fenêtre de l'Explorateur (COM)
│       ├── FileAssociations.*   # inscription « Ouvrir avec », état des associations
│       └── SystemPrefs.*        # animations et transparence de Windows
├── viewer/                      # nouveau : picazoo-viewer.exe
│   ├── main.cpp                 # instance unique, décodage anticipé, voile
│   ├── SingleInstance.*         # mutex nommé + QLocalServer
│   ├── ViewerSession.*          # contrôleur QML : parcours, infos, actions
│   ├── FileImageProvider.*      # image://file/… (écran, pleine résolution, vignette)
│   ├── ViewerSettings.*         # viewer.ini
│   └── qml/                     # Overlay, Stage, Controls, Strip, InfoCard, AssocCard, Theme
└── app/                         # picazoo.exe : option --show, instance unique
installer/picazoo.iss            # appelle --register / --unregister
tests/unit/                      # tst_folderlisting, tst_browseorder, tst_zoommath
tests/integration/               # tst_folderbrowser, tst_associations, tst_singleinstance
```

**Structure Decision**: un troisième point d'entrée dans le même dépôt. La
logique sans interface (`browse/`) rejoint `picazoo_core` pour être testée
comme le reste ; `picazoo-viewer` ne lie que ce dont il a besoin (Qt Quick,
décodeurs), sans `Library` ni SQLite. Le QML de la visionneuse forme son
propre module (`PicazooViewer`) : la visionneuse de la 001 reste intacte.

## Risques à lever en premier (spike)

| Risque | Mesure de contrôle | Plan B |
|---|---|---|
| Fenêtre Qt Quick translucide plein écran avec Direct3D 11 (V3) | capture d'écran : bureau visible derrière le voile | moteur OpenGL pour ce seul processus, ou voile opaque |
| Ordre de l'Explorateur lu à temps (V5) | ouverture depuis une fenêtre triée par date | ordre naturel des noms, sans bloquer l'affichage |
| Temps de démarrage à froid (V2) | chronométrage double-clic → photo | réduire les modules Qt chargés, retarder la bande de vignettes |

## Complexity Tracking

Aucune violation de la constitution à justifier.

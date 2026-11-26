# Quickstart — valider la 003

Guide pour vérifier que la détection des images liées tient ses promesses.
Les choix techniques sont dans [research.md](research.md), le modèle dans
[data-model.md](data-model.md) et le comportement attendu dans
[contracts/ui-contract.md](contracts/ui-contract.md) et
[contracts/core-api.md](contracts/core-api.md).

## Prérequis

Ceux de la [001](../001-bibliotheque-visionneuse/quickstart.md) (Visual
Studio Build Tools 2026, Qt 6.12 LTS, `QT_DIR`). La 003 n'ajoute aucune
dépendance, aucune API Windows, et ne lit jamais le contenu d'un fichier :
pas de jeu de vraies photos à préparer, contrairement à la 002.

## Compiler et tester

```powershell
cmake --preset windows-release
```

```powershell
cmake --build --preset windows-release
```

```powershell
ctest --preset windows-release
```

Attendu : tous les tests passent, dont :

- `tst_naming_detector` (unitaire, pur) : jeu de noms variés (séries
  régulières, racines génériques, suffixes Windows, casse différente,
  photo + vidéo de même racine) vérifiant SC-001 (≥ 95 % des séries à
  racine significative regroupées, aucune série `IMG_*`/`DSC_*` par
  défaut) ;
- `tst_naming_engine` (intégration, `LibraryFixture`) : décisions (retrait,
  ajout, fusion, scission, exclusion, annulation), suivi incrémental
  (SC-004), survie des décisions à un redémarrage et à un renommage de
  fichier (SC-007), aucune écriture dans les dossiers (SC-006) ;
- les tests 001/002/006/007 existants, inchangés (migration non
  destructive, aucune dépendance entre `naming/` et `duplicates/`).

## Mesures de performance

```powershell
.\build\windows-release\picazoo-bench.exe --items 100000 --index-only --naming
```

| Mesure | Seuil | Critère |
|---|---|---|
| Premier repérage complet, bibliothèque déjà indexée (100 000 éléments) | < 1 min | SC-002 |
| Défilement et indexation pendant le repérage | fluide, aucune dégradation perceptible | SC-003 |
| Apparition d'un élément ajouté dans son groupe | < 1 min | SC-004 |

## Validation manuelle

Préparer un dossier de test mêlant plusieurs cas, par exemple avec
`picazoo-testdata` (ou en copiant/renommant quelques photos existantes) :

- `vacances_bali_001.jpg` à `vacances_bali_006.jpg` (série régulière)
- `Anniversaire Julie (1).jpg`, `Anniversaire Julie (2).jpg`,
  `Anniversaire Julie (3).jpg` (suffixe parenthèses)
- `IMG_0001.jpg` à `IMG_0010.jpg` (racine générique d'appareil)
- 10 photos aux noms sans rapport

Ajouter ce dossier aux dossiers surveillés et attendre la fin de
l'indexation (001).

1. **Repérage automatique (US1, US3)** : sans rien faire, attendre
   quelques secondes après la fin de l'indexation. → L'entrée « Images
   liées » apparaît dans la liste des dossiers, avec 2 groupes (« vacances
   bali » et « Anniversaire Julie »). Les `IMG_*` et les 10 photos sans
   rapport n'y figurent pas.
2. **Contenu d'un groupe (US1)** : ouvrir « Images liées ». → Le groupe
   « vacances bali » affiche ses 6 vignettes et le niveau « Série
   régulière » ; « Anniversaire Julie » affiche ses 3 vignettes.
3. **Renommer (US2)** : renommer le groupe « vacances bali » en « Bali
   2024 ». → Le nom change immédiatement ; aucun fichier sur le disque
   n'est renommé (vérifier dans l'Explorateur).
4. **Retirer puis ajouter (US2)** : retirer une photo du groupe « Bali
   2024 », puis l'ajouter à nouveau via « Ajouter des éléments… ». → Le
   groupe revient à 6 éléments ; le fichier n'a jamais bougé sur le
   disque.
5. **Fusionner (US2)** : fusionner « Anniversaire Julie » dans un groupe
   test créé manuellement, ou l'inverse. → Un seul groupe réunit les
   éléments des deux, sous le nom choisi.
6. **Scinder (US2)** : scinder « Bali 2024 » en détachant 2 des 6 éléments
   vers un nouveau groupe. → Deux groupes distincts, 4 et 2 éléments.
7. **Rejeter (US2, US3)** : marquer un groupe « Ce n'est pas un groupe »,
   puis « Rechercher maintenant » (ou ajouter une nouvelle photo de même
   racine). → Le groupe ne revient pas.
8. **Suivi automatique (US3)** : ajouter `vacances_bali_007.jpg` dans le
   dossier surveillé. → En moins d'une minute, le groupe correspondant
   (s'il n'a pas été rejeté à l'étape 7) affiche un élément de plus, sans
   que les autres groupes ne soient retouchés.
9. **Racines génériques (edge case)** : activer « Inclure les noms donnés
   par l'appareil » dans Outils, puis « Rechercher maintenant ». → Les
   `IMG_*` forment désormais un groupe, avec le niveau le plus bas
   (« Racine généralement utilisée par l'appareil »). Désactiver le
   réglage et relancer : il redisparaît.
10. **Persistance (US4)** : fermer et relancer Picazoo. → Les noms
    choisis, les fusions/scissions et le groupe rejeté sont toujours là.
    Renommer un des fichiers du groupe « Bali 2024 » dans l'Explorateur
    (sans toucher à Picazoo). → Après réindexation, le groupe affiche
    toujours le même nom et le même nombre d'éléments.
11. **Indépendance de la 002** : si une bibliothèque de test des doublons
    (voir [quickstart de la 002](../002-detection-doublons/quickstart.md))
    est disponible, vérifier qu'un élément présent à la fois dans un
    groupe de doublons et un groupe d'images liées s'affiche normalement
    dans les deux écrans, sans erreur ni double comptage.

## Captures pour le développement

`PICAZOO_DATA_DIR` (bibliothèque séparée), `PICAZOO_DEV_SCREEN=naming`
(ouvre l'écran « Images liées » au démarrage), `PICAZOO_SCREENSHOT=<fichier.png>;<délai ms>`
(capture puis quitte) et `PICAZOO_LOG_FILE=<fichier>` (messages de Qt et du
QML) permettent de vérifier l'interface sans manipulation, comme pour les
fonctionnalités précédentes.

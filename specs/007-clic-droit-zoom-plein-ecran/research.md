# Research: Ergonomie des visionneuses et clic droit

**Feature**: [spec.md](spec.md) · **Date**: 2026-11-24

État des lieux relevé dans le code avant de décider :

- Le zoom existe en **deux copies presque identiques** : `src/app/qml/Viewer.qml`
  (visionneuse intégrée, propriétés `zoom`, `ox`, `oy`) et
  `src/viewer/qml/Stage.qml` (visionneuse autonome, une copie par emplacement
  d'image, propriétés `s`, `ox`, `oy`). Les deux appellent les fonctions pures
  de `core/browse/ZoomMath` (via `ViewerController` et `ViewerSession`) et
  animent l'échelle avec un `NumberAnimation` de 120 ms (`Easing.OutQuad`).
  Molette : `kWheelStep = 1.25` par cran ; maximum `kMaxScale = 16`.
- Le plein écran de la visionneuse intégrée masque en-tête, pied et panneau
  d'informations (`visible: !root.fullScreen`) ; rien ne le remplace.
- Clic droit existant : `ThumbCell` → `ThumbGrid.contextMenu`, `FolderList`,
  `DuplicateGroupRow` (copie, groupe). Rien dans les visionneuses ni dans la
  zone vide de la grille.
- La seule mise à la corbeille est `TrashService` (002/006), bâtie autour des
  groupes de doublons (copie gardée, revérification d'empreinte), avec
  `file_operation` / `file_operation_entry` (le `item_id` et le
  `keeper_path` d'une entrée acceptent déjà NULL). `RecycleBin` est le seul
  code qui retire un fichier.
- La visionneuse autonome n'ouvre pas `library.db` ; elle connaît le dossier
  de données (`ViewerSettings`, `PICAZOO_DATA_DIR`). `Database` ouvre en WAL
  avec `busy_timeout` de 5 s : deux processus peuvent écrire.

---

## Z1 — Un seul moteur de zoom, dans le cœur, pour les deux visionneuses

**Decision**: créer `core/browse/ZoomMotion` : une classe C++ **pure** (sans
QObject, temps passé en paramètre) qui tient l'échelle, le décalage, la cible,
l'état du geste en cours et l'élan, et qui avance d'un pas de temps
(`advance(dtMs)`). Un mince `QObject` (`ZoomController`, même dossier) l'expose
en QML avec ses propriétés `scale`, `offsetX`, `offsetY`, `percent`,
`fitMode`, `moving`, et il est enregistré comme type QML dans les deux
exécutables. Chaque visionneuse remplace sa logique `zoomTo` / `onZoomChanged`
/ `NumberAnimation` par une instance (une par emplacement dans la visionneuse
autonome) animée par une `FrameAnimation` qui appelle `advance(frameTime)`
tant que `moving` est vrai. L'ancrage reste celui de `ZoomMath`
(`anchorOnImage`, `zoomEndOffset`, `zoomStepOffset`), recalculé à chaque
changement de cible.

**Rationale**: la butée (FR-010 à FR-016) et l'inertie (FR-040 à FR-047)
sont des règles d'état et de temps délicates ; les écrire deux fois en
JavaScript QML garantirait des écarts entre les visionneuses. En C++ pur,
elles se testent en unitaire sans fenêtre (les `Timer` et animations QML ne
tournent pas quand Claude lance l'application : fenêtre jamais exposée). Les
deux visionneuses gagnent un comportement identique et perdent ~80 lignes de
QML chacune.

**Alternatives considered**:
- *Composant QML partagé* : les deux exécutables ont des modules QML séparés
  (déjà deux `Theme.qml`) ; un module commun ajouterait une plomberie CMake
  et laisserait la logique intestable hors fenêtre.
- *Garder `NumberAnimation` et allonger sa durée* : ne donne ni l'élan ni la
  continuité quand des crans arrivent pendant l'animation (aujourd'hui chaque
  cran redémarre l'animation : c'est une partie de la sensation sèche).
- *`QVariantAnimation` côté C++* : le cadencement par image de la scène
  (`FrameAnimation`) est synchronisé avec l'affichage, `QTimer` ne l'est pas.

## Z2 — Modèle de mouvement : poursuite exponentielle de la cible (en logarithme)

**Decision**: l'échelle affichée poursuit une **cible** avec une loi du
premier ordre sur le logarithme de l'échelle :
`log s ← log s + (log cible − log s) · (1 − e^(−dt/τ))`, avec **τ = 75 ms**.
Quand l'écart tombe sous 0,05 % (ou sous un demi-pixel de taille affichée),
l'échelle prend exactement la valeur de la cible et `moving` passe à faux.

- Un pas (cran, flèche, Ctrl +, bouton) multiplie la **cible** par 1,25
  (inchangé) ; l'affichage la rejoint à 98 % en ≈ 4τ = 0,3 s (FR-040 :
  0,25 à 0,35 s), avec un départ franc et une arrivée douce, **sans jamais
  dépasser** la cible (loi du premier ordre : pas de rebond, FR-043).
- L'animation se fait en logarithme pour qu'un pas de 50 % → 62 % dure
  autant qu'un pas de 400 % → 500 % (perception proportionnelle).

**Rationale**: la Visionneuse de photos Picasa donne cette sensation de zoom
qui « suit » la molette et termine en glissant ; une poursuite exponentielle
de la cible la reproduit simplement, s'enchaîne sans à-coup quand des crans
arrivent en cours de route (la cible bouge, la vitesse reste continue), et ne
peut pas dépasser une butée, puisque la cible elle-même est bornée.

**Alternatives considered**:
- *Ressort amorti critique* : plus « physique », mais il dépasse sa cible
  dès qu'il part avec une vitesse vers elle (crans rapprochés), ce qui
  contredit FR-014 et FR-043.
- *Courbe de durée fixe par pas (`OutCubic` 300 ms)* : redémarrer une courbe
  à chaque cran crée une cassure de vitesse visible.

**Ajustement à l'implémentation** : une loi purement exponentielle met trop
longtemps à finir après un geste rapide (la cible peut être loin devant :
≈ 75 ms × ln(écart / seuil) dépasse 600 ms). Le moteur ajoute donc une
**vitesse plancher** de 0,3 (logarithme d'échelle par seconde) : le mouvement
suit l'exponentielle tant qu'elle est plus rapide, puis finit à vitesse
constante et s'arrête exactement sur la cible. Avec **τ = 90 ms**, un pas
isolé atteint 98 % de son chemin en ≈ 265 ms et finit en ≈ 280 ms ; l'élan le
plus long (avance de la cible plafonnée à e^2,5) s'éteint en ≈ 480 ms. Les
tests `tst_zoommotion` vérifient ces bornes.

Les valeurs τ, vitesse plancher et seuils du gain sont des **valeurs de départ** ;
elles sont calées à l'œil contre une vidéo de la Visionneuse de photos
Picasa à l'implémentation (spec, Assumptions) et restent des constantes de
`ZoomMotion.h`, pas un réglage.

## Z3 — L'élan : accélération des crans rapprochés

**Decision**: l'élan (FR-041) vient de deux effets combinés :

1. **Le retard naturel de la poursuite** (Z2) : au dernier cran, la cible est
   devant l'affichage, qui continue donc à zoomer en ralentissant ; il
   s'immobilise en ≈ 0,3 s (< 0,6 s, FR-041).
2. **Une accélération selon la cadence** : la cadence des crans est suivie
   par une moyenne glissante (crans par seconde, demi-vie 100 ms). Un cran
   multiplie la cible par `1,25^g`, avec `g = 1` jusqu'à 8 crans/s, puis
   croissant linéairement jusqu'à `g = 2` à 20 crans/s (plafond). Un tour
   rapide va donc plus loin qu'un tour lent du même nombre de crans ; un cran
   isolé reste un pas de 1,25 exactement (FR-041, dernière phrase).

Pour le pavé tactile (deltas non multiples de 120), la cadence se calcule en
« crans équivalents » (`angleDelta / 120`), la même formule s'applique.
Le clavier, les boutons, le double-clic, « Ajuster » et « 100 % » utilisent
la poursuite (Z2) mais jamais l'accélération (FR-044). Le pincement suit les
doigts directement, sans poursuite ni élan (geste direct : un retard serait
ressenti comme du flottement).

**Rationale**: c'est la façon la plus simple de rendre « plus le geste était
rapide, plus l'élan va loin » tout en gardant la cible — donc la butée et les
limites — exactement connue à tout instant.

**Alternatives considered**: *vraie vitesse avec décroissance après le
dernier cran (style défilement tactile)* : l'élan dépendrait d'une fin de geste
qu'on ne peut détecter qu'après la pause de 0,3 s (Z4), ce qui retarderait la
glissade ; et la cible finale ne serait connue qu'à l'arrêt, ce qui complique
la butée.

## Z4 — Le geste et la butée de 100 %

**Decision**: `ZoomMotion` tient un **geste en cours** : `{source, sens,
échelle de départ, plafond, dernier instant}`.

- Un nouveau geste commence quand : la source change (molette / clavier /
  bouton / pincement) ; le sens s'inverse ; ou, pour la molette, plus de
  **300 ms** se sont écoulées depuis le cran précédent (FR-013) ; pour le
  clavier, à chaque appui qui n'est pas une répétition automatique
  (`event.isAutoRepeat`) ; pour un bouton, à chaque clic ; pour le pincement,
  à chaque `PinchHandler.active` qui passe à vrai.
- Au début d'un geste **de zoom avant**, l'échelle de départ est l'échelle
  **affichée** (pas la cible). Si elle est **< 0,9**, le geste reçoit un
  plafond de **1,0** ; sinon aucun plafond (FR-010, FR-012).
- Toute cible calculée pendant le geste est bornée par
  `min(plafond, kMaxScale)` puis par l'ajustement (`clampScale`). Les crans
  qui arrivent une fois la cible au plafond sont **sans effet** et ne
  s'accumulent pas (FR-011) — la cible est un nombre, pas un compteur de
  crans.
- Le zoom arrière n'a jamais de plafond (FR-015) ; les commandes absolues
  (« Ajuster », « 100 % », touches 0 / 1, double-clic) passent par
  `setTarget()` et terminent le geste en cours.
- Si l'ajustement à la fenêtre est déjà ≥ 0,9 (petite image), le geste part
  de ≥ 0,9 et n'a donc pas de plafond (spec, US2 scénario 7) ; l'ajustement
  n'agrandit jamais au-delà de 1 (`fitScale`), donc l'échelle de départ ne
  dépasse jamais 1 à l'ajustement.
- Le seuil 0,9 et la butée 1,0 portent sur la même échelle que l'indicateur
  de zoom actuel (1 = 100 % affiché) ; la sémantique du 100 % n'est pas
  modifiée par cette fonctionnalité.

**Rationale**: traduit mot pour mot la règle demandée ; la répétition
automatique du clavier est un seul geste (spec, Edge Cases), ce qui donne
« maintenir ↑ s'arrête à 100 %, relâcher et réappuyer va au-delà ».

**Alternatives considered**: *butée « collante » à franchir en insistant
(N crans)* : non demandé, moins prévisible ; *butée aussi au zoom arrière* :
exclue par la spec.

## Z5 — Interruption de l'élan, changement de photo, taille inconnue

**Decision**:
- `stop()` fige la cible sur l'échelle affichée et termine le geste ; appelé
  au début d'un glisser (`DragHandler.active`), d'un clic, d'un pincement, au
  changement de photo, et par un cran en sens inverse avant d'appliquer ce
  cran (FR-042).
- Changement de photo : `reset(fit)` (ajustement immédiat, comme aujourd'hui).
- Taille réelle inconnue (aperçu RAW, image en cours de chargement) : la
  butée se calcule sur la taille connue la plus fiable (`fullSize` puis
  taille de l'aperçu) ; quand la taille réelle arrive, `setImageSize()`
  recalcule l'échelle affichée **à taille affichée constante** (comme
  `onFullWChanged` aujourd'hui) et, si le geste en cours avait un plafond, il
  est réexprimé dans la nouvelle échelle pour rester « 100 % de la photo ».
- Animations désactivées dans Windows (`animations` déjà exposé par
  `ViewerController` et `ViewerSession`) : `ZoomMotion` est réglé en mode
  immédiat (τ = 0, pas d'accélération), FR-045.

## Z6 — Flèches du clavier

**Decision**: dans `Viewer.qml` et `Overlay.qml`, ↑ / ↓ appellent
`zoom.step(+1 / −1, source = clavier, autoRepeat)` autour du centre de la
vue ; ← / → gardent leur code actuel (`goPrevious` / `goNext`,
`session.previous` / `session.next`). Sur une vidéo (`isVideo`), ↑ / ↓ sont
ignorées. Les menus (`Menu` de Qt Quick Controls) prennent le focus clavier
quand ils sont ouverts, donc les flèches leur reviennent sans code
particulier (FR-019) ; on le vérifie dans le quickstart. Dans la grille de
la bibliothèque, rien ne change.

## M1 — Menus contextuels : composants QML par écran, actions dans les contrôleurs

**Decision**:
- Un composant `ItemContextMenu.qml` dans l'application (vignette ou
  sélection) et un `ImageContextMenu.qml` dans chacune des deux
  visionneuses, construits avec `Menu` / `Action` de Qt Quick Controls et le
  thème existant (comme les menus des doublons). Chaque `Action` affiche son
  raccourci (`shortcut` en texte, FR-067) ; une entrée sans objet est
  `enabled: false` (grisée, jamais cachée).
- Le clic droit est capté par un `TapHandler { acceptedButtons:
  Qt.RightButton }` posé sur la scène des visionneuses ; il ne déclenche ni
  zoom ni déplacement (les `DragHandler`, `TapHandler` gauches et le
  double-clic n'acceptent que le bouton gauche, déjà le cas) et appelle
  `zoom.stop()` (FR-069, FR-042).
- Zone vide de la grille : `TapHandler` droit sur le fond de `ThumbGrid`,
  sous les vignettes (les vignettes l'interceptent déjà par leur
  `MouseArea`).
- Touche Menu (`Qt.Key_Menu`) et Maj+F10 : traitées dans les
  `Keys.onPressed` existants de `ThumbGrid`, `Viewer.qml` et `Overlay.qml` ;
  le menu est ouvert près de l'élément courant (vignette courante ou centre
  de l'image).
- Échap avec un menu ouvert : le `Menu` consomme Échap et se ferme ; la
  visionneuse autonome a déjà `closePopups()` ; dans `Viewer.qml`, on vérifie
  qu'aucun menu n'est ouvert avant de fermer (FR-069, spec US5 scénario 5).

**Rationale**: suit les menus existants (mêmes composants, même thème) ;
les actions elles-mêmes vivent en C++ (contrôleurs) pour être partagées et
testées.

**Alternatives considered**: *menus natifs Windows (`popupType:
Popup.Native`)* : plus « Windows », mais incohérents avec les menus sombres
déjà présents dans l'application et la visionneuse autonome ; à revoir
globalement plus tard si souhaité.

## M2 — Copier, copier le chemin

**Decision**: `platform::copyToClipboard(paths, withImage)` dans un nouveau
`core/platform/win/ClipboardActions` :
- `QMimeData` avec les URL des fichiers (Qt les convertit en `CF_HDROP`, ce
  que l'Explorateur colle comme copie) et le format
  `Preferred DropEffect = DROPEFFECT_COPY`, pour que coller dans un dossier
  **copie** et ne déplace jamais (principe I) ;
- pour une seule photo, en plus, l'image décodée (aperçu « écran » déjà
  disponible, ou décodage pleine résolution plafonné à 8 000 px de côté)
  via `setImageData`, pour coller dans un document ou une messagerie ;
- vidéo ou sélection multiple : fichiers seulement.
- « Copier le chemin » : texte, un chemin Windows complet par ligne.

Le décodage pour le presse-papiers se fait hors du fil de l'interface ; le
menu se ferme aussitôt.

## T1 — Mettre à la corbeille hors doublons : élargir `TrashService`, pas le dupliquer

**Decision**:
- Ajouter à `TrashService` `planItems(QList<ItemId>)` : un plan dont les
  entrées n'ont **pas de copie gardée** (`keeper = 0`). `runTrash` traite ces
  entrées avec la même revérification d'identité (fichier toujours là,
  inchangé depuis l'indexation) mais sans la comparaison d'empreinte avec une
  copie gardée. Les gardes de la 002 (« au moins une copie de chaque groupe
  reste ») ne s'appliquent qu'aux entrées avec groupe.
- Le journal distingue l'origine : nouvelle colonne `file_operation.source`
  (`duplicates` | `library` | `viewer`) ; `TrashController` adapte le titre
  (« 3 photos mises à la corbeille depuis la bibliothèque »).
- Après la mise à la corbeille, `Library::forgetItems` retire les éléments
  (comme aujourd'hui) ; la grille et la visionneuse intégrée suivent par les
  signaux existants. Annuler (`TrashService::undo`) restaure puis relance une
  analyse : les éléments reviennent.
- Disque sans corbeille : `platform::volumeHasRecycleBin` est interrogé
  **à l'ouverture du menu** sur un fil de travail (résultat mis en cache par
  volume) pour griser l'entrée avec son info-bulle (FR-072) ; le plan le
  revérifie de toute façon (`BlockedEntry`).

## T2 — Mettre à la corbeille depuis la visionneuse autonome

**Decision**: extraire de `TrashService` la partie « journal + corbeille
d'une liste de chemins » dans `core/fileops/FileTrash` (sans `Library` ni
`DuplicateEngine`) : `trashPaths(Database&, paths, source)` et
`restore(Database&, operationId)`. `TrashService` l'utilise pour son
exécution ; la visionneuse autonome l'utilise directement.
- La visionneuse ouvre `library.db` **seulement au moment de mettre à la
  corbeille** (pas au démarrage : SC de la 005 sur le temps d'ouverture
  intact), via `Database` + `Migrator` (crée la base si Picazoo n'a jamais
  été ouvert). WAL et `busy_timeout` permettent l'écriture pendant que
  l'application tourne.
- Si l'application est ouverte et que le fichier est dans un dossier
  surveillé, l'observateur de dossiers (`Watcher`) voit la disparition puis
  la restauration : rien de plus à faire.
- `TrashService::recoverInterrupted` ne reprend que les opérations
  `planned` / `running` **créées depuis plus de 60 s**, pour ne pas
  « réparer » une opération qu'une visionneuse est en train de mener.
- Le message « Annuler » de la visionneuse appelle `FileTrash::restore` ;
  plus tard, l'annulation se fait depuis le journal de Picazoo.

**Alternatives considered**: *envoyer la demande à l'application par le
canal d'instance unique* : oblige l'application à tourner ; *sans journal* :
contraire au principe I.

## W1 — Définir comme fond d'écran, et annuler

**Decision**: `core/platform/win/Wallpaper` avec l'interface COM
`IDesktopWallpaper` (Windows 8 et plus) :
- `snapshot()` : pour chaque écran, `GetWallpaper` (chemin source), plus
  `GetPosition` ; si un diaporama est actif (`GetStatus`), le signaler.
- `set(path)` : `SetWallpaper(nullptr, path)` (tous les écrans), position
  inchangée (FR-074). Windows en fait sa propre copie transcodée ; aucun
  fichier de l'utilisateur n'est copié ni modifié.
- `restore(snapshot)` : `SetWallpaper` écran par écran avec les chemins
  d'avant. Si un ancien chemin n'existe plus, ou si un diaporama était actif
  (impossible à restaurer proprement), le message « Annuler » est remplacé
  par une phrase qui le dit (pas de faux « Annuler »).
- Formats proposés : JPEG (`.jpg`, `.jpeg`, `.jfif`), PNG, BMP, GIF, TIFF ;
  les autres (RAW, HEIC, WebP, vidéos) grisés avec info-bulle (spec,
  Assumptions). Une seule photo à la fois.
- Le cliché d'annulation vit en mémoire, le temps du message.

**Alternatives considered**: `SystemParametersInfo(SPI_SETDESKWALLPAPER)` :
un seul fond pour tous les écrans et pas d'information fiable pour revenir
en arrière (renvoie le fichier transcodé, écrasé par le nouveau fond).

## P1 — Propriétés de Windows

**Decision**: `platform::showProperties(paths)` dans `ShellActions` : un
fichier → `SHObjectProperties(hwnd, SHOP_FILEPATH, path)` ; plusieurs →
`SHCreateShellItemArrayFromIDLists` puis `BindToHandler(BHID_DataObject)`
et `SHMultiFileProperties` (fonctionne aussi pour des fichiers de dossiers
différents). La fenêtre est celle de Windows : ce que l'utilisateur y fait
lui appartient.

## F1 — Sortie du plein écran à la souris

**Decision**: dans `Viewer.qml`, un bouton flottant (même style que les
boutons ronds précédent / suivant, fond semi-opaque `#D9121418` et bordure,
lisible sur tout fond) en haut à droite, visible seulement en plein écran,
affiché par un `HoverHandler` (mouvement du pointeur) et masqué après 2 s
d'immobilité par un `Timer`, sauf s'il est survolé ; fondu de 250 ms (comme
la visionneuse autonome, `controlsShown` / `hideTimer`). Le clic appelle
`toggleFullScreen()`, qui rend déjà la visibilité d'avant
(`prevVisibility`) et ne touche ni à l'élément courant ni au zoom (FR-002).
Le menu contextuel offre la même commande (FR-004).

**Rationale**: reprend le modèle déjà validé par l'utilisateur dans la
visionneuse autonome.

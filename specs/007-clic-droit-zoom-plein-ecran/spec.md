# Feature Specification: Ergonomie des visionneuses et clic droit (plein écran, zoom, flèches, menus contextuels)

**Feature Branch**: `claude/007-clic-droit-zoom-plein-ecran`

**Created**: 2026-11-24

**Status**: Draft

**Input**: User description: "Right click options in the app. Automatically stopping at 100% zoom when zooming in, before allowing to zoom further unless the zooming action/animation starts from a point where the image is already 90% zoomed in. A way to exit full screen view without pressing esc." + "zooming in/out with direction up/down arrows and move to the next/previous image with left/right" + "Slow down zoom animations and add inertia, as it's done for picasa"

## Clarifications

### Session 2026-11-24

- Q: Les menus du clic droit de la bibliothèque et des visionneuses
  proposent-ils des actions qui touchent aux fichiers ou à Windows ? → A :
  Oui, comme Picasa : Mettre à la corbeille (avec confirmation, journal et
  annulation), Définir comme fond d'écran et Propriétés.
- Q: Le glisser d'une photo agrandie garde-t-il un élan quand on la lâche ?
  → A : Non, arrêt net comme aujourd'hui ; l'inertie ne concerne que le zoom.

## Contexte

Des retouches d'ergonomie relevées en utilisant Picazoo au quotidien, surtout
dans les deux visionneuses (intégrée, 001 ; autonome, 005).

- **Plein écran de la visionneuse intégrée.** Le plein écran (F11, F, ou le
  bouton de l'en-tête) masque l'en-tête et le pied de la visionneuse : il ne
  reste à l'écran que l'image et les flèches précédente / suivante. Aucun
  élément visible ne permet d'en sortir ; il faut connaître F11, ou appuyer
  sur Échap, qui ferme du même coup la visionneuse et renvoie à la
  bibliothèque. La visionneuse autonome (005) n'a pas ce problème : ses
  commandes, dont le bouton de fermeture, réapparaissent dès que la souris
  bouge.
- **Zoom.** La molette, le pincement et Ctrl + agrandissent l'image par
  crans, ancrés sous le curseur, jusqu'à 1 600 %. En partant de l'ajustement à
  la fenêtre, quelques crans rapides font passer 100 % sans s'y arrêter, alors
  que 100 % (un pixel de la photo pour un pixel de l'écran) est la taille
  qu'on cherche le plus souvent pour juger la netteté. Chaque cran est animé
  en environ 0,12 seconde et s'arrête net : le zoom paraît sec à côté de
  celui de la Visionneuse de photos Picasa, qui glisse et garde un peu d'élan.
- **Clavier.** ← et → changent déjà de photo dans les deux visionneuses ; ↑
  et ↓ n'y font rien, et zoomer au clavier demande Ctrl + / Ctrl −.
- **Clic droit.** Il existe déjà sur les vignettes de la bibliothèque (Ouvrir,
  Afficher dans l'Explorateur, Ouvrir avec l'application par défaut), sur les
  dossiers de la liste (Voir les doublons de ce dossier) et sur les copies et
  groupes de doublons. Il ne fait rien dans les deux visionneuses, ni dans la
  zone vide de la grille, et les menus existants sont pauvres par rapport à ce
  qu'on attend d'une application Windows (copier, copier le chemin…).

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Quitter le plein écran à la souris (Priority: P1)

Dans la visionneuse intégrée, en plein écran, l'utilisateur bouge la souris :
un bouton « Quitter le plein écran » apparaît en haut à droite de l'écran. Un
clic le ramène à la visionneuse en fenêtre, sur la même photo, au même zoom.
Le bouton s'efface de lui-même quand la souris reste immobile, pour ne pas
gêner la vue.

**Why this priority**: c'est aujourd'hui une impasse : une personne qui ne
connaît pas F11 ne peut sortir du plein écran qu'en quittant la visionneuse.
C'est aussi la plus petite des trois retouches.

**Independent Test**: ouvrir une photo, passer en plein écran, bouger la
souris, cliquer sur le bouton : la visionneuse revient en fenêtre sur la même
photo, sans repasser par la bibliothèque.

**Acceptance Scenarios**:

1. **Given** la visionneuse intégrée en plein écran, **When** la souris bouge,
   **Then** un bouton « Quitter le plein écran » apparaît en moins d'un
   dixième de seconde, en haut à droite, lisible sur une photo claire comme
   sur une photo sombre.
2. **Given** le bouton visible, **When** la souris reste immobile 2 secondes
   sans être posée sur lui, **Then** il s'efface en fondu ; il reste affiché
   tant que la souris est dessus.
3. **Given** le bouton visible, **When** l'utilisateur clique dessus,
   **Then** la fenêtre retrouve l'état qu'elle avait avant le plein écran
   (fenêtrée ou agrandie), l'en-tête et le pied réapparaissent, et la photo,
   le zoom et la position de l'image sont conservés.
4. **Given** la visionneuse en plein écran, **When** l'utilisateur appuie sur
   F11 ou F, ou sur Échap, **Then** le comportement actuel est inchangé
   (F11 / F basculent le plein écran ; Échap ferme la visionneuse).
5. **Given** une vidéo en lecture en plein écran, **When** la souris bouge,
   **Then** le bouton apparaît de la même façon, sans masquer ni gêner les
   commandes de lecture de la vidéo.

---

### User Story 2 - Butée à 100 % au zoom avant (Priority: P2)

L'utilisateur zoome sur une photo affichée ajustée à la fenêtre (par exemple
à 35 %) en tournant la molette d'un geste rapide. Le zoom s'arrête net à
100 % au lieu de filer vers 200 % ou 400 %. S'il veut aller plus loin, il
relance un geste : comme l'image est alors à 100 % (au-dessus du seuil de
90 %), le zoom continue librement.

**Why this priority**: 100 % est la taille de référence pour juger une
photo ; la manquer oblige à revenir en arrière, ou à connaître la touche 1 ou
le bouton « 100 % ». Moins bloquant que la sortie du plein écran.

**Independent Test**: dans chaque visionneuse, partir de l'ajustement,
tourner la molette vite et longtemps en zoom avant : l'affichage indique
exactement 100 % et y reste ; relâcher, puis tourner de nouveau : le zoom
dépasse 100 %.

**Acceptance Scenarios**:

1. **Given** une image affichée à moins de 90 %, **When** un geste de zoom
   avant (molette, pincement, Ctrl +, bouton de zoom avant) l'amènerait
   au-delà de 100 %, **Then** le zoom s'arrête exactement à 100 %, sans
   dépasser puis revenir, et le point de l'image sous le curseur reste sous
   le curseur.
2. **Given** le zoom arrêté à 100 % par la butée, **When** le même geste se
   poursuit (la molette continue de tourner, les doigts continuent de
   s'écarter), **Then** le zoom reste à 100 % jusqu'à la fin du geste ; les
   crans supplémentaires ne s'accumulent pas pour plus tard.
3. **Given** une image affichée à 90 % ou plus (y compris à 100 % après la
   butée), **When** un nouveau geste de zoom avant commence, **Then** le zoom
   progresse librement jusqu'au maximum, sans arrêt à 100 %.
4. **Given** une image affichée à 80 %, **When** un geste de zoom avant
   commence, **Then** il progresse normalement jusqu'à 100 % et s'y arrête
   (le seuil s'évalue au début du geste, pas à chaque cran).
5. **Given** une image affichée au-dessus de 100 %, **When** l'utilisateur
   zoome en arrière, **Then** aucune butée ne s'applique : le zoom arrière est
   inchangé.
6. **Given** n'importe quel zoom, **When** l'utilisateur double-clique sur
   l'image ou utilise « 100 % » / la touche 1 / « Ajuster » / la touche 0,
   **Then** le comportement actuel est inchangé.
7. **Given** une petite image dont l'ajustement à la fenêtre est déjà de 90 %
   ou plus, **When** l'utilisateur zoome en avant, **Then** le zoom dépasse
   100 % dès le premier geste.

---

### User Story 3 - Zoomer avec ↑ / ↓, changer de photo avec ← / → (Priority: P2)

Dans l'une ou l'autre visionneuse, l'utilisateur garde la main sur les
flèches du clavier : ↑ agrandit la photo, ↓ la réduit, ← et → passent à la
photo précédente ou suivante. Il n'a plus besoin de Ctrl + / Ctrl − ni de la
souris pour inspecter une série de photos.

**Why this priority**: demandé avec la butée de zoom, qu'il complète
(la flèche ↑ obéit à la même butée) ; petite modification, utile tous les
jours.

**Independent Test**: dans chaque visionneuse, ouvrir une photo, appuyer sur
↑ plusieurs fois (le zoom augmente, s'arrête à 100 % selon US2), sur ↓ (il
diminue jusqu'à l'ajustement), puis sur → et ← (photo suivante, précédente).

**Acceptance Scenarios**:

1. **Given** une photo affichée dans une visionneuse, **When** l'utilisateur
   appuie sur ↑, **Then** la photo est agrandie d'un cran (le même qu'un cran
   de molette ou que Ctrl +), autour du centre de la vue.
2. **Given** une photo agrandie, **When** l'utilisateur appuie sur ↓, **Then**
   elle est réduite d'un cran, sans descendre sous l'ajustement à la fenêtre.
3. **Given** une photo ajustée à moins de 90 %, **When** l'utilisateur
   maintient ↑ enfoncée, **Then** le zoom s'arrête à 100 % (US2) ; relâcher
   puis appuyer de nouveau le fait aller au-delà.
4. **Given** une photo, agrandie ou non, **When** l'utilisateur appuie sur →
   ou ←, **Then** la visionneuse passe à la photo suivante ou précédente,
   qui s'ouvre ajustée, comme aujourd'hui.
5. **Given** une vidéo dans la visionneuse intégrée, **When** l'utilisateur
   appuie sur ↑ ou ↓, **Then** rien ne se passe ; ← et → changent
   d'élément comme aujourd'hui.
6. **Given** un menu ou une fenêtre surgissante ouverts (menu contextuel,
   menu « Plus »), **When** l'utilisateur appuie sur une flèche, **Then** la
   flèche sert à parcourir le menu, pas à zoomer ni à changer de photo.

---

### User Story 4 - Zoom plus doux, avec inertie, comme Picasa (Priority: P2)

Quand l'utilisateur zoome, la photo ne saute plus d'une taille à l'autre en
un éclair : elle glisse vers sa nouvelle taille, un peu plus lentement
qu'aujourd'hui, et continue sur sa lancée quand la molette s'arrête, en
ralentissant jusqu'à s'immobiliser, comme dans la Visionneuse de photos
Picasa. Un tour de molette rapide va plus loin qu'un tour lent ; un seul cran
reste un petit pas précis.

**Why this priority**: c'est ce qui donne au zoom de Picasa sa sensation de
fluidité ; aujourd'hui l'animation (environ 0,12 seconde) est si brève que
le zoom paraît saccadé. Elle se conçoit avec la butée (US2), qui doit
arrêter aussi l'élan.

**Independent Test**: dans chaque visionneuse, donner un cran de molette
(le zoom glisse en environ un quart de seconde et s'arrête), puis un tour
rapide de plusieurs crans et lâcher la molette (le zoom continue un court
instant en ralentissant, puis s'immobilise), en comparant à une vidéo de
référence de la Visionneuse de photos Picasa.

**Acceptance Scenarios**:

1. **Given** une photo dans une visionneuse, **When** l'utilisateur donne un
   seul cran de molette, **Then** le zoom passe d'une taille à l'autre en
   glissant, avec un départ franc et une arrivée en douceur, en 0,25 à
   0,35 seconde, et le point sous le curseur y reste pendant tout le
   mouvement.
2. **Given** une photo, **When** l'utilisateur tourne la molette vite sur
   plusieurs crans puis la lâche, **Then** le zoom continue après le dernier
   cran, en ralentissant progressivement, et s'immobilise en moins de
   0,6 seconde ; plus le geste était rapide, plus l'élan va loin.
3. **Given** un zoom avec élan en cours, **When** l'utilisateur donne un cran
   dans l'autre sens, clique ou commence à faire glisser l'image, **Then**
   l'élan s'arrête aussitôt et le nouveau geste prend la main sans à-coup.
4. **Given** un geste soumis à la butée (US2), **When** l'élan devrait
   emmener le zoom au-delà de 100 %, **Then** il s'arrête en douceur sur
   100 % exactement, sans le dépasser puis revenir.
5. **Given** un élan qui atteindrait le zoom maximal ou l'ajustement à la
   fenêtre, **When** la limite est atteinte, **Then** le zoom s'y pose en
   douceur, sans rebond.
6. **Given** les animations désactivées dans Windows (réduction des
   mouvements), **When** l'utilisateur zoome, **Then** le zoom est immédiat
   et sans élan, comme le prévoient déjà les visionneuses.
7. **Given** les flèches ↑ / ↓, Ctrl + / Ctrl − ou les boutons de zoom,
   **When** l'utilisateur les utilise, **Then** chaque pas glisse avec la
   même animation plus lente, sans élan après le dernier pas.

---

### User Story 5 - Menus contextuels partout où l'on clique (Priority: P3)

Un clic droit ouvre, partout où il a un sens, un menu des actions qui
s'appliquent à ce qui est sous le curseur : une vignette ou une sélection de
vignettes, la zone vide de la grille, un dossier de la liste, l'image dans la
visionneuse intégrée, l'image dans la visionneuse autonome, une copie ou un
groupe de doublons. Le menu regroupe des actions déjà disponibles ailleurs
(boutons, menus du haut, raccourcis) et les actions courantes sous Windows
que proposait Picasa : copier l'image, copier le chemin, mettre à la
corbeille, définir comme fond d'écran, propriétés.

**Why this priority**: confort et découverte des fonctions plutôt que
déblocage ; la portée est aussi la plus large des trois.

**Independent Test**: faire un clic droit sur chacune des surfaces listées
en FR-060 et vérifier que le menu s'ouvre sous le curseur, avec les actions
prévues, et que chaque action fait ce qu'elle annonce.

**Acceptance Scenarios**:

1. **Given** la bibliothèque, **When** l'utilisateur fait un clic droit sur
   une vignette non sélectionnée, **Then** elle devient la sélection et le
   menu des actions d'un élément s'ouvre sous le curseur.
2. **Given** plusieurs vignettes sélectionnées, **When** l'utilisateur fait un
   clic droit sur l'une d'elles, **Then** la sélection est conservée, le menu
   indique le nombre d'éléments concernés, les actions possibles sur
   plusieurs éléments s'appliquent à tous, et les actions réservées à un seul
   élément sont grisées.
3. **Given** la grille, **When** l'utilisateur fait un clic droit dans une
   zone sans vignette, **Then** un menu propose les actions de la vue
   (taille des vignettes, tout sélectionner, ouvrir le dossier dans
   l'Explorateur).
4. **Given** la visionneuse intégrée ou autonome, **When** l'utilisateur fait
   un clic droit sur l'image, **Then** un menu propose navigation, zoom,
   plein écran (ou sa sortie), informations, copier, copier le chemin,
   afficher dans l'Explorateur, ouvrir avec une autre application, et
   fermer ; aucun zoom ni déplacement de l'image n'est déclenché par ce clic.
5. **Given** un menu ouvert, **When** l'utilisateur appuie sur Échap ou
   clique ailleurs, **Then** le menu se ferme sans rien faire d'autre (en
   particulier, Échap ne ferme pas aussi la visionneuse).
6. **Given** une vignette ou une image courante, **When** l'utilisateur
   appuie sur la touche Menu du clavier ou sur Maj+F10, **Then** le même menu
   s'ouvre, placé près de l'élément courant.
7. **Given** l'action « Copier », **When** l'utilisateur la choisit puis
   colle dans une autre application (traitement de texte, messagerie),
   **Then** la photo y est collée.
8. **Given** une photo dans une visionneuse, **When** l'utilisateur choisit
   « Mettre à la corbeille » (ou appuie sur Suppr) et confirme, **Then** le
   fichier part dans la corbeille de Windows, l'opération apparaît au journal,
   un message propose « Annuler », et la visionneuse affiche la photo
   suivante.
9. **Given** la confirmation de mise à la corbeille affichée, **When**
   l'utilisateur appuie sur Entrée sans rien choisir, **Then** rien n'est
   mis à la corbeille (« Annuler » est le choix par défaut).
10. **Given** une photo, **When** l'utilisateur choisit « Définir comme fond
    d'écran », **Then** le bureau de Windows affiche cette photo, et
    « Annuler » dans le message rétablit le fond d'écran précédent.
11. **Given** une vignette, **When** l'utilisateur choisit « Propriétés »,
    **Then** la fenêtre Propriétés de Windows s'ouvre pour ce fichier.

---

### Edge Cases

- **Plein écran puis changement de photo** : le bouton de sortie reste
  disponible au mouvement de la souris quelle que soit la photo affichée.
- **Plein écran sur un second écran** : le bouton apparaît sur l'écran où se
  trouve la visionneuse.
- **Souris immobile au-dessus du bouton** : il ne s'efface pas tant que le
  curseur est dessus.
- **Plein écran et menu contextuel ouvert** : le bouton de sortie ne masque
  pas le menu ; le menu ne s'efface pas avec le bouton.
- **Zoom : geste interrompu par une pause** : une pause de plus de
  0,3 seconde entre deux crans de molette termine le geste ; le cran suivant
  commence un nouveau geste et le seuil de 90 % est réévalué.
- **Zoom : touche Ctrl + maintenue** : la répétition automatique de la touche
  forme un seul geste ; il faut relâcher et appuyer de nouveau pour dépasser
  100 %.
- **Zoom : pincement commencé sous 90 %** : le zoom s'arrête à 100 % pour
  tout le reste du pincement ; écarter les doigts au-delà ne change plus rien
  jusqu'à ce qu'ils soient relevés.
- **Zoom : butée pendant une animation** : si des crans rapides relancent
  l'animation, elle se termine exactement à 100 %, sans à-coup.
- **Zoom : changement de photo pendant un geste** : la nouvelle photo
  s'ouvre ajustée, comme aujourd'hui ; le geste en cours ne s'y applique pas.
- **Zoom : image dont la taille réelle n'est pas encore connue** (aperçu
  intégré d'un RAW en cours de chargement) : la butée se fonde sur la taille
  réelle de l'image dès qu'elle est connue ; avant, elle se fonde sur la
  meilleure estimation disponible et ne s'arrête jamais à une valeur qui ne
  correspond pas à 100 % de la photo.
- **Clic droit sur un dossier sans chemin** (ligne d'année dans la vue par
  année) : pas de menu, ou seulement les actions qui ont un sens pour une
  année.
- **Clic droit sur un élément dont le fichier a disparu** : les actions qui
  ont besoin du fichier (copier, ouvrir avec…) sont grisées ou signalent
  calmement que le fichier est introuvable ; rien ne plante.
- **Copier une vidéo** : « Copier » met le fichier dans le presse-papiers
  (comme dans l'Explorateur) ; « Copier l'image » n'est pas proposé.
- **Clic droit dans une visionneuse pendant un déplacement de l'image** : le
  glisser en cours n'est pas perturbé, et le clic droit ne lance pas de
  zoom.

## Requirements *(mandatory)*

### Functional Requirements

**Sortie du plein écran (US1)**

- **FR-001**: En plein écran, la visionneuse intégrée MUST afficher un bouton
  « Quitter le plein écran » quand la souris bouge, et le masquer en fondu
  après 2 secondes d'immobilité, sauf si le curseur est sur le bouton.
- **FR-002**: Le bouton MUST rendre à la fenêtre l'état qu'elle avait avant
  le plein écran et laisser la visionneuse ouverte sur la même photo, avec le
  même zoom et la même position d'image.
- **FR-003**: Le bouton MUST rester lisible sur tout fond (photo claire ou
  sombre) et indiquer son raccourci (F11) dans son info-bulle.
- **FR-004**: Le menu contextuel de la visionneuse (FR-064) MUST proposer
  « Quitter le plein écran » quand la visionneuse est en plein écran, et
  « Plein écran » sinon.
- **FR-005**: Les raccourcis existants (F11, F, Échap) MUST garder leur
  comportement actuel.

**Butée de zoom à 100 % (US2)**

- **FR-010**: Dans la visionneuse intégrée et dans la visionneuse autonome,
  un geste de zoom avant qui commence alors que l'image est affichée à moins
  de 90 % de sa taille réelle MUST s'arrêter exactement à 100 % s'il devait
  aller au-delà.
- **FR-011**: Une fois la butée atteinte, le reste du même geste MUST être
  sans effet sur le zoom (pas d'accumulation de crans).
- **FR-012**: Un geste de zoom avant qui commence alors que l'image est
  affichée à 90 % ou plus MUST progresser sans butée, jusqu'au zoom maximal
  actuel.
- **FR-013**: Un geste MUST être défini ainsi : pour la molette ou le pavé
  tactile, une suite de crans dans le sens du zoom avant sans pause de plus
  de 0,3 seconde ; pour le pincement, du contact des doigts à leur retrait ;
  pour le clavier (Ctrl + ou ↑), un appui (répétition automatique comprise) ; pour le
  bouton de zoom avant, un clic.
- **FR-014**: La butée MUST conserver l'ancrage actuel du zoom (le point de
  l'image sous le curseur y reste) et l'animation actuelle, qui se termine
  sur 100 % sans dépassement visible.
- **FR-015**: Le zoom arrière, le double-clic, « Ajuster », « 100 % » et les
  touches 0 et 1 MUST garder leur comportement actuel.
- **FR-016**: L'indicateur de zoom MUST afficher « 100 % » quand la butée est
  atteinte.

**Flèches du clavier (US3)**

- **FR-017**: Dans les deux visionneuses, ↑ MUST agrandir la photo d'un cran
  et ↓ la réduire d'un cran, autour du centre de la vue, avec le même pas et
  la même animation que Ctrl + / Ctrl −. ↑ obéit à la butée (FR-010 à
  FR-013) ; ↓ ne descend pas sous l'ajustement.
- **FR-018**: ← et → MUST continuer de passer à la photo précédente et
  suivante, que la photo soit agrandie ou non.
- **FR-019**: Sur une vidéo, ↑ et ↓ MUST être sans effet ; quand un menu ou
  une fenêtre surgissante est ouvert, les flèches MUST servir à ce menu.

**Douceur et inertie du zoom (US4)**

- **FR-040**: Dans les deux visionneuses, chaque pas de zoom (cran de
  molette, flèche, Ctrl + / −, bouton) MUST être animé en 0,25 à
  0,35 seconde, avec une courbe à départ franc et arrivée douce, ancré comme
  aujourd'hui sous le curseur (ou au centre pour le clavier et les boutons).
- **FR-041**: Un geste de molette ou de pavé tactile MUST laisser un élan
  après son dernier cran : le zoom continue dans le même sens en
  ralentissant, s'immobilise en moins de 0,6 seconde, et va d'autant plus
  loin que le geste était rapide. Un cran isolé ne laisse pas d'élan
  perceptible au-delà de son pas.
- **FR-042**: L'élan MUST s'arrêter immédiatement dès que l'utilisateur
  zoome dans l'autre sens, clique, fait glisser l'image ou change de photo.
- **FR-043**: L'élan MUST respecter la butée de 100 % (FR-010) et les
  limites de zoom (ajustement, zoom maximal) en s'y posant en douceur, sans
  dépassement ni rebond.
- **FR-044**: Le clavier, les boutons, le double-clic et « Ajuster » /
  « 100 % » MUST utiliser l'animation plus lente mais sans élan.
- **FR-045**: Quand les animations sont désactivées (réglage de Windows), le
  zoom MUST rester immédiat et sans élan.
- **FR-046**: L'animation et l'élan MUST rester fluides (pas d'image figée
  ni de saccade visible) sur la machine de référence du projet, y compris
  pendant le chargement de la pleine résolution d'une grande photo.
- **FR-047**: Le déplacement d'une photo agrandie (glisser) MUST rester
  sans élan : l'image s'arrête net quand on la lâche, comme aujourd'hui.

**Menus contextuels (US5)**

- **FR-060**: Un clic droit MUST ouvrir un menu contextuel sur chacune de ces
  surfaces : vignette (ou sélection de vignettes) de la bibliothèque, zone
  vide de la grille, dossier de la liste des dossiers, image de la visionneuse
  intégrée, image de la visionneuse autonome, copie de doublon, groupe de
  doublons.
- **FR-061**: Le menu d'une vignette MUST proposer au minimum : Ouvrir,
  Ouvrir dans la visionneuse autonome (photos uniquement), Afficher dans
  l'Explorateur, Ouvrir avec l'application par défaut, Copier, Copier le
  chemin, Voir les doublons de cette photo (quand elle en a).
- **FR-062**: Quand plusieurs vignettes sont sélectionnées, le menu MUST
  indiquer le nombre d'éléments, appliquer Copier et Copier le chemin à tous
  (un chemin par ligne), et griser les actions réservées à un seul élément.
- **FR-063**: Le menu de la zone vide de la grille MUST proposer : Agrandir
  les vignettes, Réduire les vignettes, Tout sélectionner, Ouvrir le dossier
  dans l'Explorateur (quand un dossier est affiché).
- **FR-064**: Le menu de l'image dans chacune des visionneuses MUST
  proposer : Précédente, Suivante, Zoom avant, Zoom arrière, Ajuster, 100 %,
  Plein écran / Quitter le plein écran (visionneuse intégrée), Informations,
  Copier l'image, Copier le chemin, Afficher dans l'Explorateur, Ouvrir avec
  une autre application, et Fermer (retour à la bibliothèque, ou fermeture de
  la visionneuse autonome). La visionneuse autonome MUST y ajouter « Ouvrir
  dans Picazoo ».
- **FR-065**: Le menu d'un dossier MUST proposer : Afficher dans
  l'Explorateur, Copier le chemin, Voir les doublons de ce dossier.
- **FR-066**: Les menus existants des doublons MUST être conservés.
- **FR-067**: Chaque entrée de menu qui a un raccourci clavier MUST
  l'afficher ; les entrées sans objet dans le contexte MUST être grisées
  plutôt que cachées, pour que le menu garde la même forme.
- **FR-068**: La touche Menu du clavier et Maj+F10 MUST ouvrir le menu de
  l'élément courant, placé près de lui.
- **FR-069**: Échap ou un clic en dehors MUST fermer le menu sans autre effet ;
  un clic droit MUST ne jamais déclencher de zoom, de déplacement d'image ni
  d'ouverture.
- **FR-070**: Les menus d'une vignette (ou d'une sélection) et de l'image
  dans chacune des visionneuses MUST proposer aussi, dans un groupe séparé en
  bas du menu : Mettre à la corbeille (Suppr), Définir comme fond d'écran,
  Propriétés.
- **FR-071**: « Mettre à la corbeille » MUST demander confirmation en
  nommant le fichier, ou le nombre de fichiers pour une sélection, avec
  « Annuler » comme choix par défaut ; envoyer les fichiers à la corbeille de
  Windows, jamais les effacer définitivement ; inscrire l'opération au
  journal des opérations ; et proposer de l'annuler (restauration depuis la
  corbeille), aussitôt par un message « Annuler » puis plus tard depuis le
  journal, comme les mises à la corbeille des doublons (principe I).
- **FR-072**: Sur un disque sans corbeille, « Mettre à la corbeille » MUST
  être grisé, avec une info-bulle qui explique pourquoi ; Picazoo ne propose
  jamais d'effacement définitif à la place.
- **FR-073**: Après une mise à la corbeille depuis une visionneuse, elle MUST
  afficher la photo suivante (la précédente s'il n'y en a plus après, et se
  fermer s'il n'en reste aucune) ; dans la bibliothèque, l'élément disparaît
  de la grille et la sélection passe à l'élément voisin.
- **FR-074**: « Définir comme fond d'écran » MUST n'être proposé que pour une
  seule photo ; il remplace le fond d'écran de Windows par cette photo, avec
  la disposition déjà choisie dans Windows, sans copier ni modifier le
  fichier dans les dossiers de l'utilisateur, et affiche un message
  « Annuler » qui rétablit le fond d'écran précédent.
- **FR-075**: « Propriétés » MUST ouvrir la fenêtre Propriétés de Windows
  pour le fichier, ou pour la sélection.
- **FR-076**: À part ces actions et celles déjà existantes des doublons,
  aucune entrée de menu ne MUST modifier, déplacer ou supprimer un fichier de
  l'utilisateur.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Une personne qui découvre le plein écran en sort à la souris,
  sans clavier et sans aide, en moins de 5 secondes, et se retrouve dans la
  visionneuse (pas dans la bibliothèque).
- **SC-002**: Sur 20 gestes de molette rapides partant de l'ajustement, dans
  chaque visionneuse, 20 s'arrêtent exactement à 100 %.
- **SC-003**: Sur 20 gestes partant de 90 % ou plus, 20 dépassent 100 % dès
  le premier cran.
- **SC-004**: Chaque surface listée en FR-060 ouvre son menu au clic droit en
  moins de 0,1 seconde, y compris avec 1 000 vignettes sélectionnées.
- **SC-005**: Toutes les actions des barres d'outils des deux visionneuses
  sont accessibles au clic droit.
- **SC-006**: Aucune action de menu ne modifie un fichier de l'utilisateur
  sans confirmation (vérifié sur dossiers de test).
- **SC-007**: Dans chaque visionneuse, une série de 20 photos s'inspecte
  (agrandir, réduire, passer à la suivante) au clavier seul, avec les quatre
  flèches.
- **SC-008**: Comparé côte à côte avec la Visionneuse de photos Picasa sur
  les mêmes photos, le zoom à la molette est jugé au moins aussi doux par
  l'utilisateur ; aucune saccade visible sur la machine de référence.

## Assumptions

- La visionneuse autonome n'a pas besoin d'un bouton de sortie du plein
  écran : elle couvre l'écran par nature et ses commandes, dont « Fermer »,
  réapparaissent déjà au mouvement de la souris. Elle reçoit la butée de zoom
  et le menu contextuel.
- Échap garde son comportement actuel (fermer la visionneuse, y compris en
  plein écran) ; le changer n'a pas été demandé.
- Le seuil de 90 % et la butée de 100 % portent sur la taille réelle de la
  photo (100 % = un pixel de la photo par pixel d'écran), comme l'indicateur
  de zoom actuel ; ils ne sont pas réglables.
- La butée ne concerne que le zoom avant ; en zoom arrière, rien ne change.
- La vue de comparaison des doublons (zoom relatif à l'ajustement, jusqu'à
  12×) garde son zoom actuel : ni butée, ni inertie, ni flèches ↑ / ↓.
- « Copier » d'une photo met à la fois l'image et le fichier dans le
  presse-papiers, pour coller indifféremment dans un document ou dans un
  dossier de l'Explorateur ; d'une vidéo, seulement le fichier.
- Les flèches ne changent de rôle que dans les visionneuses ; dans la grille
  de la bibliothèque, elles continuent de déplacer la sélection.
- L'inertie porte sur le zoom seulement ; le glisser d'une photo agrandie
  reste sans élan (clarification du 2026-11-24).
- Suppr met à la corbeille dans la grille de la bibliothèque et dans les deux
  visionneuses ; dans l'écran des doublons, Suppr garde son rôle actuel
  (ouvrir le récapitulatif).
- La mise à la corbeille réutilise la corbeille, le journal des opérations et
  l'annulation déjà construits pour les doublons (002, 006) ; un élément mis
  à la corbeille quitte la bibliothèque, et y revient s'il est restauré. La
  visionneuse autonome, qui ne charge pas la bibliothèque, inscrit ses mises
  à la corbeille dans le même journal, pour qu'on puisse les annuler depuis
  Picazoo, y compris pour une photo hors des dossiers surveillés.
- « Définir comme fond d'écran » n'est pas proposé pour les vidéos, ni pour
  les formats que Windows n'accepte pas comme fond d'écran sans conversion
  (RAW, HEIC) : l'entrée est grisée avec une info-bulle.
- Les durées (0,25 à 0,35 s par pas, élan éteint en moins de 0,6 s) sont une
  estimation de la sensation de Picasa ; elles seront calées à l'œil contre
  une vidéo de référence de la Visionneuse de photos Picasa pendant
  l'implémentation, sans en faire un réglage.
- Les libellés sont en français, comme le reste de l'interface.

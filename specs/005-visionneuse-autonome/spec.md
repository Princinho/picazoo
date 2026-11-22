# Feature Specification: Visionneuse autonome

**Feature Branch**: `005-visionneuse-autonome`

**Created**: 2026-11-22

**Status**: Draft

**Input**: User description: "Comme dans Picasa, on double-clique sur une image dans l'Explorateur Windows et seule une visionneuse légère s'ouvre, sans l'application complète. Elle affiche cette image et permet de passer à toutes les autres images du même dossier. Zoom et dézoom à la molette de la souris. Transitions fluides et animées d'une image à l'autre."

## Clarifications

### Session 2026-11-22

- Q: Fenêtre classique sur fond sombre, ou voile semi-transparent posé sur le bureau comme la Visionneuse de photos Picasa ? → A: Le voile semi-transparent.

## Contexte

Picasa installait à côté de la bibliothèque une petite visionneuse, la
« Visionneuse de photos Picasa ». Quand on double-cliquait sur une photo dans
l'Explorateur, c'était elle qui s'ouvrait, presque instantanément : la photo
en grand, les autres photos du dossier à portée des flèches, le zoom à la
molette, et un bouton pour passer à Picasa si l'on voulait en faire plus. On
pouvait l'utiliser tous les jours sans jamais ouvrir la bibliothèque.

La fonctionnalité 001 a déjà une visionneuse, mais elle vit dans
l'application complète et parcourt les éléments de la bibliothèque indexée.
La 005 en fait un usage autonome : la même façon de regarder les photos,
lancée directement sur un fichier du disque, sans bibliothèque, sans
dossiers surveillés, sans analyse. Elle réutilise ce que la 001 sait déjà
faire (formats, décodage rapide, orientation, zoom, panneau d'informations)
et y ajoute ce qui manque : l'ouverture depuis Windows, le parcours d'un
dossier quelconque, le zoom centré sur le curseur et des transitions
animées.

C'est une deuxième porte d'entrée dans Picazoo, pas une deuxième
application : elle suit le même modèle (fond sombre, image au centre, un
double-clic pour voir en grand) que le principe V de la constitution.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Ouvrir une photo depuis l'Explorateur en un instant (Priority: P1)

Dans l'Explorateur Windows, l'utilisateur double-clique sur une photo (ou
choisit « Ouvrir avec › Picazoo »). Presque aussitôt, un voile sombre
semi-transparent recouvre l'écran, le bureau restant deviné derrière, et la
photo s'y pose, entière, ajustée à l'écran, dans le bon sens. Rien
d'autre ne se passe : pas de bibliothèque, pas d'écran d'accueil, pas
d'analyse de dossiers, pas de bruit du disque. Le nom du fichier et sa
position dans le dossier (« 12 sur 87 ») sont affichés discrètement. Échap
ferme la visionneuse.

**Why this priority**: c'est la raison d'être de la fonctionnalité ; sans
une ouverture plus rapide que l'application complète, la visionneuse
autonome n'a pas d'intérêt.

**Independent Test**: Picazoo jamais configuré (aucun dossier surveillé) ;
dans l'Explorateur, clic droit sur une photo JPEG de 24 Mpx › « Ouvrir
avec › Picazoo » : la photo s'affiche en moins d'une seconde, aucun écran
d'accueil n'apparaît, aucune bibliothèque n'est créée, et le dossier de la
photo est inchangé après fermeture.

**Acceptance Scenarios**:

1. **Given** une photo dans un dossier quelconque du disque, **When**
   l'utilisateur l'ouvre avec Picazoo depuis l'Explorateur, **Then** la
   visionneuse affiche cette photo, entière et non déformée, en moins d'une
   seconde pour une photo d'appareil courante.
2. **Given** un Picazoo jamais configuré, **When** la visionneuse est
   ouverte depuis l'Explorateur, **Then** elle fonctionne normalement, sans
   proposer l'écran d'accueil de la bibliothèque ni lancer d'analyse.
3. **Given** l'application complète déjà ouverte et en train d'analyser la
   bibliothèque, **When** l'utilisateur ouvre une photo depuis
   l'Explorateur, **Then** la visionneuse s'ouvre dans sa propre fenêtre,
   aussi vite, sans attendre l'application ni la perturber.
4. **Given** la visionneuse ouverte sur une photo, **When** l'utilisateur
   double-clique sur une autre photo dans l'Explorateur, **Then** la même
   fenêtre passe à cette nouvelle photo (et à son dossier) au lieu d'en
   ouvrir une seconde.
5. **Given** une photo prise en tenant l'appareil verticalement, **When**
   elle est ouverte, **Then** elle apparaît dans le bon sens.

---

### User Story 2 - Passer d'une photo à l'autre du même dossier, avec des transitions fluides (Priority: P1)

Depuis la photo ouverte, les flèches ← et → du clavier (ou les boutons à
l'écran, ou un clic sur les bords gauche et droit de l'image) passent à la
photo précédente ou suivante du même dossier, dans l'ordre des noms. Le
passage est animé : l'image actuelle laisse doucement place à la suivante,
sans clignotement ni écran noir. Si l'utilisateur enchaîne les appuis ou
garde la touche enfoncée, les photos défilent sans attendre la fin des
animations. Début et Fin mènent à la première et à la dernière photo.

**Why this priority**: c'est l'autre moitié de la demande ; une visionneuse
qui ne montre qu'une photo oblige à revenir sans cesse à l'Explorateur.

**Independent Test**: dans un dossier de test de 10 photos (`photo1.jpg` à
`photo10.jpg`) et d'un fichier texte, ouvrir `photo2.jpg` depuis
l'Explorateur ; → mène à `photo3.jpg` (pas à `photo10.jpg`), avec une
transition visible ; 8 appuis rapides sur → mènent à `photo10.jpg` en moins
d'une demi-seconde après le dernier appui ; le fichier texte n'est jamais
proposé.

**Acceptance Scenarios**:

1. **Given** une photo ouverte, **When** l'utilisateur appuie sur →,
   **Then** la photo suivante du dossier apparaît par une transition animée
   et nette à son terme.
2. **Given** un dossier contenant des images, des vidéos et d'autres
   fichiers, **When** l'utilisateur parcourt le dossier, **Then** seules les
   images aux formats pris en charge sont proposées, dans l'ordre naturel de
   leurs noms (`2` avant `10`).
3. **Given** la dernière photo du dossier, **When** l'utilisateur appuie
   sur →, **Then** la visionneuse revient à la première photo, et une
   indication brève le signale.
4. **Given** une suite d'appuis rapides sur →, **When** les transitions
   n'ont pas eu le temps de se terminer, **Then** la visionneuse saute
   directement à la photo voulue, sans accumuler de retard.
5. **Given** les animations désactivées dans les réglages d'accessibilité de
   Windows, **When** l'utilisateur change de photo, **Then** le passage est
   immédiat, sans animation.
6. **Given** un dossier de plusieurs milliers de photos, **When**
   l'utilisateur en ouvre une, **Then** elle s'affiche aussi vite que dans un
   petit dossier, et le parcours du reste du dossier devient possible
   quelques instants plus tard.

---

### User Story 3 - Zoomer à la molette là où l'on regarde (Priority: P1)

L'utilisateur tourne la molette vers l'avant sur un visage de la photo :
l'image grossit autour du point situé sous le curseur, qui reste sous le
curseur, avec un mouvement doux. Vers l'arrière, elle rétrécit jusqu'à
revenir à l'ajustement à l'écran. Quand l'image est plus grande que
l'écran, il la fait glisser en maintenant le bouton de la souris. Un
double-clic bascule entre l'ajustement à l'écran et la taille réelle
(100 %) au point cliqué. Le niveau de zoom est affiché, et des boutons ainsi
que Ctrl + / Ctrl − / Ctrl 0 offrent les mêmes possibilités au clavier.

**Why this priority**: demandé explicitement ; c'est ce qui permet de
vérifier une mise au point ou de lire un détail sans autre logiciel.

**Independent Test**: ouvrir une photo de 24 Mpx contenant un repère net
dans un coin ; placer le curseur sur ce repère et tourner la molette de
5 crans vers l'avant : le repère reste sous le curseur et devient net en
pleine résolution ; faire glisser l'image ; 5 crans vers l'arrière ramènent
à l'ajustement à l'écran.

**Acceptance Scenarios**:

1. **Given** une photo ajustée à l'écran, **When** l'utilisateur tourne
   la molette vers l'avant avec le curseur sur un point de l'image,
   **Then** l'image grossit progressivement et ce point reste sous le
   curseur.
2. **Given** une photo zoomée, **When** l'utilisateur tourne la molette vers
   l'arrière, **Then** l'image rétrécit autour du curseur et s'arrête à
   l'ajustement à l'écran.
3. **Given** une photo zoomée plus grande que l'écran, **When**
   l'utilisateur la fait glisser, **Then** elle suit la souris sans jamais
   laisser un bord de l'image s'éloigner au-delà du bord de l'écran.
4. **Given** une photo zoomée au-delà de la définition de l'aperçu affiché,
   **When** le zoom s'arrête, **Then** la zone visible devient nette en
   pleine résolution en moins d'une seconde, sans que le zoom ou le
   déplacement soient bloqués entre-temps.
5. **Given** une photo zoomée, **When** l'utilisateur passe à la photo
   suivante, **Then** celle-ci s'ouvre ajustée à l'écran.
6. **Given** une image plus petite que l'écran (icône, capture), **When**
   elle est ouverte, **Then** elle est affichée à sa taille réelle, centrée,
   sans être agrandie ni floutée.

---

### User Story 4 - Choisir Picazoo pour ouvrir ses photos, sans que rien ne soit imposé (Priority: P2)

À l'installation, Picazoo s'inscrit auprès de Windows comme l'une des
applications capables d'ouvrir les images (il apparaît dans « Ouvrir
avec »), sans devenir l'application par défaut. La première fois que la
visionneuse est ouverte (ou depuis les réglages de l'application complète),
Picazoo propose une seule fois : « Ouvrir vos photos avec Picazoo quand vous
double-cliquez dessus ? », avec le choix des formats concernés (JPEG, PNG,
HEIC, RAW…). S'il accepte, Picazoo le guide jusqu'au réglage « Applications
par défaut » de Windows, où c'est l'utilisateur qui confirme. « Plus tard »
et « Ne plus demander » sont toujours possibles, et l'état actuel des
associations reste consultable et modifiable dans les réglages.

**Why this priority**: sans association, le double-clic de l'Explorateur
n'ouvre pas Picazoo ; mais « Ouvrir avec » suffit pour utiliser et tester la
visionneuse, et l'utilisateur doit rester maître de ses applications par
défaut.

**Independent Test**: sur une session Windows où une autre application
ouvre les JPEG par défaut, installer Picazoo : les JPEG s'ouvrent toujours
avec l'autre application ; Picazoo figure dans « Ouvrir avec » ; à la
première ouverture de la visionneuse, refuser la proposition : les
associations de Windows sont inchangées et la question ne revient pas avant
que l'utilisateur la redemande depuis les réglages.

**Acceptance Scenarios**:

1. **Given** l'installation de Picazoo, **When** elle se termine, **Then**
   Picazoo est proposé dans « Ouvrir avec » pour les formats d'images pris en
   charge, et aucune application par défaut n'a changé.
2. **Given** la première ouverture de la visionneuse, **When** l'utilisateur
   choisit « Ne plus demander », **Then** la proposition ne réapparaît plus,
   et les associations de Windows ne sont pas modifiées.
3. **Given** l'utilisateur qui accepte la proposition, **When** il valide
   les formats choisis, **Then** Picazoo l'amène au réglage de Windows
   correspondant ; seuls les formats qu'il y confirme s'ouvrent ensuite avec
   Picazoo.
4. **Given** Picazoo désinstallé, **When** l'utilisateur double-clique sur
   une photo, **Then** Windows propose une autre application, sans erreur ni
   association cassée.

---

### User Story 5 - Passer à l'application complète ou au reste de Windows (Priority: P2)

Depuis la visionneuse, un bouton « Ouvrir dans Picazoo » ouvre (ou ramène
au premier plan) l'application complète. Si le dossier de la photo fait
partie de la bibliothèque, l'application s'ouvre sur ce dossier, la photo
sélectionnée. Sinon, elle propose d'ajouter ce dossier aux dossiers
surveillés, sans le faire d'office. D'autres actions sont à portée :
afficher le fichier dans l'Explorateur, voir les informations de la photo
(nom, chemin, date de prise de vue, dimensions, poids, appareil), rendre le voile
opaque (F11) ou le remettre semi-transparent.

**Why this priority**: la visionneuse se suffit à elle-même pour regarder ;
ces passerelles évitent seulement de devoir chercher la photo ailleurs.

**Independent Test**: ouvrir depuis l'Explorateur une photo d'un dossier
surveillé, cliquer « Ouvrir dans Picazoo » : l'application s'affiche sur ce
dossier avec la photo sélectionnée. Recommencer avec une photo d'un dossier
non surveillé : l'application propose d'ajouter le dossier ; refuser : les
dossiers surveillés sont inchangés.

**Acceptance Scenarios**:

1. **Given** une photo d'un dossier surveillé ouverte dans la visionneuse,
   **When** l'utilisateur choisit « Ouvrir dans Picazoo », **Then**
   l'application complète s'affiche sur ce dossier, la photo sélectionnée.
2. **Given** une photo d'un dossier non surveillé, **When** l'utilisateur
   choisit « Ouvrir dans Picazoo », **Then** l'application lui propose
   d'ajouter ce dossier à la bibliothèque, et ne l'ajoute que s'il accepte.
3. **Given** une photo ouverte, **When** l'utilisateur ouvre le panneau
   d'informations, **Then** il voit les mêmes informations que dans la
   visionneuse de la bibliothèque (001).
4. **Given** une photo ouverte, **When** l'utilisateur choisit « Afficher
   dans l'Explorateur », **Then** l'Explorateur s'ouvre sur son dossier,
   le fichier sélectionné.

---

### User Story 6 - Voir les photos voisines d'un coup d'œil (Priority: P3)

En bas de la visionneuse, une bande de petites vignettes montre les photos
qui précèdent et suivent la photo affichée ; un clic sur l'une y mène
directement. La bande se masque d'elle-même quand la souris ne bouge plus,
et sur demande.

**Why this priority**: confort, comme la bande de la Visionneuse de
photos Picasa ; les flèches suffisent pour parcourir un dossier.

**Independent Test**: dans un dossier de 30 photos, ouvrir la 15e : la bande
montre des vignettes de part et d'autre ; un clic sur la 18e l'affiche avec
la même transition que la flèche ; après quelques secondes sans bouger la
souris, la bande disparaît.

**Acceptance Scenarios**:

1. **Given** une photo ouverte, **When** la bande est visible, **Then** elle
   montre les photos voisines dans l'ordre du parcours, la photo actuelle
   mise en évidence.
2. **Given** la bande, **When** l'utilisateur clique sur une vignette,
   **Then** la photo correspondante s'affiche.
3. **Given** un dossier qui n'a jamais été vu par Picazoo, **When** la bande
   s'affiche, **Then** ses vignettes apparaissent au fur et à mesure, sans
   ralentir l'affichage de la photo principale et sans être enregistrées sur
   le disque.

---

### Edge Cases

- **Fichier illisible ou corrompu** : un message « image illisible » prend
  la place de la photo ; les flèches permettent de passer aux autres.
- **Format non pris en charge** ouvert via « Ouvrir avec » (PSD, SVG…) : un
  message clair le dit et propose de l'ouvrir avec une autre application ;
  si le dossier contient des images prises en charge, on peut les parcourir.
- **Vidéo** ouverte via « Ouvrir avec » : la visionneuse ne la lit pas (voir
  Assumptions) ; elle propose de l'ouvrir dans Picazoo ou avec le lecteur
  par défaut.
- **Très grande image** (panorama de 100 Mpx) : elle s'affiche d'abord dans
  une définition adaptée à l'écran, puis se précise au zoom.
- **GIF animé** : il s'anime.
- **RAW** : affiché par son aperçu intégré, comme dans la 001 ; un RAW sans
  aperçu exploitable affiche « aperçu indisponible ».
- **Photo RAW et son JPEG côte à côte** (`IMG_0042.CR2`, `IMG_0042.JPG`) :
  les deux sont parcourues, l'une après l'autre.
- **Dossier modifié pendant la consultation** (photos ajoutées, supprimées,
  renommées dans l'Explorateur) : la liste du parcours suit ; si la photo
  affichée disparaît, la visionneuse le signale et passe à la suivante.
- **Dossier de plusieurs milliers d'images, ou sur un disque lent ou un
  partage réseau** : la photo demandée s'affiche d'abord ; la liste du
  dossier se complète ensuite sans bloquer la fenêtre.
- **Dossier sans droit de lecture** sur les autres fichiers : seule la
  photo ouverte est affichée, sans message bloquant.
- **Photo ouverte depuis une archive ZIP ou un téléphone** (Windows la copie
  dans un dossier temporaire) : la visionneuse l'affiche, et parcourt ce
  qu'il y a dans ce dossier temporaire.
- **Plusieurs photos sélectionnées puis ouvertes ensemble** dans
  l'Explorateur : une seule fenêtre s'ouvre, pas une par fichier.
- **Fichiers cachés ou système** (`Thumbs.db`, `desktop.ini`) : jamais
  proposés.
- **Chemin très long, caractères spéciaux** (accents, emoji) : la photo
  s'affiche normalement.
- **Écrans multiples** : le voile recouvre l'écran où se trouve la souris ;
  les autres écrans restent utilisables.
- **Notification ou fenêtre qui passe au premier plan** pendant la
  consultation : la visionneuse ne se ferme pas et revient au premier plan
  dès qu'on la sélectionne.
- **Stockage de Picazoo inaccessible ou disque plein** : la visionneuse
  fonctionne quand même ; seuls les réglages ne sont pas retenus.

## Requirements *(mandatory)*

### Functional Requirements

**Ouverture**

- **FR-001**: Le système MUST pouvoir ouvrir directement un fichier image
  donné par Windows (double-clic dans l'Explorateur une fois l'association
  faite, « Ouvrir avec », glisser-déposer sur l'icône de Picazoo) dans une
  visionneuse autonome, distincte de la fenêtre de la bibliothèque.
- **FR-002**: L'ouverture de la visionneuse MUST NOT lancer d'analyse, de
  surveillance de dossiers ni de calcul de vignettes de la bibliothèque, ni
  attendre qu'elle soit chargée ; elle MUST fonctionner même si la
  bibliothèque n'a jamais été configurée.
- **FR-003**: La visionneuse MUST afficher la photo demandée avant d'avoir
  fini de lister son dossier.
- **FR-004**: Si une visionneuse est déjà ouverte, l'ouverture d'une
  nouvelle photo depuis Windows MUST la réutiliser (elle passe à la
  nouvelle photo et à son dossier) au lieu d'ouvrir une seconde fenêtre.
- **FR-005**: La visionneuse MUST être indépendante de l'application
  complète : l'une peut être ouverte, fermée ou occupée sans bloquer
  l'autre.

**Parcours du dossier**

- **FR-006**: La visionneuse MUST permettre de parcourir toutes les images
  prises en charge du dossier de la photo ouverte (sans les sous-dossiers),
  dans l'ordre naturel des noms de fichiers, en ignorant les fichiers
  cachés, les fichiers système et les fichiers d'autres types.
- **FR-007**: Les formats parcourus et affichés MUST être les formats
  d'images de la 001 : JPEG, PNG, GIF (animé compris), BMP, TIFF, WebP,
  HEIC, et les RAW par leur aperçu intégré (au moins CR2, CR3, NEF, ARW,
  ORF, RW2, RAF, DNG).
- **FR-008**: Les utilisateurs MUST pouvoir passer à la photo précédente et
  suivante (flèches, boutons à l'écran, clic sur les bords de l'image), à
  la première et à la dernière (Début, Fin) ; après la dernière, le
  parcours MUST reprendre à la première, et inversement, avec une
  indication brève.
- **FR-009**: La liste du parcours MUST suivre les ajouts, suppressions et
  renommages faits dans le dossier pendant la consultation.
- **FR-010**: Chaque photo MUST être affichée dans le bon sens (orientation
  enregistrée par l'appareil), entière et non déformée, ajustée à
  l'écran ; une image plus petite que l'écran MUST être affichée à sa
  taille réelle, sans agrandissement.
- **FR-011**: La visionneuse MUST afficher le nom du fichier et sa position
  dans le parcours (« 12 sur 87 »).

**Transitions**

- **FR-012**: Le passage d'une photo à une autre MUST être animé (la photo
  actuelle laisse place à la suivante en un mouvement continu d'environ un
  quart de seconde), sans clignotement, écran noir ni image intermédiaire
  floue quand la photo suivante est déjà prête.
- **FR-013**: Une nouvelle demande de passage pendant une transition MUST
  être prise en compte immédiatement : la visionneuse rejoint la photo
  voulue sans jouer toutes les transitions intermédiaires.
- **FR-014**: Les photos voisines de la photo affichée MUST être préparées
  à l'avance pour que la transition montre directement la photo nette.
- **FR-015**: Quand les animations sont désactivées dans les réglages
  d'accessibilité de Windows, les passages et le zoom MUST être immédiats.

**Zoom et déplacement**

- **FR-016**: La molette MUST zoomer (vers l'avant) et dézoomer (vers
  l'arrière) autour du point situé sous le curseur, qui reste sous le
  curseur, avec un mouvement doux ; les pavés tactiles (pincement, défilement
  à deux doigts) MUST se comporter de la même façon.
- **FR-017**: Le zoom MUST aller de l'ajustement à l'écran (minimum)
  jusqu'à 16 fois la taille réelle (maximum), et son niveau MUST être
  affiché.
- **FR-018**: Un double-clic sur l'image MUST basculer entre l'ajustement à
  l'écran et la taille réelle centrée sur le point cliqué ; des boutons
  et les raccourcis Ctrl +, Ctrl − et Ctrl 0 MUST offrir zoom avant, zoom
  arrière et retour à l'ajustement.
- **FR-019**: Une image zoomée plus grande que l'écran MUST pouvoir être
  déplacée en la faisant glisser à la souris, sans que ses bords ne
  s'éloignent des bords de l'écran.
- **FR-020**: Au-delà de la définition de l'aperçu affiché, la zone visible
  MUST être affichée en pleine résolution dès qu'elle est disponible, sans
  bloquer le zoom ni le déplacement.
- **FR-021**: Passer à une autre photo MUST la montrer ajustée à
  l'écran.

**Fenêtre et passerelles**

- **FR-022**: La visionneuse MUST se présenter, comme la Visionneuse de
  photos Picasa, sous la forme d'un voile sombre semi-transparent qui
  recouvre tout l'écran (barre des tâches comprise), le bureau restant
  visible en transparence, avec la photo posée au centre et des commandes
  discrètes qui s'estompent quand la souris ne bouge plus et reviennent dès
  qu'elle bouge. Le voile apparaît et disparaît en fondu.
- **FR-023**: Les utilisateurs MUST pouvoir rendre le voile entièrement
  opaque (F11 ou un bouton) pour regarder sans distraction, et revenir au
  voile semi-transparent ; Échap et un bouton « Fermer » MUST fermer la
  visionneuse. Quand la transparence est désactivée dans les réglages de
  Windows, le voile MUST être opaque.
- **FR-023a**: La visionneuse MUST rester accessible comme une fenêtre
  ordinaire : présente dans la barre des tâches et dans Alt+Tab, elle cède
  la place quand l'utilisateur passe à une autre application et revient
  quand il y retourne.
- **FR-024**: La visionneuse MUST afficher sur demande les informations de
  la photo : nom, chemin, date de prise de vue (ou à défaut date du
  fichier), dimensions, poids et appareil quand ils sont connus.
- **FR-025**: La visionneuse MUST permettre d'afficher le fichier dans
  l'Explorateur, et d'ouvrir une photo non affichable avec une autre
  application.
- **FR-026**: La visionneuse MUST proposer « Ouvrir dans Picazoo » : si le
  dossier fait partie de la bibliothèque, l'application complète s'ouvre
  sur ce dossier, la photo sélectionnée ; sinon, elle propose d'ajouter le
  dossier aux dossiers surveillés et ne l'ajoute que sur acceptation.
- **FR-027**: La visionneuse MUST proposer une bande de vignettes des
  photos voisines, cliquables, qui se masque d'elle-même et sur demande.
- **FR-028**: La visionneuse MUST retenir d'une ouverture à l'autre le
  choix du voile (semi-transparent ou opaque) et l'affichage de la bande de
  vignettes.

**Association avec Windows**

- **FR-029**: L'installation MUST inscrire Picazoo comme application
  capable d'ouvrir les formats de FR-007 (présent dans « Ouvrir avec »)
  sans modifier aucune application par défaut ; la désinstallation MUST
  retirer cette inscription proprement.
- **FR-030**: Le système MUST proposer, une seule fois à la première
  ouverture de la visionneuse et à tout moment depuis les réglages, de
  faire de Picazoo l'application qui ouvre les photos, avec le choix des
  formats ; « Plus tard » et « Ne plus demander » MUST être proposés.
- **FR-031**: Le système MUST NOT changer une association de fichiers sans
  la confirmation de l'utilisateur ; quand il accepte, le système MUST le
  conduire au réglage de Windows où il confirme lui-même son choix.
- **FR-032**: Les réglages MUST montrer, pour chaque format, si Picazoo est
  actuellement l'application qui l'ouvre.

**Fichiers de l'utilisateur et stockage**

- **FR-033**: La visionneuse MUST NOT modifier, déplacer, renommer ni
  supprimer aucun fichier de l'utilisateur, MUST NOT écrire dans ses
  dossiers, et MUST ouvrir les fichiers en lecture seule sans empêcher les
  autres logiciels de les modifier ou de les supprimer pendant qu'elle les
  affiche.
- **FR-034**: La visionneuse MUST NOT enregistrer de vignettes, d'index ni
  de cache d'images sur le disque ; ses seules écritures sont ses réglages
  (FR-028, FR-030), dans le stockage propre de Picazoo.
- **FR-035**: La visionneuse MUST NOT créer, ouvrir en écriture ni modifier
  la bibliothèque (index, vignettes, dossiers surveillés), sauf par
  l'action explicite de FR-026.

### Key Entities

- **Parcours** : la liste ordonnée des images prises en charge d'un dossier,
  construite à l'ouverture et tenue à jour pendant la consultation, avec la
  position de la photo affichée ; elle n'existe qu'en mémoire.
- **Photo affichée** : le fichier montré, son orientation, ses informations
  (celles de la 001), son niveau de zoom et la zone visible.
- **Réglages de la visionneuse** : voile semi-transparent ou opaque, bande
  de vignettes, réponse à la proposition d'association
  (« plus tard », « ne plus demander », formats choisis).
- **Association de format** : pour chaque format pris en charge, le fait
  que Picazoo soit inscrit auprès de Windows et qu'il soit ou non
  l'application qui l'ouvre ; c'est Windows qui détient ce choix.

## Success Criteria *(mandatory)*

### Measurable Outcomes

Les durées sont mesurées sur l'ordinateur de référence du projet (portable
à processeur basse consommation, disque SSD).

- **SC-001**: Entre le double-clic dans l'Explorateur et l'affichage complet
  d'une photo JPEG de 12 à 24 Mpx, il s'écoule moins d'une seconde
  (visionneuse fermée, ordinateur démarré depuis quelques minutes) et moins
  de 2 secondes au tout premier lancement après le démarrage de Windows.
- **SC-002**: Ce délai d'ouverture n'augmente pas de plus de 10 % quand la
  photo est dans un dossier de 5 000 images, et tout le dossier est
  parcourable moins de 3 secondes après l'ouverture.
- **SC-003**: Un appui sur → déclenche la transition en moins de 0,1 seconde,
  et la photo suivante (12 à 24 Mpx) est nette au terme de la transition.
- **SC-004**: Après une rafale de 20 appuis sur → en 2 secondes, la
  visionneuse affiche la 20e photo moins d'une demi-seconde après le dernier
  appui.
- **SC-005**: Les transitions, le zoom et le déplacement se font sans
  à-coup perceptible, à la fréquence de l'écran, sur des photos de 24 Mpx.
- **SC-006**: Pendant un zoom à la molette, le point de l'image situé sous
  le curseur s'en écarte de moins de 2 pixels.
- **SC-007**: Après n'importe quelle utilisation de la visionneuse, aucun
  fichier du dossier consulté n'a été modifié, déplacé, renommé ou créé, la
  bibliothèque est inchangée, et la visionneuse a écrit moins de 100 Ko sur
  le disque au total (vérifiable par instantané avant/après).
- **SC-008**: Après l'installation, et après un refus de la proposition
  d'association, les applications par défaut de Windows sont exactement les
  mêmes qu'avant.
- **SC-009**: En parcourant d'un bout à l'autre un dossier de 1 000 photos
  de 24 Mpx, la visionneuse n'occupe jamais plus de 500 Mo de mémoire.
- **SC-010**: Un ancien utilisateur de Picasa ouvre une photo depuis
  l'Explorateur, zoome sur un détail, parcourt le dossier et revient à la
  bibliothèque, sans explication, en moins d'une minute.

## Assumptions

- Application de bureau pour Windows 10 et 11 ; interface en français ;
  tout se passe hors ligne.
- La 005 dépend uniquement de la 001, dont elle réutilise les formats, le
  décodage, l'orientation, le zoom et le panneau d'informations. Elle peut
  être réalisée avant 002, 003 et 004.
- Picazoo est installé par un programme d'installation (ou un équivalent),
  qui est l'endroit naturel pour l'inscrire auprès de Windows. Windows 10 et
  11 ne laissent pas une application se déclarer d'office comme
  application par défaut : l'utilisateur confirme toujours dans les
  réglages de Windows, ce qui va dans le sens du « jamais imposé ».
- **Ordre du parcours** : ordre naturel des noms, quel que soit le tri
  choisi dans la fenêtre de l'Explorateur. Suivre le tri de l'Explorateur
  est un choix possible, à confirmer.
- **Vidéos** : hors de la visionneuse autonome ; elles ne sont ni
  parcourues ni associées à la visionneuse, et restent lues dans
  l'application complète (001).
- **Boucle** : après la dernière photo, retour à la première (comme
  Picasa), à confirmer.
- **Molette** : elle zoome toujours (comme demandé) ; elle ne sert pas à
  changer de photo. Les flèches, les boutons et les bords de l'image s'en
  chargent.
- **Présentation** : le voile semi-transparent sur le bureau, comme la
  Visionneuse de photos Picasa (tranché le 2026-11-22) ; pas de fenêtre
  redimensionnable classique. Un clic sur le voile hors de la photo ne
  ferme pas la visionneuse, pour éviter les fermetures accidentelles
  pendant un déplacement.
- Pas de suppression, de rotation, de retouche, de diaporama, d'impression
  ni d'envoi dans la 005 : la visionneuse ne fait que regarder. La rotation
  ou la suppression toucheraient aux fichiers et passeront, plus tard, par
  les règles du principe I.
- Une seule visionneuse autonome à la fois (FR-004).

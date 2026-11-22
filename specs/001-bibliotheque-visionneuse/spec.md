# Feature Specification: Bibliothèque et visionneuse

**Feature Branch**: `001-bibliotheque-visionneuse`

**Created**: 2026-11-22

**Status**: Draft

**Input**: User description: "Refaire Picasa, le lecteur d'images et de vidéos qui n'est plus maintenu, de la façon dont il était fait, avec en plus la détection des doublons, la détection des images liées par leur nom et la réorganisation de la bibliothèque (feuille de route du 2026-11-22). Fonctionnalité 001 : la base Picasa — retrouver les photos et vidéos de l'ordinateur, les afficher par dossier en vignettes, les ouvrir en grand, lire les vidéos, et garder la bibliothèque à jour toute seule."

## Clarifications

### Session 2026-11-22

- Q: Les photos RAW des appareils photo (CR2, NEF, ARW, DNG…) sont-elles prises en charge dans la 001 ? → A: Oui, en affichant l'aperçu intégré au fichier RAW (pas de développement complet du RAW).
- Q: Comment présenter la liste des dossiers à gauche ? → A: Les deux : liste à plat regroupée par année par défaut (comme Picasa), avec une bascule vers l'arborescence du disque.

## Contexte

Avec Picasa, l'utilisateur n'importait rien : il indiquait quels dossiers
surveiller, et le logiciel trouvait tout seul toutes les photos et vidéos qui
s'y trouvaient. La fenêtre montrait à gauche la liste des dossiers contenant
des images, à droite les vignettes de tous ces dossiers à la suite, dans un
défilement continu. Un double-clic ouvrait l'image en grand, les flèches
passaient à la suivante, et la bibliothèque se mettait à jour d'elle-même
quand on ajoutait des photos sur le disque.

Cette première fonctionnalité reproduit ce socle. Les doublons (002), les
images liées (003) et la réorganisation (004) s'appuieront dessus : tant que
Picazoo ne sait pas lister, indexer et montrer la bibliothèque, aucune des
trois ne peut exister.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Retrouver toutes ses photos sans rien importer (Priority: P1)

Au premier lancement, Picazoo propose de surveiller les emplacements habituels
(Images, Bureau, Documents) ou de choisir soi-même les dossiers. L'utilisateur
valide ; Picazoo parcourt ces dossiers et leurs sous-dossiers, et les dossiers
contenant des photos ou des vidéos apparaissent au fur et à mesure dans la
liste de gauche, avec le nombre d'éléments et la date. À droite, les
vignettes de tous les dossiers s'affichent à la suite, chaque dossier
précédé de son nom. Cliquer sur un dossier à gauche fait défiler jusqu'à lui.
Plus tard, depuis un écran « Dossiers surveillés », l'utilisateur peut ajouter
un dossier, en retirer un ou exclure un sous-dossier.

**Why this priority**: c'est le cœur de Picasa et la condition de tout le
reste — sans bibliothèque, pas de doublons, pas de groupes, pas de
réorganisation.

**Independent Test**: préparer un dossier de test contenant 3 sous-dossiers
avec des photos et une vidéo, plus un sous-dossier sans image ; lancer
Picazoo pour la première fois, choisir ce dossier ; les 3 sous-dossiers
apparaissent (pas le vide), avec les bonnes vignettes et les bons nombres.

**Acceptance Scenarios**:

1. **Given** un premier lancement, **When** l'utilisateur accepte les
   emplacements proposés, **Then** Picazoo commence l'analyse et affiche les
   premiers dossiers trouvés en quelques secondes, sans attendre la fin.
2. **Given** une analyse en cours, **When** l'utilisateur fait défiler,
   ouvre une image ou change de dossier, **Then** l'application répond
   normalement et une indication discrète montre la progression.
3. **Given** un dossier surveillé contenant un sous-dossier exclu, **When**
   l'analyse se termine, **Then** aucune image de ce sous-dossier n'apparaît.
4. **Given** un dossier retiré des dossiers surveillés, **When** l'utilisateur
   valide, **Then** ses images disparaissent de la bibliothèque et **aucun
   fichier n'est touché sur le disque**.
5. **Given** une bibliothèque déjà analysée, **When** l'utilisateur relance
   Picazoo, **Then** elle s'affiche immédiatement, sans nouvelle analyse
   complète.

---

### User Story 2 - Voir une photo en grand et passer d'une photo à l'autre (Priority: P1)

L'utilisateur double-clique sur une vignette : l'image s'ouvre en grand dans
la fenêtre, ajustée à l'écran, sur fond sombre. Les flèches du clavier (ou
des boutons à l'écran) passent à l'image précédente ou suivante du même
dossier. Il peut zoomer (molette, boutons) et se déplacer dans l'image
zoomée, voir les informations de la photo (nom, date de prise de vue,
dimensions, poids, appareil), passer en plein écran, ou demander à ouvrir
l'emplacement du fichier dans l'Explorateur. Échap ou un bouton « Retour à la
bibliothèque » le ramène à la vignette d'où il était parti.

**Why this priority**: c'est l'autre moitié du « lecteur d'images » ; une
bibliothèque qu'on ne peut pas regarder n'a pas d'intérêt.

**Independent Test**: dans un dossier de 10 photos, ouvrir la 3e, appuyer
4 fois sur → (on est sur la 7e), zoomer, appuyer sur Échap ; on revient à la
bibliothèque avec la 7e vignette visible et sélectionnée.

**Acceptance Scenarios**:

1. **Given** une vignette, **When** l'utilisateur double-clique dessus ou
   appuie sur Entrée, **Then** l'image s'ouvre en grand, entière et non
   déformée, en moins d'une seconde pour une photo d'appareil courante.
2. **Given** la dernière image d'un dossier, **When** l'utilisateur appuie
   sur →, **Then** Picazoo passe à la première image du dossier suivant.
3. **Given** une photo prise en tenant l'appareil verticalement, **When**
   elle est affichée (vignette ou en grand), **Then** elle apparaît dans le
   bon sens.
4. **Given** une image affichée en grand, **When** l'utilisateur ouvre le
   panneau d'informations, **Then** il voit le nom, le chemin, la date de
   prise de vue (ou à défaut la date du fichier), les dimensions, le poids et
   l'appareil quand ils sont connus.

---

### User Story 3 - Regarder ses vidéos dans Picazoo (Priority: P2)

Les vidéos apparaissent dans la bibliothèque comme les photos, avec une
vignette tirée de la vidéo, un symbole qui les distingue et leur durée.
Ouvertes en grand, elles se lisent directement dans Picazoo : lecture/pause,
barre de progression pour se déplacer, volume, plein écran. Les flèches
passent à l'élément précédent ou suivant comme pour les photos.

**Why this priority**: Picasa était aussi un lecteur de vidéos et
l'utilisateur le demande explicitement, mais la bibliothèque de photos seule
apporte déjà la valeur principale.

**Independent Test**: dans un dossier contenant 2 photos et une vidéo MP4 de
30 secondes, la vidéo a une vignette avec sa durée ; double-clic, elle se lit
avec le son ; on avance à la moitié, on met en pause, → passe à la photo
suivante.

**Acceptance Scenarios**:

1. **Given** une vidéo dans un dossier surveillé, **When** la bibliothèque
   l'affiche, **Then** sa vignette est une image de la vidéo, marquée comme
   vidéo, avec sa durée.
2. **Given** une vidéo ouverte en grand, **When** l'utilisateur lance la
   lecture, **Then** elle se lit avec le son, et il peut mettre en pause, se
   déplacer, régler le volume et passer en plein écran.
3. **Given** une vidéo dans un format que Picazoo ne sait pas lire, **When**
   l'utilisateur l'ouvre, **Then** un message clair le dit et propose de
   l'ouvrir avec le lecteur par défaut du système.

---

### User Story 4 - Une bibliothèque qui se tient à jour toute seule (Priority: P2)

Pendant que Picazoo est ouvert, l'utilisateur copie des photos d'un téléphone
dans un dossier surveillé : elles apparaissent d'elles-mêmes, sans rien
faire. S'il supprime ou renomme des fichiers dans l'Explorateur, la
bibliothèque suit. Si Picazoo était fermé pendant ces changements, il les
rattrape au démarrage suivant.

**Why this priority**: c'est ce qui rendait Picasa « magique » au quotidien ;
mais une analyse manuelle (« Actualiser ») suffirait pour démarrer.

**Independent Test**: Picazoo ouvert sur un dossier de test ; y copier
5 photos, en supprimer 1, en renommer 1 dans l'Explorateur ; en moins de
10 secondes la bibliothèque montre les 5 nouvelles, plus la supprimée, et la
renommée sous son nouveau nom.

**Acceptance Scenarios**:

1. **Given** Picazoo ouvert, **When** des photos sont ajoutées dans un
   dossier surveillé, **Then** elles apparaissent dans la bibliothèque en
   moins de 10 secondes.
2. **Given** Picazoo fermé, **When** des photos sont ajoutées, supprimées ou
   déplacées entre dossiers surveillés, puis Picazoo relancé, **Then** la
   bibliothèque reflète l'état réel du disque après une courte vérification
   en arrière-plan.
3. **Given** une photo renommée ou déplacée d'un dossier surveillé à un
   autre, **When** Picazoo la retrouve, **Then** elle est reconnue comme la
   même photo (ses informations déjà calculées ne sont pas perdues).
4. **Given** un dossier surveillé sur un disque externe débranché, **When**
   l'utilisateur ouvre Picazoo, **Then** ce dossier apparaît comme
   indisponible au lieu de disparaître, et redevient normal quand le disque
   est rebranché.

---

### User Story 5 - Parcourir à sa façon (Priority: P3)

L'utilisateur règle la taille des vignettes avec un curseur, choisit l'ordre
des dossiers (par date, par nom) et l'ordre des images dans un dossier (par
date de prise de vue, par nom), et ces choix sont retenus d'une session à
l'autre. Il peut sélectionner plusieurs vignettes (Ctrl, Maj, glisser) en
vue des fonctionnalités suivantes.

**Why this priority**: confort de navigation ; la bibliothèque est utilisable
sans.

**Independent Test**: passer les vignettes au plus grand, trier les dossiers
par nom, basculer la liste en arborescence, fermer et rouvrir Picazoo : les
trois réglages sont conservés.

**Acceptance Scenarios**:

1. **Given** la bibliothèque, **When** l'utilisateur déplace le curseur de
   taille, **Then** toutes les vignettes changent de taille immédiatement.
2. **Given** un tri choisi, **When** Picazoo est relancé, **Then** le même
   tri est appliqué.
3. **Given** la liste des dossiers par année avec un dossier sélectionné,
   **When** l'utilisateur bascule en arborescence, **Then** l'arborescence
   s'ouvre jusqu'à ce dossier, qui reste sélectionné.
4. **Given** des vignettes, **When** l'utilisateur clique sur l'une puis
   Maj+clic sur une autre, **Then** toutes celles entre les deux sont
   sélectionnées et leur nombre est affiché.

---

### Edge Cases

- **Fichier illisible ou corrompu** : une vignette « image illisible » le
  signale, sans bloquer l'analyse ni le reste de la bibliothèque.
- **Très grande image** (panorama de 100 mégapixels) : elle s'ouvre, quitte
  à apparaître d'abord en basse définition puis se préciser.
- **Dossier sans droit d'accès** : il est ignoré et signalé dans l'écran des
  dossiers surveillés, pas de message bloquant pendant l'analyse.
- **Dossiers imbriqués** : un dossier surveillé à l'intérieur d'un autre ne
  produit pas d'images en double dans la bibliothèque.
- **Raccourcis et liens symboliques** : ne sont pas suivis, pour éviter les
  boucles et les doubles comptes.
- **Fichiers système et miniatures** (`Thumbs.db`, dossiers cachés, caches
  d'autres logiciels) : ignorés.
- **Photo sans date de prise de vue** : la date du fichier est utilisée.
- **Photo RAW accompagnée de son JPEG** (`IMG_0042.CR2` + `IMG_0042.JPG`) :
  les deux apparaissent comme deux éléments distincts dans la 001 ; leur
  rapprochement relève des images liées (003).
- **Chemin très long ou caractères spéciaux** (accents, emoji) : la photo
  s'affiche normalement.
- **Disque plein ou stockage de Picazoo inaccessible** : message clair,
  aucune perte des données déjà enregistrées.
- **Fichier encore en cours de copie** : il n'apparaît (ou ne prend sa vraie
  vignette) qu'une fois la copie terminée.

## Requirements *(mandatory)*

### Functional Requirements

**Dossiers surveillés et analyse**

- **FR-001**: Au premier lancement, le système MUST proposer de surveiller
  les dossiers Images, Bureau et Documents de l'utilisateur, ou de choisir
  d'autres dossiers.
- **FR-002**: Les utilisateurs MUST pouvoir ajouter et retirer des dossiers
  surveillés, et exclure des sous-dossiers, à tout moment.
- **FR-003**: Le système MUST trouver, dans les dossiers surveillés et leurs
  sous-dossiers, toutes les images aux formats JPEG, PNG, GIF, BMP, TIFF,
  WebP et HEIC, et toutes les vidéos aux formats MP4, MOV, AVI et MKV.
- **FR-004**: Le système MUST trouver les photos RAW des appareils photo
  (au moins CR2, CR3, NEF, ARW, ORF, RW2, RAF et DNG) et les afficher, en
  vignette comme en grand, à partir de l'aperçu intégré au fichier. Un RAW
  sans aperçu intégré exploitable MUST apparaître avec une vignette
  « aperçu indisponible » et l'option de l'ouvrir avec l'application par
  défaut du système (FR-022). Le développement complet du RAW est hors
  périmètre.
- **FR-005**: L'analyse MUST se faire en arrière-plan, afficher les dossiers
  au fur et à mesure, montrer sa progression, pouvoir être mise en pause et
  reprendre où elle s'était arrêtée après une fermeture de l'application.
- **FR-006**: Le système MUST conserver l'index de la bibliothèque et les
  vignettes entre deux lancements, pour s'afficher sans nouvelle analyse
  complète.
- **FR-007**: Le système MUST détecter les ajouts, suppressions, renommages
  et déplacements de fichiers dans les dossiers surveillés, pendant qu'il est
  ouvert et au démarrage pour ce qui s'est passé pendant sa fermeture.
- **FR-008**: Le système MUST reconnaître un fichier renommé ou déplacé
  entre dossiers surveillés comme le même élément, sans perdre les
  informations déjà calculées à son sujet.
- **FR-009**: Le système MUST signaler comme « indisponible » un dossier
  surveillé temporairement inaccessible (disque débranché, partage réseau
  absent) plutôt que de retirer ses éléments.
- **FR-010**: Le système MUST NOT modifier, déplacer, renommer ni supprimer
  aucun fichier de l'utilisateur, et MUST NOT écrire de fichier dans ses
  dossiers.

**Bibliothèque**

- **FR-011**: La bibliothèque MUST présenter les dossiers contenant au moins
  une image ou vidéo, avec leur nom, leur nombre d'éléments et leur date,
  selon deux modes entre lesquels l'utilisateur bascule à tout moment :
  - **Par année** (mode par défaut, comme Picasa) : liste à plat des
    dossiers, regroupés sous des intitulés d'année selon leur date ;
  - **Arborescence** : les dossiers tels qu'ils sont rangés sur le disque,
    à partir des dossiers surveillés, dépliables, en ne montrant que les
    branches qui mènent à des éléments.

  Le mode choisi MUST être conservé d'une session à l'autre, et le dossier
  sélectionné MUST rester sélectionné quand on change de mode.
- **FR-012**: La zone des vignettes MUST afficher les éléments de tous les
  dossiers dans un défilement continu, chaque dossier précédé d'un en-tête ;
  sélectionner un dossier dans la liste MUST faire défiler jusqu'à lui.
- **FR-013**: Les vignettes MUST respecter l'orientation de la photo et
  distinguer visuellement les vidéos (symbole et durée).
- **FR-014**: Les utilisateurs MUST pouvoir régler la taille des vignettes,
  choisir le tri des dossiers (date, nom) et des éléments (date de prise de
  vue, nom) ; ces réglages MUST être conservés.
- **FR-015**: Les utilisateurs MUST pouvoir sélectionner un ou plusieurs
  éléments (clic, Ctrl+clic, Maj+clic, sélection au cadre).

**Visionneuse**

- **FR-016**: Un double-clic ou Entrée sur une vignette MUST ouvrir l'élément
  en grand, ajusté à la fenêtre, dans la bonne orientation.
- **FR-017**: La visionneuse MUST permettre de passer à l'élément précédent
  et suivant (clavier et boutons), en continuant d'un dossier au suivant.
- **FR-018**: La visionneuse MUST permettre de zoomer, de se déplacer dans
  l'image zoomée et de revenir à l'ajustement à la fenêtre.
- **FR-019**: La visionneuse MUST pouvoir passer en plein écran et en
  sortir.
- **FR-020**: La visionneuse MUST afficher sur demande les informations de
  l'élément : nom, chemin, date de prise de vue, dimensions, poids, appareil
  et, pour une vidéo, durée.
- **FR-021**: La visionneuse MUST lire les vidéos avec le son et proposer
  lecture/pause, déplacement dans la vidéo, volume et plein écran.
- **FR-022**: Pour un élément que Picazoo ne sait pas afficher ou lire, le
  système MUST le dire clairement et proposer de l'ouvrir avec l'application
  par défaut du système.
- **FR-023**: Les utilisateurs MUST pouvoir ouvrir l'emplacement d'un
  élément dans l'Explorateur de fichiers, depuis la bibliothèque et depuis
  la visionneuse.
- **FR-024**: Quitter la visionneuse MUST ramener à la bibliothèque, sur la
  vignette du dernier élément regardé.

### Key Entities

- **Dossier surveillé** : un emplacement choisi par l'utilisateur, analysé
  avec ses sous-dossiers ; peut avoir des sous-dossiers exclus ; état
  disponible ou indisponible.
- **Dossier** : un dossier du disque qui contient au moins un élément ; nom,
  chemin, nombre d'éléments, date (celle de son élément le plus ancien).
- **Élément** : une photo ou une vidéo de la bibliothèque ; chemin, nom,
  type, dates (prise de vue, fichier), dimensions, poids, orientation,
  appareil, durée (vidéo), empreinte permettant de le reconnaître s'il est
  renommé ou déplacé. Base des fonctionnalités 002 à 004.
- **Vignette** : l'aperçu réduit d'un élément, conservé par Picazoo dans son
  propre stockage.
- **Préférences d'affichage** : taille des vignettes, tris, mode de la liste
  des dossiers (par année ou arborescence), dernier dossier consulté.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Sur une bibliothèque de 20 000 photos, les premiers dossiers
  apparaissent moins de 5 secondes après le choix des dossiers, et l'analyse
  complète, vignettes comprises, se termine en moins de 15 minutes sur un
  ordinateur portable courant.
- **SC-002**: Une bibliothèque déjà analysée de 100 000 éléments s'affiche
  en moins de 3 secondes au lancement.
- **SC-003**: Le défilement des vignettes reste fluide (aucun à-coup
  perceptible) sur une bibliothèque de 100 000 éléments, y compris pendant
  une analyse.
- **SC-004**: Une photo d'appareil courante (12 à 24 mégapixels) s'ouvre en
  grand en moins d'une seconde, et passer à la suivante prend moins d'une
  demi-seconde.
- **SC-005**: Une photo ajoutée dans un dossier surveillé apparaît en moins
  de 10 secondes pendant que Picazoo est ouvert.
- **SC-006**: Après n'importe quelle utilisation de cette fonctionnalité,
  aucun fichier des dossiers surveillés n'a été modifié, déplacé, renommé ou
  créé (vérifiable en comparant le contenu des dossiers avant et après).
- **SC-007**: Un ancien utilisateur de Picasa retrouve une photo précise de
  sa bibliothèque et l'ouvre en grand, sans explication, en moins d'une
  minute.

## Assumptions

- Application de bureau pour Windows 10 et 11 en premier ; utilisateur seul
  sur sa machine, pas de compte ni de connexion Internet.
- Bibliothèque visée : jusqu'à 100 000 photos et vidéos.
- L'interface est en français.
- Pas d'import depuis un appareil photo ou un téléphone dans la 001 :
  l'utilisateur copie ses fichiers comme il le fait aujourd'hui, Picazoo les
  trouve.
- Pas de suppression, de renommage ni de retouche dans la 001 : la mise à la
  corbeille arrivera avec les doublons (002), les retouches plus tard.
- Pas d'étoiles, mots-clés, albums, recherche, diaporama ni visages dans la
  001 (voir les pistes de la feuille de route).
- Les raccourcis et liens symboliques ne sont pas suivis.
- Le choix initial « Images, Bureau, Documents » reprend la proposition de
  Picasa ; analyser le disque entier reste possible en ajoutant sa racine
  comme dossier surveillé.

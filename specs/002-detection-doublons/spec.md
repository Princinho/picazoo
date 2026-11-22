# Feature Specification: Détection des doublons

**Feature Branch**: `002-detection-doublons`

**Created**: 2026-11-22

**Status**: Draft

**Input**: User description: "Fonctionnalité 002 de la feuille de route Picazoo : détection des doublons de photos et vidéos. Retrouver les doublons exacts (même contenu, par empreinte/hachage) et les quasi-doublons (même image redimensionnée, recompressée, convertie dans un autre format ou légèrement retouchée, par empreinte perceptuelle), avec un niveau de confiance compréhensible. Regrouper les doublons en groupes. Un écran de revue permet de parcourir les groupes, de comparer les copies côte à côte (dimensions, poids, format, date, dossier) et de choisir quelle copie garder, avec une suggestion automatique de la meilleure copie que l'utilisateur valide ou corrige ; ses choix (y compris « ce ne sont pas des doublons ») sont retenus. Suppression sûre : les copies écartées vont à la corbeille du système, avec aperçu avant d'agir, journal et confirmation ; jamais de suppression définitive. Le calcul est incrémental et tourne en arrière-plan : il ne fige pas l'interface et ne sature pas le disque (lectures limitées en débit, à faible priorité, reprise possible après fermeture), il s'appuie sur l'index de la bibliothèque (001) et ne recalcule que les éléments nouveaux ou modifiés."

## Contexte

Au fil des années, une bibliothèque de photos se remplit de copies : la même
photo importée deux fois du téléphone, une sauvegarde recopiée dans un autre
dossier, une version réduite envoyée par e-mail puis réenregistrée, un PNG
tiré d'un JPEG, une copie dont on a juste remonté la luminosité. Picasa
savait à peine repérer les copies strictement identiques ; Picazoo doit
retrouver aussi les copies de la même image dans une autre taille, un autre
format ou une autre compression, et aider l'utilisateur à faire le ménage
sans jamais risquer de perdre une photo.

La fonctionnalité s'appuie sur la bibliothèque de la 001 : les éléments
indexés, leur identifiant stable (qui survit aux renommages et aux
déplacements) et leurs vignettes. Elle ajoute la première opération de
Picazoo qui touche aux fichiers de l'utilisateur — la mise à la corbeille —
et doit donc appliquer à la lettre le principe I de la constitution : rien
ne bouge sans action explicite et confirmée, tout est montré avant, journalisé
et annulable.

Le calcul des doublons demande de lire les fichiers. Il ne doit ni figer
l'interface, ni saturer le disque au point de ralentir l'ordinateur : c'est
un travail de fond, discret, qui cède toujours la priorité à l'utilisateur.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Retrouver les copies identiques et les voir regroupées (Priority: P1)

Une fois la bibliothèque analysée, Picazoo cherche en arrière-plan les
fichiers au contenu strictement identique. Une entrée « Doublons » apparaît
en haut de la liste des dossiers, avec le nombre de groupes trouvés et la
place qu'ils occupent. En l'ouvrant, l'utilisateur voit la liste des groupes :
chaque groupe montre les copies côte à côte, avec pour chacune sa vignette,
son nom, son dossier, ses dimensions, son poids, son format et sa date. Le
niveau de confiance « Identique » est affiché clairement. Un double-clic sur
une copie l'ouvre en grand dans la visionneuse.

**Why this priority**: c'est le cas le plus fréquent (imports et sauvegardes
en double) et le plus sûr (aucune estimation) ; il apporte à lui seul un
gain de place visible.

**Independent Test**: dans un dossier de test, placer une photo `A.jpg` et
deux copies identiques renommées dans deux autres sous-dossiers, plus
10 photos différentes ; après l'analyse, l'entrée « Doublons » indique
1 groupe de 3 copies, et aucune des 10 autres photos n'y figure.

**Acceptance Scenarios**:

1. **Given** une bibliothèque contenant trois copies identiques d'une même
   photo dans des dossiers différents, **When** l'analyse des doublons se
   termine, **Then** un groupe de 3 copies apparaît, marqué « Identique ».
2. **Given** deux photos différentes de même taille de fichier, **When**
   l'analyse se termine, **Then** elles ne sont pas regroupées.
3. **Given** l'écran des doublons ouvert, **When** l'utilisateur regarde un
   groupe, **Then** il voit pour chaque copie la vignette, le nom, le
   dossier, les dimensions, le poids, le format et la date, et la place
   récupérable si on ne garde qu'une copie.
4. **Given** aucun doublon dans la bibliothèque, **When** l'utilisateur
   ouvre l'entrée « Doublons », **Then** un message indique qu'aucun doublon
   n'a été trouvé (et, si l'analyse n'est pas finie, qu'elle continue).

---

### User Story 2 - Choisir quoi garder et mettre le reste à la corbeille, sans risque (Priority: P1)

Dans chaque groupe, Picazoo propose la copie à garder (marquée d'une étoile)
et coche les autres pour la corbeille. L'utilisateur peut changer ce choix,
garder plusieurs copies, ou n'en écarter aucune. Le bouton « Mettre à la
corbeille… » ouvre un récapitulatif : la liste exacte des fichiers qui
partiront, leur emplacement, la place libérée, et la copie gardée pour chaque
groupe. Après confirmation, les fichiers vont à la corbeille du système, la
bibliothèque se met à jour, et l'opération est inscrite au journal. Depuis le
journal, l'utilisateur peut l'annuler : les fichiers reviennent à leur
emplacement d'origine.

**Why this priority**: retrouver les doublons sans pouvoir s'en débarrasser
n'a qu'un intérêt limité ; et c'est l'étape où une erreur coûterait une
photo, d'où les garde-fous.

**Independent Test**: sur le groupe de 3 copies de l'US1, accepter la
suggestion et confirmer ; 2 fichiers sont dans la corbeille du système, 1 est
resté en place, la bibliothèque n'en montre plus qu'un ; « Annuler » dans le
journal remet les 2 fichiers à leur place d'origine, identiques.

**Acceptance Scenarios**:

1. **Given** un groupe affiché, **When** l'utilisateur l'ouvre, **Then** une
   copie est proposée à garder et les autres sont cochées pour la corbeille,
   avec la raison du choix (par exemple « plus grande résolution »).
2. **Given** des copies cochées dans un ou plusieurs groupes, **When**
   l'utilisateur demande la mise à la corbeille, **Then** un récapitulatif
   liste chaque fichier concerné et rien ne se passe tant qu'il n'a pas
   confirmé.
3. **Given** un groupe où l'utilisateur a décoché la copie gardée, **When**
   toutes ses copies se retrouvent cochées pour la corbeille, **Then**
   Picazoo refuse et demande d'en garder au moins une.
4. **Given** une mise à la corbeille confirmée, **When** elle s'exécute,
   **Then** les fichiers sont dans la corbeille du système (jamais effacés
   définitivement), la bibliothèque ne les montre plus, et le journal
   indique pour chacun la date, l'emplacement d'origine et le résultat.
5. **Given** un fichier modifié, déplacé ou disparu depuis l'analyse, **When**
   la mise à la corbeille s'exécute, **Then** ce fichier n'est pas touché,
   son groupe est réanalysé, et le récapitulatif final le signale.
6. **Given** une opération inscrite au journal, **When** l'utilisateur
   l'annule, **Then** les fichiers encore présents dans la corbeille
   reviennent à leur emplacement d'origine, et ceux qui n'y sont plus sont
   signalés.

---

### User Story 3 - Retrouver la même image en une autre taille, un autre format ou retouchée (Priority: P2)

En plus des copies identiques, Picazoo retrouve les copies d'une même image
qui ne sont pas identiques octet pour octet : réduite pour un envoi,
réenregistrée avec une autre compression, convertie (JPEG, PNG, HEIC, WebP…),
tournée, ou légèrement retouchée (luminosité, contraste, couleurs). Chaque
groupe affiche un niveau de confiance en mots : « Identique », « Même image »
(autre taille, format ou compression) ou « Très ressemblante » (retouche
probable, à vérifier). L'utilisateur peut ouvrir deux copies côte à côte en
grand, avec un zoom synchronisé, pour juger de la qualité de chacune.

**Why this priority**: c'est ce que Picasa ne savait pas faire et ce que
l'utilisateur demande explicitement (« même en résolutions différentes ») ;
mais les copies identiques apportent déjà une première valeur.

**Independent Test**: à partir d'une photo de 12 Mpx, produire une copie
réduite de moitié, une copie recompressée en qualité basse, une copie PNG,
une copie tournée de 90° et une copie à la luminosité remontée de 10 % ;
ajouter 20 photos différentes, dont 5 prises en rafale de la même scène ;
après l'analyse, un seul groupe réunit l'original et ses 5 copies, et les
photos de la rafale n'y figurent pas.

**Acceptance Scenarios**:

1. **Given** une photo et sa copie réduite de moitié, **When** l'analyse se
   termine, **Then** elles sont regroupées avec le niveau « Même image ».
2. **Given** une photo et sa copie légèrement éclaircie, **When** l'analyse
   se termine, **Then** elles sont regroupées avec le niveau « Très
   ressemblante ».
3. **Given** plusieurs photos d'une même scène prises à quelques secondes
   d'intervalle, **When** l'analyse se termine, **Then** elles ne sont pas
   proposées comme doublons.
4. **Given** un groupe « Même image », **When** Picazoo suggère la copie à
   garder, **Then** il propose la copie de plus grande résolution, et à
   résolution égale la moins compressée.
5. **Given** deux copies d'un groupe, **When** l'utilisateur les ouvre côte
   à côte et zoome sur l'une, **Then** l'autre suit le même zoom et le même
   cadrage.
6. **Given** la liste des groupes, **When** l'utilisateur filtre par niveau
   de confiance, **Then** seuls les groupes du niveau choisi sont affichés.

---

### User Story 4 - Une analyse discrète qui ne gêne jamais l'ordinateur (Priority: P2)

L'analyse des doublons tourne en fond, après l'indexation de la bibliothèque.
L'utilisateur continue d'utiliser Picazoo et son ordinateur normalement : le
défilement reste fluide, les autres logiciels ne ralentissent pas, le disque
n'est jamais saturé. Une indication discrète montre la progression ; il peut
mettre l'analyse en pause. S'il ferme Picazoo, l'analyse reprend au lancement
suivant là où elle s'était arrêtée. Quand des photos sont ajoutées ou
modifiées, seules celles-ci sont analysées, et leurs doublons éventuels
apparaissent peu après.

**Why this priority**: sans cela la fonctionnalité serait inutilisable sur
une vraie bibliothèque (principe III), mais un premier calcul sur une petite
bibliothèque de test suffit pour valider les US1 à US3.

**Independent Test**: sur une bibliothèque de test de 5 000 photos, lancer
l'analyse ; pendant qu'elle tourne, faire défiler la bibliothèque et copier
un gros fichier avec l'Explorateur sur le même disque ; le défilement reste
fluide et la copie n'est pas sensiblement plus lente qu'à l'arrêt ; fermer
Picazoo à mi-parcours puis le relancer : la progression reprend au même
point.

**Acceptance Scenarios**:

1. **Given** une analyse des doublons en cours, **When** l'utilisateur fait
   défiler, ouvre une photo ou une vidéo, **Then** Picazoo répond aussi vite
   que sans analyse.
2. **Given** une analyse en cours, **When** une autre application lit ou
   écrit sur le même disque, **Then** l'analyse ralentit d'elle-même pour lui
   laisser la priorité.
3. **Given** une analyse interrompue par la fermeture de Picazoo, **When**
   l'utilisateur le relance, **Then** l'analyse reprend sans refaire le
   travail déjà fait.
4. **Given** une bibliothèque déjà analysée, **When** l'utilisateur ajoute
   une copie d'une photo existante dans un dossier surveillé, **Then** le
   groupe correspondant apparaît ou s'enrichit en moins d'une minute, sans
   réanalyser le reste de la bibliothèque.
5. **Given** une analyse en cours, **When** l'utilisateur la met en pause,
   **Then** Picazoo cesse toute lecture liée aux doublons jusqu'à la reprise.

---

### User Story 5 - Des décisions retenues et un ménage rapide (Priority: P3)

L'utilisateur peut dire d'un groupe, ou d'une copie dans un groupe, « Ce ne
sont pas des doublons » : Picazoo ne le lui reproposera plus, même après une
nouvelle analyse. Il peut aussi décider de garder volontairement toutes les
copies d'un groupe (« Garder toutes ») pour ne plus le voir. Pour les
grandes bibliothèques, il peut appliquer en une fois les suggestions à tous
les groupes « Identique », avec le même récapitulatif et la même
confirmation qu'en US2. Depuis la visionneuse ou la bibliothèque, une photo
qui a des doublons le signale et permet d'ouvrir son groupe.

**Why this priority**: confort et rapidité ; le ménage groupe par groupe des
US1 à US3 est déjà complet et sûr.

**Independent Test**: marquer un groupe « Ce ne sont pas des doublons »,
fermer et relancer Picazoo, forcer une nouvelle analyse : le groupe
n'apparaît plus. Sur 20 groupes « Identique », appliquer les suggestions en
une fois : le récapitulatif liste les 20 groupes et, après confirmation, une
copie de chacun reste en place.

**Acceptance Scenarios**:

1. **Given** un groupe marqué « Ce ne sont pas des doublons », **When** une
   nouvelle analyse a lieu, **Then** ces éléments ne sont plus regroupés
   entre eux.
2. **Given** une copie retirée d'un groupe par l'utilisateur, **When** le
   groupe est réaffiché, **Then** elle n'y figure plus et les autres copies
   restent regroupées.
3. **Given** plusieurs groupes « Identique », **When** l'utilisateur applique
   les suggestions à tous, **Then** un récapitulatif unique liste tous les
   fichiers concernés, et rien ne se passe sans confirmation.
4. **Given** une photo qui a des doublons, **When** l'utilisateur l'affiche
   dans la visionneuse, **Then** le panneau d'informations indique le nombre
   de doublons et permet d'ouvrir le groupe.
5. **Given** une décision de l'utilisateur sur un groupe, **When** une des
   copies est renommée ou déplacée sur le disque, **Then** la décision est
   conservée.

---

### Edge Cases

- **Copies sur un disque sans corbeille** (partage réseau, certaines clés
  USB) : Picazoo ne peut pas les mettre à la corbeille ; il ne les efface
  **jamais** définitivement, les signale dans le récapitulatif comme « non
  supprimables par Picazoo » et propose d'ouvrir leur emplacement.
- **Dossier surveillé indisponible** (disque débranché) : ses éléments
  restent dans leurs groupes, marqués indisponibles ; ils ne peuvent pas être
  mis à la corbeille tant que le disque est absent.
- **Fichier en lecture seule ou verrouillé** par une autre application : il
  n'est pas touché, et l'échec est indiqué dans le récapitulatif et le
  journal.
- **Liens physiques** (deux chemins vers le même fichier sur le disque) : ils
  ne sont pas proposés comme doublons, puisque supprimer l'un ne libère rien
  et ne retire pas l'autre.
- **Fichiers « disponibles uniquement en ligne »** (OneDrive et équivalents) :
  ils ne sont pas lus, pour ne pas déclencher leur téléchargement ; ils sont
  signalés comme non analysés.
- **Photos d'une rafale, panoramas en plusieurs morceaux, photos très
  semblables mais distinctes** : elles ne sont pas des doublons et ne doivent
  pas être proposées comme tels.
- **Images presque unies ou très simples** (photo noire, ciel uni, capture
  d'écran blanche) : faute de détails, leur ressemblance n'est pas fiable ;
  elles ne sont regroupées que si elles sont identiques.
- **Recadrage important, image en miroir, montage** : hors périmètre ; ces
  copies ne sont pas forcément retrouvées.
- **Chaînes de ressemblance** (A ressemble à B, B ressemble à C, mais A ne
  ressemble pas à C) : un groupe ne réunit que des copies qui ressemblent
  toutes à la copie proposée à garder.
- **Fichier illisible ou corrompu** : il est ignoré par l'analyse et signalé,
  sans bloquer le reste.
- **Copie modifiée entre l'analyse et la suppression** : voir US2-5 ; aucune
  action n'est appliquée sur la base d'une analyse périmée.
- **Corbeille vidée avant une annulation** : les fichiers concernés sont
  signalés comme définitivement perdus par la corbeille, sans erreur
  bloquante pour les autres.
- **Photo RAW et son JPEG** (`IMG_0042.CR2` + `IMG_0042.JPG`) : ce sont deux
  versions complémentaires de la même prise de vue, pas des doublons ; elles
  ne sont pas proposées comme tels (leur rapprochement relève des images
  liées, 003). Deux copies identiques d'un même RAW sont, elles, des
  doublons.
- **Ordinateur sur batterie** : l'analyse ralentit ou se met en pause pour
  préserver l'autonomie, et reprend sur secteur.
- **Très grande bibliothèque avec des milliers de groupes** : la liste des
  groupes s'affiche et défile sans attente, triée par place récupérable.

## Requirements *(mandatory)*

### Functional Requirements

**Détection**

- **FR-001**: Le système MUST retrouver les éléments de la bibliothèque dont
  le contenu est strictement identique (octet pour octet), quels que soient
  leur nom, leur dossier et leur date, parmi tous les dossiers surveillés.
- **FR-002**: Le système MUST retrouver les photos qui représentent la même
  image sous une autre forme : autre résolution, autre compression, autre
  format d'image pris en charge par la 001, rotation par quart de tour, ou
  retouche légère de luminosité, de contraste ou de couleurs.
- **FR-003**: Le système MUST NOT proposer comme doublons des photos
  distinctes d'une même scène (rafales, photos prises à quelques secondes
  d'intervalle), ni des images trop pauvres en détails pour être comparées
  de façon fiable, sauf si elles sont identiques.
- **FR-004**: Pour les vidéos, le système MUST retrouver les copies
  identiques (FR-001). Les vidéos réencodées ou redimensionnées ne sont pas
  retrouvées dans la 002 (voir Questions ouvertes).
- **FR-005**: Le système MUST attribuer à chaque groupe un niveau de
  confiance exprimé en mots : « Identique » (contenu strictement identique),
  « Même image » (même image, autre taille, format ou compression) ou
  « Très ressemblante » (différences visibles, retouche probable, à
  vérifier). Le détail de la ressemblance MUST pouvoir être consulté.
- **FR-006**: Le système MUST NOT proposer comme doublons deux chemins qui
  désignent le même fichier sur le disque (liens physiques), ni une photo RAW
  et le JPEG qui l'accompagne.

**Groupes**

- **FR-007**: Le système MUST réunir les copies d'une même image en un
  groupe ; chaque copie d'un groupe MUST ressembler à la copie proposée à
  garder (pas de chaîne de ressemblance de proche en proche).
- **FR-008**: Le système MUST proposer dans chaque groupe une copie à garder,
  selon l'ordre : plus grande résolution, puis moindre compression (poids le
  plus élevé à résolution égale), puis présence des informations de prise de
  vue, puis date la plus ancienne ; la raison du choix MUST être affichée.
- **FR-009**: Le système MUST indiquer pour chaque groupe et pour l'ensemble
  la place que libérerait la mise à la corbeille des copies non gardées.

**Écran de revue**

- **FR-010**: Une entrée « Doublons » MUST apparaître en haut de la liste
  des dossiers dès qu'au moins un groupe existe, avec le nombre de groupes ;
  elle ouvre l'écran de revue à la place des vignettes.
- **FR-011**: L'écran de revue MUST présenter les groupes triés par place
  récupérable (par défaut), filtrables par niveau de confiance, et pour
  chaque copie : vignette, nom, dossier, dimensions, poids, format, date de
  prise de vue (ou du fichier) et disponibilité.
- **FR-012**: Les utilisateurs MUST pouvoir ouvrir une copie en grand dans la
  visionneuse, et ouvrir deux copies d'un groupe côte à côte en grand avec un
  zoom et un déplacement synchronisés.
- **FR-013**: Les utilisateurs MUST pouvoir changer la copie gardée, garder
  plusieurs copies, retirer une copie d'un groupe, marquer un groupe « Ce ne
  sont pas des doublons » ou « Garder toutes ».
- **FR-014**: Le système MUST conserver les décisions de l'utilisateur entre
  les sessions et les nouvelles analyses, y compris quand un fichier est
  renommé ou déplacé ; une décision MUST pouvoir être annulée depuis l'écran
  de revue.
- **FR-015**: Le panneau d'informations de la visionneuse MUST indiquer
  qu'un élément a des doublons et permettre d'ouvrir son groupe.

**Mise à la corbeille**

- **FR-016**: Avant toute mise à la corbeille, le système MUST afficher un
  récapitulatif listant chaque fichier concerné (nom, emplacement, poids),
  la copie gardée de chaque groupe et la place libérée, et MUST attendre une
  confirmation explicite.
- **FR-017**: Le système MUST refuser une opération qui ne laisserait aucune
  copie d'un groupe en place.
- **FR-018**: Le système MUST mettre les fichiers à la corbeille du système
  et MUST NOT jamais les effacer définitivement ; un fichier qui ne peut pas
  aller à la corbeille MUST être laissé en place et signalé.
- **FR-019**: Juste avant d'agir, le système MUST vérifier que chaque fichier
  à écarter et la copie gardée de son groupe sont toujours présents et
  inchangés depuis l'analyse ; sinon il MUST laisser le fichier en place,
  réanalyser le groupe et le signaler.
- **FR-020**: Le système MUST inscrire chaque opération dans un journal
  consultable (date, fichiers, emplacements d'origine, résultat par fichier)
  conservé dans le stockage de Picazoo.
- **FR-021**: Les utilisateurs MUST pouvoir annuler une opération du journal :
  les fichiers encore dans la corbeille reviennent à leur emplacement
  d'origine, les autres sont signalés.
- **FR-022**: Les utilisateurs MUST pouvoir appliquer en une fois les
  suggestions à tous les groupes « Identique », avec le même récapitulatif
  et la même confirmation (FR-016) ; cette application en lot n'est pas
  proposée pour les niveaux « Même image » et « Très ressemblante ».
- **FR-023**: Après une mise à la corbeille ou une annulation, la
  bibliothèque et les groupes MUST refléter immédiatement le nouvel état.
- **FR-024**: En dehors des mises à la corbeille confirmées et de leurs
  annulations, le système MUST NOT modifier, déplacer, renommer ni supprimer
  aucun fichier de l'utilisateur, ni écrire dans ses dossiers.

**Analyse en arrière-plan**

- **FR-025**: L'analyse des doublons MUST se faire en arrière-plan, sans
  jamais bloquer l'interface, et montrer sa progression de façon discrète.
- **FR-026**: L'analyse MUST être incrémentale : un élément déjà analysé et
  inchangé (même contenu selon l'index de la 001) MUST NOT être relu ; seuls
  les éléments nouveaux ou modifiés sont analysés, et les groupes mis à jour
  en conséquence.
- **FR-027**: L'analyse MUST limiter son activité disque : débit de lecture
  plafonné, priorité basse vis-à-vis des autres logiciels, ralentissement
  automatique quand l'utilisateur ou une autre application sollicite le
  disque, et jamais plus d'un travail de lecture intensive de Picazoo à la
  fois (l'indexation et les vignettes de la 001 passent avant).
- **FR-028**: L'analyse MUST éviter les lectures inutiles : un fichier n'est
  lu en entier que s'il a une chance d'être identique à un autre, et la
  comparaison visuelle réutilise autant que possible ce que la bibliothèque
  a déjà calculé.
- **FR-029**: Les utilisateurs MUST pouvoir mettre l'analyse en pause et la
  reprendre ; en pause, aucune lecture liée aux doublons n'a lieu.
- **FR-030**: L'analyse MUST reprendre après une fermeture de Picazoo là où
  elle s'était arrêtée, sans refaire le travail déjà enregistré.
- **FR-031**: Sur batterie, l'analyse MUST ralentir ou se mettre en pause,
  et reprendre normalement sur secteur.
- **FR-032**: Le système MUST NOT lire les fichiers « disponibles uniquement
  en ligne » (ce qui déclencherait leur téléchargement) ; ils sont signalés
  comme non analysés.

### Key Entities

- **Empreinte de contenu** : identifie exactement le contenu d'un élément ;
  deux éléments de même empreinte sont identiques. Rattachée à l'élément de
  la 001 et à sa version (recalculée si le contenu change).
- **Empreinte visuelle** : résume l'apparence d'une photo pour la comparer à
  d'autres malgré une autre taille, compression, format, rotation ou
  retouche légère.
- **Groupe de doublons** : ensemble d'éléments considérés comme des copies
  d'une même image ; niveau de confiance, copie proposée à garder et raison,
  place récupérable, état (à revoir, traité, ignoré).
- **Décision de l'utilisateur** : choix retenu sur un groupe ou une copie
  (copie gardée, « Ce ne sont pas des doublons », « Garder toutes », copie
  retirée) ; liée aux identifiants stables des éléments.
- **Opération sur les fichiers** : une mise à la corbeille confirmée ; date,
  liste des fichiers avec emplacement d'origine et résultat, état (faite,
  partiellement faite, annulée). Forme le journal.
- **État de l'analyse** : progression, éléments restant à analyser, pause,
  de quoi reprendre après une fermeture.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Sur un jeu de test de copies connues, 100 % des copies
  identiques sont regroupées, et au moins 95 % des copies réduites,
  recompressées, converties, tournées ou légèrement retouchées sont
  regroupées avec leur original.
- **SC-002**: Sur un jeu de test de 2 000 photos toutes distinctes, dont des
  rafales et des photos très semblables, aucun groupe « Identique » ni
  « Même image » n'est proposé, et moins de 1 % des photos apparaissent dans
  un groupe « Très ressemblante ».
- **SC-003**: Sur une bibliothèque déjà indexée de 20 000 photos, l'analyse
  complète des doublons se termine en moins de 30 minutes sur un ordinateur
  portable courant, ordinateur sur secteur et laissé au repos.
- **SC-004**: Pendant l'analyse, le défilement des vignettes reste fluide et
  une photo s'ouvre en grand aussi vite que sans analyse (moins d'une
  seconde).
- **SC-005**: Pendant l'analyse, la copie d'un gros fichier par l'Explorateur
  sur le même disque ne prend pas plus de 20 % de temps de plus qu'à l'arrêt,
  et l'ordinateur reste utilisable normalement (ouvrir un logiciel, une
  fenêtre de l'Explorateur).
- **SC-006**: Après l'ajout d'une copie d'une photo existante dans une
  bibliothèque déjà analysée, son groupe apparaît en moins d'une minute.
- **SC-007**: Après une interruption, l'analyse reprend en ne relisant aucun
  élément déjà analysé et inchangé.
- **SC-008**: Après n'importe quelle utilisation de cette fonctionnalité, les
  seuls changements dans les dossiers surveillés sont les fichiers que
  l'utilisateur a confirmé mettre à la corbeille, et 100 % d'entre eux sont
  récupérables depuis la corbeille ou par l'annulation tant que la corbeille
  n'a pas été vidée (vérifiable en comparant le contenu des dossiers avant
  et après).
- **SC-009**: Avec les suggestions automatiques, un utilisateur traite un
  groupe de doublons (vérifier, confirmer) en moins de 10 secondes, et
  50 groupes « Identique » en moins de 2 minutes grâce à l'application en
  lot.
- **SC-010**: Un utilisateur qui découvre l'écran des doublons comprend, sans
  explication, quelle copie sera gardée et quels fichiers partiront à la
  corbeille avant de confirmer (vérifié auprès de 5 personnes).

## Assumptions

- La bibliothèque de la 001 est en place : éléments indexés avec un
  identifiant stable, détection des renommages et déplacements, vignettes
  conservées. La 002 s'appuie sur elle et n'en change pas le comportement.
- Le problème de saturation du disque au lancement constaté sur la 001 est
  corrigé séparément ; la 002 respecte les mêmes limites et ne lance pas son
  analyse tant que l'indexation et les vignettes visibles ne sont pas
  terminées.
- L'analyse des doublons démarre automatiquement en arrière-plan après
  l'indexation, peut être mise en pause, et peut être désactivée dans les
  réglages (voir Questions ouvertes).
- La comparaison couvre toute la bibliothèque, tous dossiers surveillés
  confondus ; les éléments des dossiers exclus ne sont pas analysés.
- La seule opération sur les fichiers de la 002 est la mise à la corbeille
  du système ; le déplacement vers un autre dossier, la fusion des
  informations (dates, mots-clés) entre copies et la suppression définitive
  sont hors périmètre.
- Les doublons de vidéos se limitent aux copies identiques ; les vidéos
  réencodées ou redimensionnées sont hors périmètre de la 002.
- Les photos RAW sont comparées par leur aperçu intégré, comme dans la 001 ;
  un RAW et son JPEG compagnon ne sont pas des doublons.
- L'interface est en français ; le journal des opérations est conservé dans
  le stockage de Picazoo et n'est jamais écrit dans les dossiers de
  l'utilisateur.
- Ordinateur visé : un portable courant, potentiellement peu puissant et
  sensible à la chauffe ; les seuils de SC-003 et SC-005 sont mesurés dans
  ces conditions.

## Questions ouvertes

*À trancher à la relecture (ou avec `/speckit-clarify`) ; une réponse par
défaut est retenue dans la spec en attendant.*

1. **Vidéos réencodées** : faut-il retrouver aussi la même vidéo dans une
   autre résolution ou un autre encodage (par exemple la version envoyée par
   une messagerie) ? *Par défaut : non, seulement les copies identiques
   (FR-004).*
2. **Démarrage de l'analyse** : automatique en arrière-plan après
   l'indexation, ou seulement à la demande (« Rechercher les doublons ») ?
   *Par défaut : automatique, avec pause et désactivation possibles.*
3. **Disques sans corbeille** (partage réseau, clé USB) : se contenter de
   signaler ces copies, ou proposer de les déplacer dans un dossier de
   quarantaine choisi par l'utilisateur ? *Par défaut : signaler seulement,
   jamais d'effacement définitif (FR-018).*
4. **RAW + JPEG** : un JPEG tiré d'un RAW (même prise de vue) ne doit-il
   jamais être proposé comme doublon, ou seulement quand les deux portent le
   même nom dans le même dossier ? *Par défaut : jamais (FR-006).*

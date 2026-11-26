# Feature Specification: Images liées

**Feature Branch**: `003-images-liees`

**Created**: 2026-11-26

**Status**: Draft

**Input**: User description: "Feature 003 — Images liées, d'après la feuille de route (docs/feuille-de-route.md) : repérer les images dont les noms de fichiers partagent une même racine répétée (ex. vacances_bali_001, vacances_bali_002…), afficher une estimation de confiance du groupement, et laisser l'utilisateur valider ou corriger ces « groupes » avant toute action. Dépend de la fonctionnalité 001 (bibliothèque et visionneuse), déjà livrée. Prépare le terrain pour la fonctionnalité 004 (réorganisation de la bibliothèque, un sous-dossier par groupe). Inspiré de l'esprit Picasa (regroupement de photos liées), mais Picasa ne le faisait pas par nom de fichier de cette façon — c'est une fonctionnalité propre à Picazoo."

## Contexte

Un appareil photo ou un téléphone nomme ses fichiers par rafale ou par
événement : `vacances_bali_001.jpg`, `vacances_bali_002.jpg`… ou, une fois
renommés par l'utilisateur dans l'Explorateur, `Anniversaire Julie (1).jpg`,
`Anniversaire Julie (2).jpg`. Ce nom répété est un indice fort que les photos
appartiennent au même événement, indépendamment de leur contenu visuel — à la
différence de la 002, qui regroupe par ressemblance d'image. Beaucoup de
photos n'ont toutefois jamais été renommées et portent encore le nom donné
par l'appareil (`IMG_0001.jpg`, `DSC_0002.jpg`, `PXL_20240714_...`) : ce
nom-là ne dit rien d'un événement, seulement de l'appareil, et ne doit pas
produire de faux regroupements.

La 003 s'appuie sur la bibliothèque indexée par la 001 (noms de fichiers,
chemins, identifiants stables) et ne lit aucun contenu de fichier : c'est une
fonctionnalité de suggestion, bien plus légère que la 002. Elle ne modifie
jamais les fichiers de l'utilisateur ; elle prépare les groupes que la 004
utilisera plus tard pour proposer une réorganisation en sous-dossiers.

## Clarifications

### Session 2026-11-26

- Q: Le regroupement par racine de nom doit-il porter sur toute la
  bibliothèque (racines identiques dans des dossiers différents) ou être
  limité à un même dossier (et ses sous-dossiers) ? → A: Toute la
  bibliothèque, tous dossiers confondus, car Picasa faisait déjà ça.
- Q: Une racine générique donnée par l'appareil (`IMG_`, `DSC_`, `PXL_`,
  `VID_`, `MVI_`, un numéro seul…) doit-elle être écartée du regroupement, ou
  regroupée quand même avec une confiance plus faible ? → A: Écartée par
  défaut (ne forme pas de groupe), avec un réglage pour l'inclure si
  l'utilisateur le souhaite explicitement.
- Q: Les groupes d'images liées doivent-ils apparaître dans un nouvel écran
  séparé de l'écran « Doublons » de la 002, ou y être fusionnés ? → A: Un
  nouvel écran séparé (« Images liées »), à côté de « Doublons » dans la
  liste des dossiers ; les deux fonctionnalités restent indépendantes.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Voir les groupes proposés par nom, avec leur confiance (Priority: P1)

Une fois la bibliothèque indexée (001), Picazoo repère en quelques instants
les éléments dont les noms de fichiers partagent une racine commune suivie
d'un numéro ou d'une variante (`vacances_bali_001`, `vacances_bali_002`…).
Une entrée « Images liées » apparaît dans la liste des dossiers, à côté de
« Doublons », avec le nombre de groupes trouvés. En l'ouvrant, l'utilisateur
voit la liste des groupes : chaque groupe affiche la racine détectée comme
nom suggéré, le nombre d'éléments, leurs vignettes, et un niveau de confiance
en mots. Les racines génériques données par l'appareil (`IMG_`, `DSC_`…)
ne forment pas de groupe par défaut.

**Why this priority**: c'est la valeur de base de la fonctionnalité — sans
repérage visible et compréhensible, rien d'autre n'est possible.

**Independent Test**: dans une bibliothèque de test, ajouter 5 photos
nommées `vacances_bali_001.jpg` à `vacances_bali_005.jpg`, 5 photos nommées
`IMG_0001.jpg` à `IMG_0005.jpg`, et 10 photos aux noms sans rapport ; après le
repérage, l'entrée « Images liées » indique 1 groupe de 5 éléments nommé
« vacances bali », et ni les `IMG_*` ni les photos sans rapport n'y figurent.

**Acceptance Scenarios**:

1. **Given** une bibliothèque contenant cinq photos nommées avec une racine
   commune suivie d'un numéro, **When** le repérage se termine, **Then** un
   groupe de 5 éléments apparaît, avec la racine comme nom suggéré.
2. **Given** des photos nommées `IMG_0001.jpg` à `IMG_0010.jpg`, **When** le
   repérage se termine, **Then** elles ne forment pas de groupe.
3. **Given** l'écran « Images liées » ouvert, **When** l'utilisateur regarde
   un groupe, **Then** il voit le nom suggéré, le nombre d'éléments, leurs
   vignettes et le niveau de confiance.
4. **Given** aucun groupe trouvé, **When** l'utilisateur ouvre l'entrée
   « Images liées », **Then** un message l'indique clairement.

---

### User Story 2 - Valider ou corriger un groupe avant toute action (Priority: P1)

Dans l'écran « Images liées », l'utilisateur peut retirer un élément d'un
groupe, en ajouter un depuis le reste de la bibliothèque, renommer le nom
suggéré du groupe, fusionner deux groupes, scinder un groupe en deux, ou
marquer un groupe « Ce n'est pas un groupe ». Aucune de ces actions ne
modifie, ne déplace ni ne renomme un fichier sur le disque : elles ne
changent que la façon dont Picazoo regroupe les éléments dans son propre
stockage, en vue d'une future réorganisation (004).

**Why this priority**: le regroupement automatique par nom produit
forcément des approximations (racines qui se ressemblent par coïncidence,
événements découpés en plusieurs racines) ; sans correction possible, la
fonctionnalité resterait peu fiable.

**Independent Test**: sur le groupe « vacances bali » de l'US1, retirer une
des 5 photos, renommer le groupe en « Bali 2024 », puis rouvrir l'écran :
le groupe affiche 4 éléments sous le nom « Bali 2024 », et aucun fichier n'a
changé sur le disque.

**Acceptance Scenarios**:

1. **Given** un groupe affiché, **When** l'utilisateur retire un élément,
   **Then** il n'apparaît plus dans ce groupe mais reste visible normalement
   dans sa bibliothèque.
2. **Given** deux groupes dont l'utilisateur sait qu'ils forment un seul
   événement, **When** il les fusionne, **Then** un seul groupe réunit tous
   leurs éléments, sous un nom qu'il choisit.
3. **Given** un groupe mélangeant par erreur deux événements, **When**
   l'utilisateur le scinde, **Then** deux groupes distincts apparaissent,
   chacun avec son propre nom.
4. **Given** un groupe marqué « Ce n'est pas un groupe », **When** une
   nouvelle détection a lieu, **Then** ces éléments ne sont plus proposés
   ensemble.
5. **Given** n'importe laquelle de ces corrections, **When** elle est
   appliquée, **Then** aucun fichier de l'utilisateur n'est modifié, déplacé
   ou renommé sur le disque.

---

### User Story 3 - Un repérage discret qui suit la bibliothèque (Priority: P2)

Le repérage des groupes se fait automatiquement après l'indexation de la
001, sans action de l'utilisateur, et se met à jour tout seul : une photo
ajoutée dans un dossier surveillé rejoint son groupe si son nom correspond à
une racine déjà repérée ou confirmée, sauf si l'utilisateur a explicitement
rejeté cette racine. Comme le repérage ne lit que les noms déjà indexés par
la 001 (aucune lecture du contenu des fichiers), il reste quasi instantané
même sur une grande bibliothèque et ne ralentit jamais l'ordinateur.

**Why this priority**: sans mise à jour automatique, la fonctionnalité
perdrait son intérêt dès le premier ajout de photos après le premier
repérage ; mais un premier repérage manuel suffit à valider les US1 et US2.

**Independent Test**: après le repérage initial de l'US1, ajouter une photo
`vacances_bali_006.jpg` dans le dossier surveillé ; en moins d'une minute,
le groupe « vacances bali » affiche 6 éléments, sans qu'aucune autre partie
de la bibliothèque n'ait été relue.

**Acceptance Scenarios**:

1. **Given** une bibliothèque déjà repérée, **When** une photo rejoint un
   dossier surveillé avec un nom prolongeant une racine existante, **Then**
   elle rejoint le groupe correspondant en moins d'une minute.
2. **Given** un repérage en cours sur une bibliothèque de 100 000 éléments,
   **When** l'utilisateur fait défiler la bibliothèque, **Then** il ne note
   aucun ralentissement.
3. **Given** une racine que l'utilisateur a marquée « Ce n'est pas un
   groupe », **When** une nouvelle photo portant cette racine est ajoutée,
   **Then** elle ne rejoint pas un groupe reconstitué sous cette racine.

---

### User Story 4 - Des groupes prêts pour la réorganisation à venir (Priority: P3)

Les décisions de l'utilisateur (groupes confirmés, fusionnés, scindés,
renommés, exclus) sont conservées entre les sessions et après une nouvelle
analyse, y compris si un fichier est renommé ou déplacé entre-temps. Chaque
groupe confirmé a un nom clair, prêt à être proposé plus tard comme nom de
sous-dossier par la 004.

**Why this priority**: valeur surtout préparatoire (004) ; les US1 à US3
apportent déjà toute la valeur immédiate de la 003.

**Independent Test**: confirmer le nom « Bali 2024 » pour un groupe, fermer
et relancer Picazoo, puis renommer manuellement un des fichiers du groupe
dans l'Explorateur : à la réouverture de l'écran, le groupe affiche toujours
le même nom et le même nombre d'éléments.

**Acceptance Scenarios**:

1. **Given** un groupe renommé par l'utilisateur, **When** Picazoo est
   relancé, **Then** le nom choisi est toujours affiché.
2. **Given** un élément d'un groupe renommé ou déplacé sur le disque,
   **When** le groupe est réaffiché, **Then** il y figure toujours.
3. **Given** un groupe confirmé, **When** l'utilisateur consulte ses
   informations, **Then** le nom prêt pour un sous-dossier est visible.

---

### Edge Cases

- **Racine générique de l'appareil** (`IMG_`, `DSC_`, `PXL_`, `VID_`,
  `MVI_`, une capture d'écran, un numéro seul) : écartée du regroupement par
  défaut (FR-003) ; un réglage permet de l'inclure explicitement.
- **Suffixe de copie Windows** (`photo (1).jpg`, `photo (2).jpg`, `photo -
  copie.jpg`) : traité comme une variante de numérotation, au même titre
  qu'un numéro de séquence.
- **Casse différente** (`Vacances_Bali_001.jpg` vs `vacances_bali_002.JPG`) :
  la comparaison des racines ignore la casse.
- **Photo et vidéo de même racine** (`vacances_bali_001.jpg` et
  `vacances_bali_001.mp4`, cas d'une photo en rafale avec vidéo associée) :
  regroupées ensemble comme tout autre élément de même racine.
- **Racine très courte ou peu spécifique** (`a_1`, `01`, `img`) : jugée trop
  générique, pas de groupe formé (même règle que pour les racines
  d'appareil).
- **Deux événements distincts partageant par coïncidence une racine
  générique** (deux vacances différentes toutes deux nommées « photo ») :
  l'utilisateur peut scinder le groupe (US2) ; la détection n'essaie pas de
  deviner cette distinction seule.
- **Élément qui appartient aussi à un groupe de doublons (002)** : les deux
  fonctionnalités restent indépendantes ; un élément peut figurer à la fois
  dans un groupe de doublons et un groupe d'images liées.
- **Dossier surveillé devenu indisponible** (disque débranché) : ses
  éléments restent dans leurs groupes, marqués indisponibles.
- **Très grand groupe** (plusieurs centaines d'éléments sous une même
  racine) : la liste de ses éléments s'affiche et défile sans attente.
- **Élément supprimé ou mis à la corbeille depuis un autre écran (002)** :
  il disparaît de son groupe d'images liées sans erreur.

## Requirements *(mandatory)*

### Functional Requirements

**Détection**

- **FR-001**: Le système MUST repérer les éléments de la bibliothèque dont
  les noms de fichiers partagent une racine commune suivie d'un numéro de
  séquence ou d'une variante de numérotation usuelle (suffixe numérique avec
  ou sans séparateur, numérotation entre parenthèses, suffixe « copie »),
  indépendamment de la casse.
- **FR-002**: Le regroupement MUST porter sur l'ensemble de la bibliothèque,
  tous dossiers surveillés confondus, et non sur un seul dossier à la fois.
- **FR-003**: Le système MUST écarter par défaut du regroupement les
  racines jugées génériques : préfixes usuels donnés par un appareil photo
  ou un téléphone (`IMG_`, `DSC_`, `PXL_`, `VID_`, `MVI_`, captures d'écran…)
  et racines trop courtes ou peu spécifiques pour désigner un événement ;
  les utilisateurs MUST pouvoir activer leur inclusion dans les réglages.
- **FR-004**: Le système MUST NOT proposer de groupe d'un seul élément.
- **FR-005**: Le système MUST attribuer à chaque groupe un niveau de
  confiance exprimé en mots, reflétant la spécificité de la racine détectée
  et la régularité de la numérotation.
- **FR-006**: Les vidéos indexées par la 001 MUST être prises en compte au
  même titre que les photos pour la détection de racine commune.

**Groupes**

- **FR-007**: Chaque groupe MUST avoir un nom suggéré dérivé de la racine
  détectée, modifiable par l'utilisateur.
- **FR-008**: Le système MUST permettre à l'utilisateur d'ajouter ou de
  retirer un élément d'un groupe, de fusionner deux groupes, de scinder un
  groupe en deux, et de marquer un groupe « Ce n'est pas un groupe ».
- **FR-009**: Le système MUST conserver les décisions de l'utilisateur entre
  les sessions et les nouvelles analyses, y compris quand un fichier est
  renommé ou déplacé ; une décision MUST pouvoir être annulée depuis l'écran
  de revue.
- **FR-010**: Un élément MUST pouvoir appartenir à la fois à un groupe de
  doublons (002) et à un groupe d'images liées (003), sans conflit ni
  fusion des deux écrans.

**Écran de revue**

- **FR-011**: Une entrée « Images liées », distincte de l'entrée
  « Doublons » (002), MUST apparaître dans la liste des dossiers dès qu'au
  moins un groupe existe, avec le nombre de groupes ; elle ouvre l'écran de
  revue des groupes d'images liées.
- **FR-012**: L'écran de revue MUST présenter les groupes avec leur nom
  suggéré, leur nombre d'éléments, les vignettes de leurs éléments, et leur
  niveau de confiance, filtrables par niveau de confiance.
- **FR-013**: Les utilisateurs MUST pouvoir ouvrir un élément en grand dans
  la visionneuse depuis l'écran de revue.

**Repérage en arrière-plan**

- **FR-014**: Le repérage MUST se faire automatiquement après l'indexation
  de la 001 (nouveaux éléments, renommages, suppressions), sans action de
  l'utilisateur et sans lire le contenu des fichiers.
- **FR-015**: Le repérage MUST ne jamais bloquer l'interface, même sur une
  bibliothèque de 100 000 éléments.
- **FR-016**: Un élément ajouté dans un dossier surveillé dont le nom
  prolonge une racine déjà repérée ou confirmée MUST rejoindre le groupe
  correspondant, sauf si cette racine a été explicitement rejetée par
  l'utilisateur (FR-008).
- **FR-017**: En dehors de l'écriture dans son propre stockage, le système
  MUST NOT modifier, déplacer, renommer ni supprimer aucun fichier de
  l'utilisateur ; aucune action de cette fonctionnalité ne touche au disque
  de l'utilisateur (principe I de la constitution).

### Key Entities

- **Racine de nommage** : partie commune détectée d'un ensemble de noms de
  fichiers (ex. « vacances_bali ») ; porte une indication de spécificité
  (générique ou significative) qui conditionne si elle forme un groupe par
  défaut (FR-003).
- **Groupe d'images liées** : ensemble d'éléments de la bibliothèque (001)
  partageant une racine ; nom suggéré (éditable), niveau de confiance, état
  (à revoir, confirmé, ignoré).
- **Décision de l'utilisateur** : choix retenu sur un groupe ou un élément
  (ajout, retrait, fusion, scission, renommage, « ce n'est pas un groupe »)
  ; liée aux identifiants stables des éléments (001).

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Sur un jeu de test mêlant des séries nommées par événement
  (racine suivie d'une numérotation régulière) et des photos nommées par
  l'appareil (`IMG_`, `DSC_`…), au moins 95 % des séries à racine
  significative sont regroupées, et aucune série de photos nommées par
  l'appareil ne forme de groupe par défaut.
- **SC-002**: Sur une bibliothèque déjà indexée de 100 000 éléments, le
  premier repérage des groupes se termine en moins d'une minute.
- **SC-003**: Pendant le repérage, le défilement de la bibliothèque reste
  fluide et une photo s'ouvre en grand aussi vite que sans repérage en
  cours.
- **SC-004**: Après l'ajout d'un élément prolongeant une racine déjà
  repérée dans un dossier surveillé, il rejoint son groupe en moins d'une
  minute.
- **SC-005**: Un utilisateur qui découvre l'écran « Images liées » comprend,
  sans explication, pourquoi les éléments d'un groupe sont proposés
  ensemble et peut corriger un regroupement incorrect en moins de 30
  secondes (vérifié auprès de 5 personnes).
- **SC-006**: Après n'importe quelle utilisation de cette fonctionnalité,
  aucun fichier dans les dossiers surveillés n'a été modifié, déplacé,
  renommé ou supprimé (vérifiable en comparant le contenu des dossiers
  avant et après).
- **SC-007**: Les décisions de l'utilisateur sur un groupe (renommage,
  fusion, scission, exclusion) survivent à un redémarrage de Picazoo et à un
  renommage ou déplacement de fichier sur le disque.

## Assumptions

- La bibliothèque de la 001 est en place : éléments indexés avec un
  identifiant stable, détection des renommages et déplacements. La 003
  s'appuie sur elle et n'en change pas le comportement.
- La 003 ne lit que les métadonnées déjà indexées par la 001 (noms de
  fichiers, chemins) ; elle ne lit jamais le contenu d'un fichier, à la
  différence de la 002, ce qui explique un repérage quasi instantané sans
  infrastructure de limitation disque dédiée.
- La 003 est indépendante de la 002 (détection des doublons) : un même
  élément peut apparaître dans les deux écrans sans lien entre eux pour
  cette fonctionnalité ; un rapprochement éventuel des deux écrans est hors
  périmètre.
- Aucune action sur les fichiers n'a lieu dans la 003 : c'est une
  fonctionnalité de suggestion et de validation pure, dont le résultat
  (groupes confirmés et leur nom) sera utilisé par la 004 pour proposer une
  réorganisation. La 004 reste hors périmètre de cette spec.
- Le regroupement par racine de nom porte sur toute la bibliothèque, tous
  dossiers confondus (voir Clarifications).
- Les racines génériques données par un appareil sont écartées par défaut,
  avec un réglage pour les inclure (voir Clarifications).
- L'écran « Images liées » est séparé de l'écran « Doublons » de la 002
  (voir Clarifications).
- L'interface est en français, cohérente avec les écrans de la 001 et de la
  002.

# Feature Specification: Réduction de la consommation de RAM au scan initial

**Feature Branch**: `008-reduction-ram-scan`

**Created**: 2026-11-25

**Status**: Draft

**Input**: User description: "On a un souci de consommation de RAM lors du chargement des images. Lors du premier lancement et du scan des images, Picazoo monte jusqu'à 2,5 Go de RAM. Optimiser la consommation de RAM."

## Clarifications

### Session 2026-11-25

- Q: Quel plafond de RAM viser pour le pic pendant le scan initial (actuellement ~2,5 Go) ? → A: ~1,2-1,5 Go.
- Q: Quelle configuration minimale Picazoo doit bien supporter pendant le scan (RAM totale de la machine) ? → A: 8 Go de RAM.
- Q: Si réduire la RAM ralentit un peu l'apparition des vignettes pendant le scan, quel compromis acceptez-vous ? → A: léger ralentissement toléré (jusqu'à 20-30 %).
- Q: Quand on parle du « pic de mémoire utilisé par Picazoo » dans les critères de succès, quelle mesure fait foi ? → A: la mémoire du Gestionnaire des tâches Windows (colonne « Mémoire », working set privé) pour le processus `picazoo.exe`.

## Contexte

Au premier lancement, et à chaque fois qu'un scan complet tourne (nouveau
dossier surveillé, reprise après une longue interruption), Picazoo découvre
les dossiers puis génère les vignettes de toutes les photos et vidéos
trouvées. Pendant cette phase, la consommation de RAM de Picazoo grimpe
jusqu'à 2,5 Go, ce qui est trop sur une machine portable courante (8 Go de
RAM au total) : moins de RAM reste disponible pour les autres logiciels, et
l'ordinateur peut ralentir ou se mettre à échanger sur le disque pendant ce
temps.

Le scan initial reste indispensable et doit continuer à remplir la grille de
vignettes rapidement (c'est l'un des points forts de Picazoo face à
l'Explorateur Windows). L'objectif de cette fonctionnalité n'est donc pas de
ralentir le scan à tout prix, mais de retrouver un pic de mémoire raisonnable
pour une machine à 8 Go de RAM, quitte à accepter un léger ralentissement de
l'apparition des vignettes (jusqu'à 20-30 %) en échange.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Premier lancement sans asphyxier une machine modeste (Priority: P1)

Un utilisateur installe Picazoo sur un ordinateur portable courant (8 Go de
RAM) avec une bibliothèque de plusieurs dizaines de milliers de photos et
vidéos, dont des photos iPhone (HEIC haute résolution) et quelques RAW.
Pendant tout le premier scan, Picazoo reste dans une limite de mémoire
raisonnable : l'utilisateur peut continuer à se servir d'autres logiciels
(navigateur, messagerie, bureautique) sans ralentissement perceptible ni
avertissement de Windows sur le manque de mémoire.

**Why this priority**: c'est le problème signalé ; sans cela, Picazoo donne
une mauvaise première impression et peut gêner le reste de l'ordinateur dès
l'installation.

**Independent Test**: sur une machine à 8 Go de RAM, lancer Picazoo pour la
première fois sur une bibliothèque de référence (mélange JPEG, HEIC, RAW,
vidéos, environ 100 000 éléments) pendant qu'un navigateur et un traitement
de texte sont ouverts ; mesurer le pic de mémoire utilisé par Picazoo pendant
tout le scan et constater que les autres logiciels restent fluides.

**Acceptance Scenarios**:

1. **Given** une machine à 8 Go de RAM et une bibliothèque non encore
   analysée, **When** Picazoo lance son premier scan, **Then** le pic de
   mémoire utilisé par Picazoo pendant toute la durée du scan (découverte et
   vignettes) ne dépasse pas la limite visée (voir Success Criteria).
2. **Given** un scan initial en cours au plafond de mémoire visé, **When**
   l'utilisateur ouvre d'autres applications courantes, **Then** elles
   restent réactives, sans ralentissement perceptible ni avertissement
   Windows de mémoire insuffisante.
3. **Given** une bibliothèque contenant de nombreuses photos iPhone (HEIC,
   très haute résolution), **When** leurs vignettes sont générées, **Then**
   elles ne provoquent pas de pic de mémoire disproportionné par rapport aux
   autres formats.
4. **Given** le scan initial terminé, **When** l'utilisateur consulte la
   mémoire utilisée par Picazoo au repos, **Then** elle redescend à un niveau
   nettement inférieur au pic du scan (pas de fuite de mémoire laissée par le
   scan).

---

### User Story 2 - Les vignettes continuent d'apparaître vite (Priority: P1)

Pendant le scan, la grille de Picazoo continue de se remplir de vignettes au
fur et à mesure, dossier par dossier, comme aujourd'hui. La réduction de la
mémoire utilisée ne doit pas transformer l'attente en un écran vide qui met
beaucoup plus de temps à se remplir : un léger ralentissement est acceptable,
une attente très supérieure à aujourd'hui ne l'est pas.

**Why this priority**: la vitesse d'apparition des vignettes est ce qui rend
le premier lancement de Picazoo agréable ; une fonctionnalité qui économise
de la RAM mais rend ce moment frustrant déplacerait le problème plutôt que de
le résoudre.

**Independent Test**: sur la même bibliothèque de référence et la même
machine, chronométrer le temps nécessaire pour que toutes les vignettes
soient visibles dans la grille, avant et après cette fonctionnalité, et
constater que l'écart reste dans la marge tolérée.

**Acceptance Scenarios**:

1. **Given** la bibliothèque de référence, **When** le scan initial tourne
   avec les nouvelles limites de mémoire, **Then** le temps total pour que
   toutes les vignettes soient visibles n'augmente pas de plus de 30 % par
   rapport au comportement actuel.
2. **Given** le scan initial en cours, **When** l'utilisateur fait défiler la
   grille vers des dossiers pas encore traités, **Then** ces dossiers restent
   prioritaires dans la génération des vignettes, comme aujourd'hui.
3. **Given** une machine plus puissante avec davantage de RAM, **When** le
   scan tourne, **Then** Picazoo n'impose pas inutilement le même
   ralentissement qu'une machine à 8 Go de RAM (les limites tiennent compte
   de la RAM réellement disponible).

---

### User Story 3 - Le scan reste fiable dans la durée (Priority: P2)

Les nouvelles limites de mémoire ne doivent rien changer aux garanties déjà
en place : le scan interrompu (fermeture de Picazoo, mise en veille, perte
d'un disque externe) reprend là où il s'était arrêté, aucune vignette n'est
corrompue ou manquante à cause d'une limite de mémoire atteinte, et la
bibliothèque reste exacte une fois le scan terminé.

**Why this priority**: réduire la mémoire ne doit pas introduire de nouveaux
bugs ; c'est une contrainte de non-régression plutôt qu'une nouvelle valeur
visible, d'où une priorité un cran sous les deux premières.

**Independent Test**: lancer un scan sur une grande bibliothèque, le
suspendre en fermant Picazoo à mi-parcours, le relancer, et vérifier que tous
les éléments finissent par avoir leur vignette, sans doublon ni élément
oublié, en restant sous le plafond de mémoire visé à la reprise aussi.

**Acceptance Scenarios**:

1. **Given** un scan interrompu par la fermeture de Picazoo, **When**
   l'utilisateur relance Picazoo, **Then** le scan reprend là où il s'était
   arrêté et respecte les mêmes limites de mémoire qu'un scan initial.
2. **Given** un scan qui tourne au plafond de mémoire visé, **When** il se
   termine, **Then** la bibliothèque obtenue est identique (mêmes éléments,
   mêmes vignettes) à celle obtenue sans la limite de mémoire.
3. **Given** un nouveau dossier ajouté après le premier scan, **When** son
   analyse démarre, **Then** les mêmes limites de mémoire s'appliquent (pas
   seulement au tout premier lancement).

---

### Edge Cases

- **Bibliothèque avec de très grandes photos** (RAW 50+ Mpx, HEIC d'iPhone
  récents, panoramas) : leur traitement ne doit pas, à lui seul, faire
  dépasser le plafond de mémoire visé, même si plusieurs sont traitées en
  même temps.
- **Machine avec beaucoup de cœurs mais peu de RAM** : le scan doit limiter
  le nombre de traitements menés de front pour respecter le plafond de
  mémoire, plutôt que de se caler uniquement sur le nombre de cœurs du
  processeur.
- **Machine avec peu de cœurs mais beaucoup de RAM** : le scan ne doit pas se
  limiter inutilement en dessous de ce que la machine peut encaisser.
- **RAM disponible qui diminue pendant le scan** (l'utilisateur ouvre
  d'autres logiciels gourmands) : le scan doit réduire son activité plutôt
  que de risquer de pousser la machine à manquer de mémoire.
- **Disque externe lent ou débranché pendant le scan** : la réduction de
  mémoire ne doit pas aggraver ce cas, déjà géré par ailleurs (racine
  indisponible, reprise automatique).
- **Visionneuse ouverte pendant le scan** : l'image affichée en grand doit
  rester fluide, les limites de mémoire du scan ne doivent pas la pénaliser
  de façon visible.
- **Très petite bibliothèque** (quelques dizaines de photos) : le scan doit
  rester instantané comme aujourd'hui ; les nouvelles limites ne doivent
  avoir d'effet perceptible que sur les grandes bibliothèques où le problème
  se posait.
- **Vidéos nombreuses** : leur traitement (lecture d'un extrait pour la
  vignette) ne doit pas non plus faire dépasser le plafond visé.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: Le système MUST limiter le pic de mémoire vive utilisée par
  Picazoo pendant un scan avec génération de vignettes (premier lancement,
  nouveau dossier, reprise après interruption) à un plafond cohérent avec une
  machine disposant de 8 Go de RAM, sans réglage à faire par l'utilisateur.
- **FR-002**: Le système MUST adapter le nombre de traitements menés de
  front pendant un scan à la mémoire disponible sur la machine, et non au
  seul nombre de cœurs du processeur ; si la mémoire disponible diminue
  pendant le scan, le système MUST réduire son activité en conséquence.
- **FR-003**: Le traitement d'une photo ou d'une vidéo pour en produire la
  vignette MUST éviter de conserver en mémoire, même brièvement, une
  représentation à pleine résolution plus grande que nécessaire pour la
  vignette finale, quel que soit le format d'origine (JPEG, HEIC, RAW, TIFF,
  vidéo).
- **FR-004**: Les vignettes MUST continuer d'apparaître dans la grille au fur
  et à mesure qu'elles sont prêtes, dossier par dossier, comme aujourd'hui ;
  les dossiers visibles à l'écran MUST rester prioritaires.
- **FR-005**: Picazoo MUST rester utilisable pendant tout le scan initial, au
  plafond de mémoire visé : la grille reste navigable, un élément déjà traité
  s'ouvre en grand sans attendre la fin du scan.
- **FR-006**: Les garanties déjà en place MUST continuer de s'appliquer sous
  les nouvelles limites de mémoire : reprise exacte d'un scan interrompu,
  aucune vignette manquante ou corrompue, bibliothèque finale identique à
  celle obtenue sans limite.
- **FR-007**: Le ralentissement de l'apparition des vignettes causé par les
  nouvelles limites de mémoire MUST rester dans la marge tolérée (voir
  Success Criteria), mesuré sur une bibliothèque de référence représentative.
- **FR-008**: Le système MUST offrir un moyen de mesurer, lors d'un scan de
  test sur une bibliothèque donnée, le pic de mémoire utilisé par Picazoo (la
  même grandeur que la colonne « Mémoire » du Gestionnaire des tâches
  Windows), afin de vérifier les objectifs de cette fonctionnalité et de
  détecter une régression future.

### Key Entities

- **Budget mémoire du scan** : limite interne, déduite de la RAM disponible
  sur la machine, qui borne le nombre de traitements de vignettes menés de
  front et les caches internes actifs pendant un scan. Recalculée au
  lancement et réévaluée si la mémoire disponible change notablement pendant
  le scan.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Sur une bibliothèque comparable à celle qui faisait monter
  Picazoo à 2,5 Go aujourd'hui, le pic de mémoire pendant le scan initial
  (colonne « Mémoire » du Gestionnaire des tâches Windows pour `picazoo.exe`)
  ne dépasse pas 1,5 Go, soit une réduction d'au moins 40 %.
- **SC-002**: Sur une bibliothèque de référence d'environ 100 000 photos et
  vidéos (mélange JPEG, HEIC haute résolution, RAW, vidéos), lancée sur une
  machine à 8 Go de RAM, le pic de mémoire utilisé par Picazoo (Gestionnaire
  des tâches) pendant tout le scan initial ne dépasse pas 1,5 Go.
- **SC-003**: Sur cette même bibliothèque de référence et la même machine, le
  temps total pour que toutes les vignettes soient visibles dans la grille
  n'augmente pas de plus de 30 % par rapport au comportement actuel.
- **SC-004**: Pendant le scan initial au plafond de mémoire visé, sur une
  machine à 8 Go de RAM, d'autres logiciels courants ouverts en parallèle
  (navigateur, messagerie, bureautique) restent utilisables sans
  ralentissement perceptible ni avertissement Windows de mémoire
  insuffisante.
- **SC-005**: Une fois le scan initial terminé, la mémoire utilisée par
  Picazoo au repos redescend à moins de la moitié du pic observé pendant le
  scan, dans la minute qui suit.
- **SC-006**: Sur une machine disposant de nettement plus de 8 Go de RAM, le
  temps du scan initial sur la bibliothèque de référence n'est pas dégradé
  par les nouvelles limites de mémoire par rapport au comportement actuel.
- **SC-007**: Un scan interrompu puis repris produit, dans 100 % des essais,
  la même bibliothèque finale (mêmes éléments, mêmes vignettes) qu'un scan
  sans interruption, en respectant les mêmes limites de mémoire.

## Assumptions

- La bibliothèque de référence pour les mesures (SC-002, SC-003, SC-006) est
  une bibliothèque synthétique d'environ 100 000 éléments mélangeant JPEG,
  HEIC (type photos iPhone, haute résolution), quelques RAW et quelques
  vidéos, cohérente avec le banc d'essai déjà utilisé par le projet
  (`picazoo-bench`).
- La machine de référence pour les mesures est une machine à 8 Go de RAM,
  avec un processeur modeste (comparable au Core Ultra 5 déjà utilisé pour
  les tests précédents), conformément aux réponses de la session de
  clarification du 2026-11-25.
- Le plafond de mémoire visé (1,2-1,5 Go) s'applique au scan avec génération
  de vignettes dans son ensemble, pas seulement au tout premier lancement :
  il vaut aussi pour l'ajout d'un nouveau dossier surveillé ou la reprise
  d'un scan interrompu.
- Un ralentissement de l'apparition des vignettes allant jusqu'à 20-30 % est
  accepté en échange de la réduction de mémoire ; au-delà, le compromis n'est
  plus acceptable et doit être revu.
- Cette fonctionnalité est un comportement interne de Picazoo : elle n'ajoute
  pas de nouveau réglage visible par l'utilisateur dans Outils, sauf si la
  phase de planification identifie un besoin réel de le rendre réglable.
- Les garanties du principe I de la constitution (aucune perte ni corruption
  de données utilisateur) et la fluidité visée par le principe III restent
  prioritaires sur la réduction de mémoire en cas de conflit.

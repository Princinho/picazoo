# Feature Specification: Gestion des doublons en lot

**Feature Branch**: `006-gestion-doublons-lot`

**Created**: 2026-11-23

**Status**: Draft

**Input**: User description: "La détection des doublons fonctionne, mais leur gestion reste à faire : aujourd'hui l'utilisateur ne peut agir que fichier par fichier, groupe par groupe, sans aucune option de traitement en lot. Il faut corriger cela."

## Clarifications

### Session 2026-11-23

- Q: Quand une règle « préférer ce dossier » désigne une copie de moins bonne qualité qu'une copie écartée, que suggérer ? → A: Qualité protégée : la règle s'applique toujours aux groupes « Identique » ; pour « Même image » et « Très ressemblante », elle ne s'applique pas si elle écarterait une copie RAW ou de plus grande résolution que la copie préférée, et le groupe est signalé.
- Q: Faut-il des filtres par taille de fichier ou par date dans la 006 ? → A: Oui, les deux : une taille minimale et une plage de dates.
- Q: Faut-il une confirmation supplémentaire au-delà d'un gros volume de fichiers ? → A: Non : le récapitulatif par niveau et par dossier, « Annuler » par défaut, la corbeille et l'annulation depuis le journal suffisent.

## Contexte

La 002 retrouve les doublons et permet de les trier, mais l'écran « Doublons »
a été pensé pour quelques dizaines de groupes. Sur une vraie bibliothèque, on
en trouve des centaines, voire des milliers : une sauvegarde recopiée dans un
autre dossier, un téléphone importé trois fois, un dossier « Exports » plein
de versions réduites. Aujourd'hui :

- chaque groupe occupe une grande carte : on en voit deux par écran, et il
  faut faire défiler des milliers de cartes ;
- les actions « Garder toutes », « Ce ne sont pas des doublons », « Revenir à
  la suggestion » et le choix de la copie gardée se font **un groupe à la
  fois**, par son menu ;
- impossible de dire « garde toujours les copies de `Images`, écarte celles de
  `D:\Sauvegarde` », alors que c'est la façon naturelle de raisonner quand une
  sauvegarde fait doublon avec l'original ;
- le seul traitement en lot (« Appliquer les suggestions à tous les groupes
  identiques ») est tout ou rien ;
- à l'inverse, le bouton « Mettre à la corbeille… » prend **tous** les groupes,
  y compris ceux que le filtre cache et ceux que l'utilisateur n'a jamais
  regardés, « Très ressemblante » compris : c'est contraire au principe IV
  (le logiciel propose, l'utilisateur décide) et doit être corrigé ici ;
- le récapitulatif liste les fichiers un par un, ce qui devient illisible
  au-delà de quelques dizaines ;
- une décision prise par erreur (« Garder toutes » sur le mauvais groupe) fait
  disparaître le groupe, sans moyen simple de le revoir.

La 006 donne à l'utilisateur les moyens de traiter des centaines de groupes
en quelques minutes **sans rien perdre des garde-fous de la 002** : rien ne
part à la corbeille sans récapitulatif ni confirmation, tout est journalisé et
annulable, jamais d'effacement définitif.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Sélectionner plusieurs groupes et agir sur eux en une fois (Priority: P1)

Chaque groupe a une case de sélection. L'utilisateur coche des groupes un par
un, en plage (Maj + clic), ou tous les groupes affichés d'un coup (Ctrl + A ou
« Tout sélectionner »). Dès qu'un groupe est sélectionné, une barre d'actions
en lot apparaît : « 37 groupes sélectionnés · 1,2 Go » avec « Revenir à la
suggestion », « Garder toutes », « Ce ne sont pas des doublons » et « Mettre à
la corbeille… ». Une action en lot s'applique à tous les groupes sélectionnés
en une fois, puis un message propose de l'annuler (« 37 groupes marqués
« Garder toutes ». Annuler »).

**Why this priority**: c'est le manque signalé ; sans sélection multiple,
aucune des autres histoires n'a de point d'appui.

**Independent Test**: avec 40 groupes « Identique », sélectionner les 10
premiers puis Maj + clic sur le 20ᵉ (20 groupes sélectionnés), choisir
« Garder toutes » : les 20 groupes disparaissent de la revue, les 20 autres
restent ; cliquer « Annuler » dans le message : les 20 groupes reviennent avec
leurs choix d'avant.

**Acceptance Scenarios**:

1. **Given** l'écran « Doublons » ouvert, **When** l'utilisateur coche la case
   d'un groupe, **Then** le groupe est marqué sélectionné et la barre d'actions
   en lot affiche le nombre de groupes et la place qu'ils libéreraient.
2. **Given** un groupe sélectionné, **When** l'utilisateur fait Maj + clic sur
   la case d'un autre groupe, **Then** tous les groupes affichés entre les deux
   sont sélectionnés.
3. **Given** un filtre actif, **When** l'utilisateur choisit « Tout
   sélectionner », **Then** tous les groupes affichés par ce filtre, et
   seulement eux, sont sélectionnés, même s'il y en a des milliers.
4. **Given** des groupes sélectionnés, **When** l'utilisateur choisit une
   action en lot, **Then** elle s'applique à tous en une fois et un message
   indique ce qui a été fait, avec « Annuler ».
5. **Given** des groupes sélectionnés, **When** l'utilisateur choisit « Mettre
   à la corbeille… », **Then** le récapitulatif ne porte que sur les groupes
   sélectionnés.
6. **Given** des groupes sélectionnés, **When** l'utilisateur change de
   filtre, **Then** les groupes qui ne sont plus affichés quittent la
   sélection, et la barre l'indique.
7. **Given** une sélection en cours, **When** l'utilisateur appuie sur Échap,
   **Then** la sélection est vidée (un second Échap revient à la
   bibliothèque).

---

### User Story 2 - Garder ou écarter selon le dossier (Priority: P1)

L'utilisateur ouvre « Dossiers » sur l'écran « Doublons » : la liste des
dossiers qui contiennent des doublons, triés par nombre de copies, et les
**paires de dossiers** qui partagent le plus de copies (« Images\2019 ↔
D:\Sauvegarde\2019 — 412 groupes · 1,8 Go »). Pour une paire, il choisit
« Garder celles de Images\2019 » : dans ces 412 groupes, la copie de
`Images\2019` devient la copie gardée et celle de la sauvegarde passe « À la
corbeille ». Il peut aussi faire de ce choix une **règle de dossier** retenue
pour l'avenir : « Toujours préférer `Images` » ou « Toujours écarter
`D:\Sauvegarde` ». Les règles modifient les suggestions, jamais les fichiers :
il faut toujours passer par le récapitulatif pour mettre à la corbeille.

**Why this priority**: c'est le cas le plus fréquent et le plus volumineux
(sauvegarde recopiée, import en double) ; il se règle en une décision au lieu
de centaines.

**Independent Test**: créer `Images\2019` avec 50 photos et
`Sauvegarde\2019` avec leurs 50 copies identiques, plus 5 photos en double à
l'intérieur de `Images\2019` ; dans « Dossiers », la paire apparaît avec
50 groupes ; « Garder celles de Images\2019 » : les 50 copies de la sauvegarde
sont cochées « À la corbeille », les 5 groupes internes à `Images\2019` ne
changent pas ; récapitulatif puis confirmation : `Sauvegarde\2019` est vide de
ces 50 copies, `Images\2019` intact.

**Acceptance Scenarios**:

1. **Given** des doublons répartis dans plusieurs dossiers, **When**
   l'utilisateur ouvre « Dossiers », **Then** il voit les dossiers concernés
   (nombre de copies, place) et les paires de dossiers qui partagent des
   copies, triées par place récupérable.
2. **Given** une paire de dossiers A ↔ B, **When** l'utilisateur choisit
   « Garder celles de A », **Then** dans chaque groupe qui a une copie dans A
   et une dans B, la copie de A est gardée et celle de B est cochée « À la
   corbeille », et rien d'autre ne change.
3. **Given** une paire de dossiers, **When** l'utilisateur clique dessus,
   **Then** la liste des groupes est filtrée sur les groupes qui ont une copie
   dans chacun des deux dossiers, prêts à être sélectionnés.
4. **Given** une règle « Toujours écarter D:\Sauvegarde », **When** un nouveau
   doublon apparaît avec une copie dans `D:\Sauvegarde\2024` et une ailleurs,
   **Then** la copie de la sauvegarde est suggérée pour la corbeille, avec la
   raison « Dossier à écarter : D:\Sauvegarde », et rien n'est mis à la
   corbeille sans le récapitulatif.
5. **Given** un groupe dont toutes les copies sont dans des dossiers à
   écarter, **When** les règles s'appliquent, **Then** la meilleure copie
   reste gardée selon la règle de la 002 et le groupe est signalé « Toutes les
   copies sont dans des dossiers à écarter ».
6. **Given** un choix fait à la main sur un groupe, **When** une règle de
   dossier est créée ensuite, **Then** le choix fait à la main est conservé ;
   « Revenir à la suggestion » applique la règle.
7. **Given** une règle de dossier, **When** l'utilisateur la supprime, **Then**
   les suggestions qui en dépendaient reviennent à la règle de la 002, sans
   toucher aux choix faits à la main.

---

### User Story 3 - Parcourir des milliers de groupes d'un coup d'œil (Priority: P2)

Une **vue compacte** affiche chaque groupe sur une seule ligne : case de
sélection, badge de niveau, petites vignettes des copies (la gardée avec son
étoile), nombre de copies, place récupérable, copie gardée et son dossier, et
d'où viennent les copies qui partent (« → corbeille : 3 dans
D:\Sauvegarde\2019 »). Un clic sur la ligne la déplie en carte détaillée,
comme aujourd'hui. Des filtres supplémentaires réduisent la liste : par
dossier, par type (photos, vidéos, RAW + JPEG), par état (suggestion intacte,
choix modifié, signalé), et la liste se trie aussi par dossier.

**Why this priority**: indispensable pour choisir quoi sélectionner sur une
grande bibliothèque, mais les US1 et US2 apportent déjà le gain principal.

**Independent Test**: sur une bibliothèque de test de 2 000 groupes, passer en
vue compacte : au moins 8 groupes tiennent dans un écran 1080p, le défilement
reste fluide ; filtrer sur le dossier `Sauvegarde` : seuls les groupes ayant
une copie dans `Sauvegarde` ou un de ses sous-dossiers restent affichés.

**Acceptance Scenarios**:

1. **Given** l'écran « Doublons », **When** l'utilisateur choisit la vue
   compacte, **Then** chaque groupe tient sur une ligne avec ses informations
   essentielles, et ce choix est retenu à la prochaine ouverture.
2. **Given** la vue compacte, **When** l'utilisateur clique sur une ligne,
   **Then** le groupe se déplie en carte détaillée avec toutes les actions de
   la 002 ; un nouveau clic le replie.
3. **Given** un filtre par dossier, **When** il est appliqué, **Then** seuls
   les groupes qui ont au moins une copie dans ce dossier ou ses
   sous-dossiers sont affichés.
4. **Given** un filtre « RAW + JPEG », **When** il est appliqué, **Then**
   seuls les groupes qui réunissent une photo RAW et une copie d'un autre
   format sont affichés.
5. **Given** un filtre « taille minimale 100 Mo », **When** il est appliqué,
   **Then** seuls les groupes dont au moins une copie pèse 100 Mo ou plus
   sont affichés ; **Given** une plage de dates, **When** elle est
   appliquée, **Then** seuls les groupes dont la copie gardée est datée dans
   cette plage sont affichés.
6. **Given** plusieurs filtres combinés, **When** l'utilisateur regarde
   l'en-tête, **Then** il voit le nombre de groupes affichés sur le total et
   peut retirer chaque filtre d'un clic.
7. **Given** la liste des dossiers de la bibliothèque, **When** l'utilisateur
   choisit « Voir les doublons de ce dossier » dans le menu d'un dossier,
   **Then** l'écran « Doublons » s'ouvre filtré sur ce dossier.

---

### User Story 4 - Un récapitulatif et une exécution à la mesure des gros lots (Priority: P2)

Pour une mise à la corbeille de centaines ou de milliers de fichiers, le
récapitulatif résume d'abord : nombre de fichiers et place libérée, répartis
par niveau de confiance et par dossier d'origine (« D:\Sauvegarde\2019 —
412 fichiers · 1,8 Go »). Chaque dossier se déplie pour montrer ses fichiers
et la copie gardée de chacun. Les groupes « Très ressemblante » sont présentés
à part et ne sont inclus que si l'utilisateur le décide. Une fois confirmée,
l'opération tourne en arrière-plan avec sa progression ; l'utilisateur peut
continuer à se servir de Picazoo, et peut l'arrêter : ce qui est déjà à la
corbeille y reste, inscrit au journal et annulable.

**Why this priority**: sans cela, les US1 et US2 produisent des lots
impossibles à vérifier ; mais un récapitulatif plat reste juste pour de petits
lots.

**Independent Test**: sélectionner 300 groupes (200 « Identique », 80 « Même
image », 20 « Très ressemblante » non modifiés) et ouvrir le récapitulatif :
il montre 3 sections par niveau, la troisième non incluse, et une liste par
dossier ; confirmer : 280 groupes sont traités, les 20 « Très ressemblante »
sont intacts ; relancer sur 2 000 fichiers et arrêter à mi-parcours : le
journal montre exactement les fichiers déjà traités, et « Annuler cette
opération » les restaure.

**Acceptance Scenarios**:

1. **Given** un lot de plusieurs centaines de fichiers, **When** le
   récapitulatif s'ouvre, **Then** il affiche en tête le total, la répartition
   par niveau et la répartition par dossier d'origine, en moins de 2 secondes,
   sans figer l'interface.
2. **Given** le récapitulatif, **When** l'utilisateur déplie un dossier,
   **Then** il voit chaque fichier qui part et la copie gardée correspondante.
3. **Given** un lot qui contient des groupes « Très ressemblante » dont
   l'utilisateur n'a modifié aucun choix, **When** le récapitulatif s'ouvre,
   **Then** ces groupes sont dans une section à part, non incluse par défaut.
4. **Given** une opération en cours, **When** l'utilisateur continue à
   parcourir la bibliothèque ou les doublons, **Then** Picazoo reste réactif
   et la progression est visible dans la barre d'actions.
5. **Given** une opération en cours, **When** l'utilisateur choisit
   « Arrêter », **Then** elle s'arrête en moins de 2 secondes, les fichiers
   déjà traités sont au journal, l'opération est marquée « Arrêtée » et peut
   être annulée.
6. **Given** un groupe modifié par l'analyse entre la sélection et la
   confirmation (nouvelle copie, copie disparue), **When** l'opération
   s'exécute, **Then** la vérification de la 002 (FR-019 de la 002)
   s'applique : rien n'est fait sur une analyse périmée.

---

### User Story 5 - Revenir sur des décisions prises en lot (Priority: P3)

Les groupes que l'utilisateur a sortis de la revue (« Garder toutes », « Ce ne
sont pas des doublons », copies retirées d'un groupe) restent consultables
dans un filtre « Écartés de la revue ». Il peut les sélectionner et les
remettre dans la revue en une fois. Chaque action en lot y apparaît comme un
ensemble (« 23 novembre, 14 h 02 — 37 groupes, Garder toutes ») qu'il peut
annuler d'un coup, même après la disparition du message « Annuler ».

**Why this priority**: filet de sécurité pour les erreurs de lot ; le message
« Annuler » de l'US1 couvre déjà l'erreur immédiate.

**Independent Test**: marquer 15 groupes « Ce ne sont pas des doublons » en
lot, fermer et relancer Picazoo ; dans « Écartés de la revue », retrouver le
lot de 15 et choisir « Remettre dans la revue » : les 15 groupes réapparaissent
dans la revue après la mise à jour des groupes.

**Acceptance Scenarios**:

1. **Given** des groupes sortis de la revue, **When** l'utilisateur choisit le
   filtre « Écartés de la revue », **Then** il les voit avec la décision prise
   et sa date.
2. **Given** des groupes écartés sélectionnés, **When** l'utilisateur choisit
   « Remettre dans la revue », **Then** les décisions correspondantes sont
   annulées et les groupes reviennent dans la revue.
3. **Given** une action en lot passée, **When** l'utilisateur l'annule depuis
   « Écartés de la revue », **Then** tous les groupes concernés retrouvent
   leur état d'avant cette action, y compris après un redémarrage.

---

### Edge Cases

- **Sélection très grande** (10 000 groupes) : la sélection, le compte et
  la place affichés dans la barre sont immédiats ; le récapitulatif se
  prépare sans figer l'interface.
- **Groupe modifié pendant que l'utilisateur sélectionne** (l'analyse
  continue en fond) : la sélection suit le groupe ; les chiffres de la barre
  se mettent à jour ; le récapitulatif reflète l'état au moment où il s'ouvre,
  et la vérification juste avant d'agir (002) reste la dernière barrière.
- **Groupe qui disparaît de la sélection** (résolu par une autre opération,
  ou tombé à une seule copie) : il quitte la sélection silencieusement ; la
  barre affiche le nouveau compte.
- **Règles en conflit** (préférer `Images`, écarter `Images\Exports`) : la
  règle du dossier le plus précis l'emporte.
- **Plusieurs copies d'un groupe dans le dossier préféré** : la règle de la
  002 départage ces copies entre elles ; les autres copies du dossier préféré
  partent à la corbeille comme d'habitude.
- **Toutes les copies dans des dossiers à écarter** : la meilleure copie est
  gardée (jamais aucune copie en place, FR-017 de la 002) et le groupe est
  signalé.
- **Copie préférée de moins bonne qualité** (la copie du dossier préféré est
  une version réduite, l'autre l'original) : voir FR-006.
- **Dossier d'une règle renommé, déplacé ou plus surveillé** : la règle est
  conservée mais marquée « dossier introuvable » et ignorée ; l'utilisateur
  peut la supprimer ou la rattacher à un autre dossier.
- **Paire de dossiers sur un disque débranché** : la paire reste listée, ses
  copies indisponibles ne peuvent pas partir à la corbeille (002).
- **Copies dans le même dossier** (`IMG_0042.jpg` et `IMG_0042 (1).jpg`) :
  elles apparaissent dans la liste des dossiers comme « doublons internes » ;
  la règle de la 002 garde déjà le nom le plus court.
- **Arrêt d'une opération au milieu d'un fichier** : le fichier en cours finit
  d'être traité ou n'est pas touché, jamais à moitié.
- **Fermeture de Picazoo pendant une opération en lot** : comme dans la 002,
  l'opération est reprise en compte au lancement suivant et marquée « Fait en
  partie » ou « Arrêtée », avec le détail exact.
- **Annulation d'une action en lot dont une partie a été modifiée depuis** (un
  groupe du lot a été remis en revue à la main) : seuls les groupes encore
  dans l'état laissé par le lot sont rétablis ; les autres sont signalés.
- **Lot qui mélange photos et vidéos** : traité de la même façon ; les vidéos
  ne sont de toute façon regroupées que si elles sont identiques (002).

## Requirements *(mandatory)*

### Functional Requirements

**Sélection**

- **FR-001**: Chaque groupe de l'écran « Doublons » MUST pouvoir être
  sélectionné et désélectionné individuellement, en plage (Maj + clic) et en
  totalité (« Tout sélectionner », Ctrl + A) ; « Tout sélectionner »
  sélectionne exactement les groupes affichés par les filtres en cours.
- **FR-002**: Le système MUST afficher en permanence, quand la sélection n'est
  pas vide, le nombre de groupes sélectionnés, le nombre de fichiers qui
  partiraient à la corbeille et la place libérée, mis à jour à chaque
  changement.
- **FR-003**: Changer de filtre MUST retirer de la sélection les groupes qui ne
  sont plus affichés ; une action en lot MUST NOT porter sur un groupe qui
  n'est pas affiché.
- **FR-004**: Les actions en lot MUST être : « Revenir à la suggestion »,
  « Garder les copies du dossier… » (choix parmi les dossiers présents dans la
  sélection), « Garder toutes », « Ce ne sont pas des doublons » et « Mettre à
  la corbeille… ». Chacune MUST s'appliquer à tous les groupes sélectionnés en
  une seule fois.

**Règles de dossier**

- **FR-005**: Le système MUST présenter la liste des dossiers qui contiennent
  des doublons (nombre de copies, place) et la liste des paires de dossiers
  qui partagent des copies (nombre de groupes, place récupérable), triées par
  place récupérable ; les doublons internes à un dossier MUST y figurer
  distinctement.
- **FR-006**: Les utilisateurs MUST pouvoir, pour une paire de dossiers,
  garder les copies de l'un et écarter celles de l'autre en une action, et
  créer des règles durables « Toujours préférer ce dossier » ou « Toujours
  écarter ce dossier » qui s'appliquent au dossier et à ses sous-dossiers.
  Pour les groupes « Identique », une règle de dossier passe avant les
  critères de la 002. Pour les groupes « Même image » et « Très
  ressemblante », elle passe avant les critères de la 002 **sauf** si elle
  écarterait une copie RAW ou de plus grande résolution que la copie
  désignée par la règle : la suggestion de la 002 est alors conservée et le
  groupe est signalé « La copie du dossier préféré est moins bonne ».
- **FR-007**: Les règles de dossier MUST seulement modifier la copie suggérée
  à garder et sa raison affichée (« Dossier préféré : … », « Dossier à
  écarter : … ») ; elles MUST NOT déclencher aucune action sur les fichiers.
- **FR-008**: Un choix fait à la main sur une copie MUST l'emporter sur les
  règles de dossier ; « Revenir à la suggestion » MUST appliquer la suggestion
  qui tient compte des règles.
- **FR-009**: Les règles de dossier MUST être conservées entre les sessions,
  consultables et supprimables ; la règle du dossier le plus précis MUST
  l'emporter ; une règle dont le dossier est introuvable MUST être signalée et
  ignorée.
- **FR-010**: Quand toutes les copies d'un groupe sont dans des dossiers à
  écarter, le système MUST garder la meilleure copie selon la règle de la 002
  et signaler le groupe.

**Parcours et filtres**

- **FR-011**: L'écran « Doublons » MUST proposer une vue compacte (un groupe
  par ligne : sélection, niveau, vignettes réduites, nombre de copies, place,
  copie gardée et son dossier, provenance des copies qui partent) et la vue
  détaillée de la 002 ; une ligne compacte MUST pouvoir être dépliée en carte
  détaillée ; le choix de vue MUST être retenu.
- **FR-012**: Les filtres MUST pouvoir se combiner : niveau de confiance (002),
  dossier (copie dans ce dossier ou un sous-dossier), paire de dossiers, type
  (photos, vidéos, RAW + JPEG), état (suggestion intacte, choix modifié,
  signalé), taille minimale (au moins une copie du groupe atteint cette
  taille), plage de dates (date de prise de vue, ou du fichier à défaut, de
  la copie gardée) et « Écartés de la revue » (FR-021). Le tri MUST proposer
  en plus de la 002 le tri par dossier.
- **FR-013**: L'en-tête MUST indiquer le nombre de groupes affichés sur le
  total et permettre de retirer chaque filtre d'un clic.
- **FR-014**: Le menu d'un dossier dans la liste des dossiers de la
  bibliothèque MUST proposer « Voir les doublons de ce dossier », qui ouvre
  l'écran « Doublons » filtré sur ce dossier.

**Récapitulatif et exécution**

- **FR-015**: « Mettre à la corbeille… » MUST porter sur les groupes
  sélectionnés, ou, sans sélection, sur le groupe courant seulement ; il MUST
  NOT porter sur des groupes cachés par les filtres. Le bouton « Appliquer les
  suggestions à tous les groupes identiques » de la 002 est remplacé par
  « Tout sélectionner » sur le filtre « Identiques » suivi de « Mettre à la
  corbeille… ».
- **FR-016**: Le récapitulatif MUST afficher en tête le nombre de fichiers et
  la place libérée, répartis par niveau de confiance et par dossier d'origine ;
  chaque dossier MUST pouvoir être déplié pour montrer ses fichiers et la
  copie gardée de chacun ; il conserve tout ce qu'exige la 002 (fichiers non
  touchés et pourquoi, groupes refusés, « Annuler » par défaut).
- **FR-017**: Le récapitulatif MUST se préparer sans bloquer l'interface et
  s'afficher en moins de 2 secondes pour 10 000 fichiers.
- **FR-018**: Les groupes « Très ressemblante » dont l'utilisateur n'a modifié
  aucun choix MUST apparaître dans une section à part du récapitulatif, non
  incluse par défaut ; l'utilisateur peut l'inclure explicitement.
- **FR-019**: Une opération confirmée MUST s'exécuter en arrière-plan, avec sa
  progression visible et Picazoo utilisable pendant ce temps ; les
  utilisateurs MUST pouvoir l'arrêter : elle s'interrompt entre deux fichiers
  en moins de 2 secondes, ce qui est fait reste au journal, l'opération est
  marquée « Arrêtée » et reste annulable (FR-021 de la 002).
- **FR-020**: Une opération en lot MUST être inscrite au journal comme une
  seule opération. Aucune confirmation supplémentaire n'est demandée selon le
  volume : le bouton de confirmation indique le nombre de fichiers
  (« Mettre 2 140 fichiers à la corbeille ») et « Annuler » reste le bouton
  par défaut, quelle que soit la taille du lot.

**Décisions en lot et retour en arrière**

- **FR-021**: Les groupes sortis de la revue (« Garder toutes », « Ce ne sont
  pas des doublons », copies retirées) MUST rester consultables dans un filtre
  « Écartés de la revue », avec la décision et sa date, et pouvoir être remis
  dans la revue, un par un ou en lot.
- **FR-022**: Chaque action en lot sur les décisions MUST être annulable
  d'un coup : immédiatement par le message « Annuler », puis à tout moment
  depuis « Écartés de la revue », y compris après un redémarrage ; seuls les
  groupes encore dans l'état laissé par l'action sont rétablis, les autres
  sont signalés.
- **FR-023**: Toutes les actions en lot MUST être réalisables au clavier
  (sélection, plage, tout sélectionner, actions, Échap pour vider la
  sélection).

**Garanties**

- **FR-024**: Toutes les garanties de la 002 restent valables pour les
  actions en lot : récapitulatif et confirmation explicite avant toute mise à
  la corbeille, au moins une copie gardée par groupe, vérification juste avant
  d'agir, corbeille du système uniquement, journal et annulation, aucune
  écriture dans les dossiers de l'utilisateur.
- **FR-025**: Une action en lot sur les décisions MUST s'appliquer en entier
  ou pas du tout (pas d'état à moitié appliqué en cas d'erreur ou de
  fermeture).

### Key Entities

- **Sélection de groupes** : ensemble des groupes choisis par l'utilisateur
  sur l'écran « Doublons », limité aux groupes affichés ; non conservée entre
  les sessions.
- **Règle de dossier** : dossier (et ses sous-dossiers) marqué « préférer » ou
  « écarter » ; date de création ; état (active, dossier introuvable). Elle
  oriente la suggestion de la copie gardée, rien d'autre.
- **Paire de dossiers** : deux dossiers qui contiennent chacun une copie d'un
  même groupe ; nombre de groupes, place récupérable. Calculée, non stockée
  comme décision.
- **Action en lot** : une décision appliquée à plusieurs groupes en une fois ;
  date, type, groupes et copies concernés, état d'avant ; de quoi l'annuler.
- **Opération sur les fichiers** (002) : gagne l'état « Arrêtée ».

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Un dossier de sauvegarde qui fait doublon avec 500 photos de la
  bibliothèque est vidé de ses copies (de l'ouverture de l'écran « Doublons »
  à la confirmation) en moins d'une minute et en au plus 5 actions avant la
  confirmation.
- **SC-002**: Sur 10 000 groupes, « Tout sélectionner » et l'affichage du
  compte de la sélection prennent moins de 200 ms ; le récapitulatif s'ouvre
  en moins de 2 secondes ; l'interface ne se fige jamais plus de 100 ms.
- **SC-003**: En vue compacte, au moins 8 groupes tiennent dans un écran
  1080p (contre 2 aujourd'hui), et le défilement de 10 000 groupes reste
  fluide.
- **SC-004**: Sur un jeu de test, 0 fichier mis à la corbeille n'appartient à
  un groupe absent du récapitulatif, et 0 groupe « Très ressemblante » non
  modifié n'est traité sans avoir été inclus explicitement.
- **SC-005**: Annuler une action en lot rétablit exactement l'état d'avant
  (choix et groupes) pour 100 % des groupes non modifiés depuis, y compris
  après un redémarrage.
- **SC-006**: Une opération de 5 000 fichiers s'arrête en moins de
  2 secondes après « Arrêter », et 100 % des fichiers déjà traités figurent au
  journal et sont restaurables.
- **SC-007**: Un utilisateur qui découvre la fonctionnalité nettoie une
  sauvegarde en double à l'aide des paires de dossiers sans explication
  (vérifié auprès de 5 personnes).

## Assumptions

- La 002 est en place (groupes, choix, décisions, corbeille, journal) ; la
  006 l'étend sans changer la détection ni la règle de qualité, sauf l'ajout
  des règles de dossier dans la suggestion.
- FR-022 de la 002 (application en lot limitée aux groupes « Identique ») est
  remplacée par la sélection de la 006 : l'utilisateur peut sélectionner des
  groupes de tout niveau, avec le garde-fou de FR-018 pour « Très
  ressemblante ».
- La seule opération sur les fichiers reste la mise à la corbeille du
  système ; le déplacement vers un autre dossier relève de la 004
  (réorganisation), la suppression définitive et la fusion des informations
  entre copies restent hors périmètre.
- Les règles de dossier sont des préférences de Picazoo, conservées dans son
  propre stockage ; elles ne sont jamais écrites dans les dossiers de
  l'utilisateur.
- La sélection n'est pas conservée entre les sessions ; les règles et les
  actions en lot le sont.
- L'interface est en français ; les raccourcis de la 002 sont conservés, la
  touche Échap vide d'abord la sélection.
- Ordinateur visé : portable courant, peu puissant (comme la 002) ; les
  seuils de SC-002, SC-003 et SC-006 sont mesurés dans ces conditions.

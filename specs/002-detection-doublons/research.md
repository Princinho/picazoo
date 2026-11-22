# Research: Détection des doublons

Fonctionnalité 002 — décisions techniques. Deux contraintes dominent :
le **principe I** (la mise à la corbeille est la première opération de
Picazoo qui touche aux fichiers) et la **discrétion disque** (l'analyse ne
doit jamais saturer le disque d'un portable peu puissant, problème déjà
rencontré avec la 001).

Les décisions reprennent la pile de la 001 (C++20, Qt 6.12, SQLite,
xxHash, TurboJPEG) et **n'ajoutent aucune dépendance vcpkg**. Les numéros
R1–R13 renvoient à la [research de la 001](../001-bibliotheque-visionneuse/research.md)
quand ils sont cités « 001-R… ».

Aucun `NEEDS CLARIFICATION` ne reste ouvert. Les points marqués
**[à valider au spike]** sont des hypothèses fortes à confirmer par la
première tâche technique.

---

## D1 — Doublons exacts : du filtre gratuit au hachage complet

- **Decision**: trois étages, du moins cher au plus cher :
  1. **Taille** (`item.size_bytes`, déjà dans l'index) : un élément dont la
     taille est unique dans la bibliothèque n'a pas de doublon exact ; il
     n'est jamais lu.
  2. **Empreinte rapide** (`item.quick_hash`, XXH3-64 de la taille + 64 Ko
     du début + 64 Ko de la fin, déjà calculée par la 001-R4) : seuls les
     éléments qui partagent `(size_bytes, quick_hash)` avec au moins un
     autre deviennent **candidats**.
  3. **Empreinte de contenu** : XXH3-128 du fichier entier, calculée
     uniquement pour les candidats, par la lecture régulée de D5.
  Deux éléments de même empreinte de contenu et de même taille sont
  « Identique ». Deux chemins qui ont le même `(volume_serial, file_id)`
  sont un seul fichier (liens physiques, FR-006) : ils sont fusionnés avant
  le regroupement et jamais proposés.
- **Rationale**: sur une bibliothèque de photos, la taille seule élimine la
  très grande majorité des éléments, et l'empreinte rapide élimine presque
  tous les faux candidats restants ; on ne lit donc en entier que des
  fichiers qui sont presque sûrement des doublons (FR-028). XXH3-128 est
  déjà lié (xxHash), tourne à plusieurs Go/s (le disque est le seul goulot)
  et rend une collision accidentelle négligeable (2⁻¹²⁸). La vérification
  avant suppression (D8) refait de toute façon le hachage des fichiers
  concernés.
- **Alternatives considered**: SHA-256 (rejeté : plus lent, aucune menace
  adverse ici) ; s'arrêter à l'empreinte rapide (rejeté : « Identique » doit
  vouloir dire identique, une retouche au milieu du fichier qui garde la
  taille passerait) ; comparaison octet par octet des paires (rejeté : coût
  quadratique dans les gros groupes, l'empreinte suffit).

## D2 — Empreinte visuelle : calculée depuis les vignettes, sans relire les photos

- **Decision**: l'empreinte visuelle est calculée à partir de la **vignette
  finale de 320 px** déjà stockée dans `thumbs.db` (001-R6), jamais à partir
  du fichier original. Pour chaque photo (images et RAW ; pas les vidéos) :
  1. décoder la vignette (TurboJPEG, réduction ×1/4 → ~80 px), passer en
     luminance, ramener à 32×32 ;
  2. **pHash** : DCT 32×32, coefficients basse fréquence 8×8 hors composante
     continue, comparés à leur médiane → 64 bits ;
  3. calculer aussi les pHash des **trois autres quarts de tour** (90°, 180°,
     270°) pour retrouver une copie tournée (FR-002) ;
  4. **score de détail** : énergie des coefficients AC ; en dessous d'un
     seuil, l'image est « pauvre en détails » et ne sera regroupée que par
     son empreinte de contenu (FR-003) ;
  5. conserver le **rapport largeur/hauteur** (déjà dans `item`).
  Une photo n'est traitée qu'une fois sa vignette finale disponible
  (`thumb_state = done`) ; les vignettes provisoires ne servent pas.
- **Rationale**: c'est la décision qui rend la 002 discrète : la partie
  visuelle ne lit **aucun fichier de l'utilisateur**, seulement `thumbs.db`
  (quelques Ko par photo), et décoder 20 000 vignettes de 320 px coûte
  quelques dizaines de secondes de calcul. L'orientation EXIF est déjà
  appliquée aux vignettes (001), donc une copie « physiquement tournée » et
  une copie « tournée par EXIF » se ressemblent sans traitement. Les RAW
  sont comparés par leur aperçu intégré, comme la 001 les affiche, ce qui
  rend la paire RAW + JPEG naturellement comparable (clarification du
  2026-11-22). Le pHash résiste au redimensionnement, à la recompression,
  au changement de format et aux retouches légères de luminosité et de
  contraste. Aucune dépendance : une DCT 32×32 tient en quelques dizaines
  de lignes.
- **Alternatives considered**: aHash/dHash seuls (rejetés : trop sensibles
  aux retouches de luminosité, plus de faux positifs) ; descripteurs locaux
  (ORB/SIFT via OpenCV) ou réseau de neurones d'empreinte (rejetés : grosse
  dépendance, calcul lourd sur un portable basse consommation, utile
  surtout pour les recadrages, hors périmètre) ; empreinte calculée sur
  l'original (rejeté : relirait toute la bibliothèque, des dizaines de Go).

## D3 — Trouver les paires candidates

- **Decision**: toutes les empreintes visuelles (un `uint64` par photo et par
  rotation) sont chargées en mémoire dans un tableau contigu (100 000 photos
  ≈ 3,2 Mo pour les quatre rotations). Recherche par **force brute avec
  `popcount`** :
  - **analyse complète** : chaque photo est comparée aux suivantes, par
    tranches de 2 000, avec un point de reprise et une pause possible entre
    deux tranches ;
  - **incrémental** : une photo nouvelle ou modifiée est comparée à toute la
    bibliothèque (100 000 × 4 comparaisons, moins d'une milliseconde).
  Une paire est candidate si la distance de Hamming minimale sur les quatre
  rotations est ≤ 12 **et** si les rapports largeur/hauteur concordent à 2 %
  près (en tenant compte de la rotation).
- **Rationale**: 100 000² / 2 ≈ 5 × 10⁹ comparaisons de 64 bits : quelques
  secondes par cœur, une seule fois ; le code est simple, déterministe et
  facile à interrompre. Un index (arbre BK, hachage multi-index) ne se
  justifie pas à cette échelle.
- **Alternatives considered**: arbre BK (rejeté : plus de code, gain
  inutile à 100 000 éléments) ; requêtes SQL sur les empreintes (rejeté :
  SQLite ne sait pas indexer une distance de Hamming).

## D4 — Vérifier, qualifier et écarter les fausses ressemblances

- **Decision**: chaque paire candidate passe une **vérification** avant
  d'être regroupée :
  1. **Prises de vue distinctes** : si les deux photos ont une date de prise
     de vue EXIF et que ces dates diffèrent, ce sont deux photos
     différentes (rafale, série) → jamais regroupées. Une copie réduite,
     recompressée ou convertie garde sa date EXIF ou la perd, mais ne la
     change pas.
  2. **Ressemblance fine** : on compare les deux vignettes ramenées à 64×64
     en luminance, après normalisation de la luminosité et du contraste
     (moyenne et écart-type), par un **SSIM** global sur fenêtres 8×8, dans
     la rotation retenue à l'étape D3.
  3. **Qualification** :
     - SSIM ≥ 0,95 → « Même image » ;
     - 0,85 ≤ SSIM < 0,95 → « Très ressemblante » ;
     - en dessous → paire rejetée.
     Les seuils sont **[à valider au spike]** sur le jeu de test de D11, avec
     pour objectif SC-001 (≥ 95 % des copies retrouvées) et SC-002 (aucun
     « Même image » et moins de 1 % de « Très ressemblante » sur des photos
     distinctes).
  4. Les photos pauvres en détails (D2-4) ne passent pas par cette voie.
  Les vignettes décodées pour la vérification passent par un petit cache
  LRU (64×64 luminance, 4 Ko chacune, 2 000 entrées).
- **Rationale**: le pHash seul confond les photos d'une rafale ; la date
  EXIF est le discriminant le plus sûr et le moins cher (déjà dans
  `item.taken_at`). Pour les rafales prises dans la même seconde ou les
  photos sans EXIF, le SSIM sur une image normalisée sépare un léger
  déplacement de sujet d'une simple recompression. La normalisation rend
  la comparaison insensible à une retouche de luminosité ou de contraste,
  qui reste qualifiée « Très ressemblante » si l'écart est visible.
- **Alternatives considered**: seuil unique sur la distance de Hamming
  (rejeté : confond rafales et retouches) ; SSIM pleine résolution sur les
  originaux (rejeté : relit les fichiers).

## D5 — Lecture régulée : ne jamais saturer le disque

- **Decision**: toutes les lectures complètes de fichiers par la 002
  (empreinte de contenu, D1-3) passent par un **lecteur régulé**
  (`platform/win/ThrottledReader`) et un **régulateur** commun
  (`IoGovernor`) :
  1. **Priorité basse du système** : le fil de la 002 entre en
     `THREAD_MODE_BACKGROUND_BEGIN`, ce qui abaisse à la fois sa priorité
     processeur, sa priorité d'entrée-sortie et sa priorité mémoire.
  2. **Pas de pollution du cache** : ouverture en lecture partagée avec
     `FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN`, par blocs de
     1 Mo alignés. Lire des Go en mode normal évince du cache de Windows
     les fichiers des autres logiciels, une cause classique de ralentissement
     général sur une machine qui a peu de mémoire.
  3. **Débit plafonné et adaptatif** : seau à jetons, débit initial 16 Mo/s,
     entre 2 et 32 Mo/s. Le régulateur mesure le temps de chaque bloc et
     garde une latence de référence (médiane glissante au repos) ; si la
     latence dépasse 3 fois la référence (autre application sur le disque),
     le débit est divisé par deux, puis il remonte de 10 % par seconde
     calme (FR-027, SC-005).
  4. **Un seul gros travail à la fois** : l'étage de lecture de la 002 ne
     tourne que si l'indexeur de la 001 est inactif (`Library::indexerIdle()`)
     et que le pipeline des vignettes n'a plus rien en attente ; il se met
     en attente dès que l'un des deux reprend.
  5. **Retrait devant l'utilisateur** : comme le pipeline de la 001
     (001-R12), la 002 se suspend pendant l'interaction dans la visionneuse
     (`hold()`/`release()`).
  6. **Batterie** : sur batterie ou en mode économiseur d'énergie
     (`GetSystemPowerStatus`, notification `GUID_ACDC_POWER_SOURCE`),
     l'analyse se met en pause ; elle reprend sur secteur (FR-031).
  L'étage visuel (D2), qui ne lit que `thumbs.db`, tourne au même niveau de
  priorité mais sans plafond de débit.
- **Rationale**: le plafond seul ne suffit pas (un disque déjà occupé peut
  saturer à 16 Mo/s), la priorité seule non plus (Windows ne régule pas un
  lecteur séquentiel à faible priorité s'il n'y a pas de concurrence
  visible). La combinaison priorité basse + pas de cache + débit adaptatif
  + exclusion mutuelle avec la 001 couvre les causes connues de
  ralentissement général.
  **Coordination** : un correctif de la 001 contre la saturation du disque
  au lancement est en cours dans une autre branche. Si ce correctif
  introduit lui-même un régulateur d'E/S, la 002 l'adopte au lieu d'en
  créer un ; sinon `IoGovernor` est créé dans `platform/win/` de façon que
  la 001 puisse l'adopter ensuite. **La 002 ne modifie pas `FileIO.cpp`** ;
  à trancher au moment de `/speckit-tasks`, après rebasage sur ce correctif.
- **Alternatives considered**: `SetFileInformationByHandle(FileIoPriorityHintInfo)`
  par fichier seulement (retenu comme complément si le mode d'arrière-plan
  du fil ne suffit pas, **[à valider au spike]**) ; lecture mappée en
  mémoire (rejetée : pollue le cache comme une lecture normale) ; aucune
  limite et confiance dans l'ordonnanceur (rejeté : c'est le comportement
  qui a figé l'ordinateur avec la 001).

## D6 — Fichiers « disponibles uniquement en ligne »

- **Decision**: avant toute ouverture de contenu, le lecteur régulé lit les
  attributs (`GetFileAttributesW`, qui ne déclenche pas de téléchargement)
  et refuse les fichiers marqués `FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS`,
  `FILE_ATTRIBUTE_RECALL_ON_OPEN` ou `FILE_ATTRIBUTE_OFFLINE` ; l'élément
  passe à l'état `skipped_online` (FR-032). La partie visuelle n'est pas
  concernée : elle ne lit que les vignettes.
- **Rationale**: lire un tel fichier déclencherait son téléchargement depuis
  OneDrive (ou équivalent) : réseau, disque et quota consommés à l'insu de
  l'utilisateur.
- **Alternatives considered**: ignorer le problème (rejeté : contraire au
  principe II d'esprit et à FR-032).

## D7 — Former les groupes et choisir la copie à garder

- **Decision**:
  1. **Exact d'abord** : les éléments de même empreinte de contenu forment
     des « ensembles identiques ». Pour la partie visuelle, un ensemble
     identique compte pour une seule image (son meilleur représentant).
  2. **Regroupement en étoile** autour de la copie à garder (FR-007) : les
     images reliées par des paires vérifiées (D4) sont triées selon la
     règle de FR-008 ; la meilleure non encore placée devient le **centre**
     d'un groupe, et rejoignent ce groupe les images dont la paire *avec ce
     centre* est vérifiée. On recommence avec la meilleure image restante.
     Il n'y a donc jamais de chaîne A–B–C où A et C ne se ressemblent pas.
  3. **Règle de la copie à garder** (FR-008), dans l'ordre : RAW avant tout
     autre format ; plus grand nombre de pixels ; à pixels égaux, le plus
     gros fichier (moins compressé) ; présence d'une date de prise de vue
     EXIF ; date la plus ancienne ; chemin le plus court. La raison retenue
     est le premier critère qui départage (affichée : « RAW d'origine »,
     « Plus grande résolution », « Moins compressée », « Informations de
     prise de vue », « La plus ancienne »).
  4. **Niveau d'un groupe** : chaque copie a son niveau par rapport au
     centre (« Identique », « Même image », « Très ressemblante ») ; le
     niveau affiché du groupe est le plus faible de ses copies, et le
     filtre de l'écran de revue s'applique à ce niveau.
  5. **Incrémental** : une image nouvelle ou modifiée est comparée à toute
     la bibliothèque (D3), puis vérifiée contre les centres des groupes
     touchés. Si elle devient la meilleure copie d'un groupe, elle en
     devient le centre, et les copies qui ne lui ressemblent pas
     directement sont détachées puis replacées.
  6. **Décisions de l'utilisateur** (FR-013, FR-014) : une paire marquée
     « Ce ne sont pas des doublons » n'est plus jamais vérifiée ; un choix
     de copie gardée remplace la suggestion tant que les copies existent ;
     « Garder toutes » masque le groupe tant qu'aucune nouvelle copie n'y
     entre. Ces décisions sont liées aux `item.id`, stables pour les
     renommages et déplacements (001-R4).
- **Rationale**: le regroupement en étoile garantit FR-007 par
  construction ; le centre est la copie suggérée, donc chaque copie
  affichée a été comparée directement à celle qu'on garde.
- **Alternatives considered**: composantes connexes du graphe de
  ressemblance (rejeté : crée des chaînes) ; regroupement hiérarchique
  (rejeté : plus coûteux, résultat moins lisible).

## D8 — Mise à la corbeille sans aucun risque d'effacement définitif

- **Decision**: la mise à la corbeille passe par `IFileOperation`
  (`FOFX_RECYCLEONDELETE | FOF_ALLOWUNDO | FOF_NOCONFIRMATION |
  FOF_NOERRORUI | FOF_SILENT`) avec un **puits de progression**
  (`IFileOperationProgressSink`) :
  1. **Avant** : pour chaque volume concerné, `SHQueryRecycleBinW` doit
     réussir et le type de lecteur doit être fixe (`DRIVE_FIXED`) ; sinon
     les fichiers de ce volume sont marqués « non supprimables par
     Picazoo » (FR-018, clarification du 2026-11-22) et ne sont pas confiés
     au système.
  2. **Pendant** : dans `PreDeleteItem`, si le drapeau
     `TSF_DELETE_RECYCLE_IF_POSSIBLE` est absent (le système s'apprête à
     effacer définitivement : fichier plus gros que la corbeille, corbeille
     désactivée…), le puits renvoie un échec et l'élément est **sauté**.
     C'est le verrou de sécurité : `FOF_NOCONFIRMATION` sans ce contrôle
     pourrait effacer définitivement en silence.
  3. **Vérification préalable** (FR-019), juste avant d'agir, pour chaque
     fichier à écarter **et** la copie gardée de son groupe : même
     `(volume_serial, file_id)`, même taille, même date de modification
     que dans l'index ; pour un groupe « Identique », l'empreinte de contenu
     est recalculée sur les deux (lecture prioritaire, non plafonnée, avec
     barre de progression). Tout écart → fichier laissé en place, groupe
     réanalysé, signalé dans le compte rendu.
  4. **Après** : le compte rendu par fichier (`PostDeleteItem`) alimente le
     journal (D9) ; les éléments mis à la corbeille sont retirés de l'index
     (via la tâche d'indexation de la 001, `Library::post()`), avec leur
     vignette, ce qui met la bibliothèque à jour immédiatement (FR-023).
  5. Le service refuse une opération qui ne laisserait aucune copie d'un
     groupe (FR-017), indépendamment de l'interface.
  L'opération tourne sur un fil dédié, jamais sur le fil de l'interface.
  **[à valider au spike]** : comportement exact de `TSF_DELETE_RECYCLE_IF_POSSIBLE`
  sur Windows 10 et 11 pour un fichier sur clé USB, sur partage réseau et
  plus gros que la corbeille.
- **Rationale**: `IFileOperation` est l'API moderne de l'Explorateur ; elle
  gère les chemins longs, les noms Unicode et la corbeille de chaque volume.
  Le double verrou (contrôle préalable du volume + refus dans
  `PreDeleteItem`) rend l'effacement définitif impossible par construction,
  ce qu'exige le principe I.
- **Alternatives considered**: `SHFileOperationW` avec `FOF_ALLOWUNDO`
  (rejeté : API ancienne, efface définitivement sans prévenir quand la
  corbeille est indisponible et que les confirmations sont coupées) ;
  déplacement vers un dossier de quarantaine (rejeté par la clarification
  du 2026-11-22).

## D9 — Journal et annulation

- **Decision**:
  - **Journal** : tables `file_operation` et `file_operation_entry` dans
    `library.db` (voir data-model.md). L'opération est écrite **avant**
    d'agir (état `planned`, une entrée par fichier), puis chaque entrée est
    mise à jour au fil des résultats ; une interruption (plantage, coupure)
    laisse donc un journal exact de ce qui a été tenté.
  - **Annulation** (FR-021) : on énumère la corbeille par le dossier shell
    `FOLDERID_RecycleBinFolder` et on retrouve chaque élément par son
    emplacement d'origine (`PKEY_Recycle_DisplacedFrom` + nom), sa taille et
    sa date de suppression postérieure au début de l'opération ; on le
    restaure par le verbe `undelete` du menu contextuel shell (le même que
    « Restaurer » dans l'Explorateur). Un élément introuvable (corbeille
    vidée) est marqué `restore_missing`. La 001 redécouvre les fichiers
    restaurés par sa surveillance en direct ; restaurés sur le même volume,
    ils gardent leur identifiant NTFS.
  **[à valider au spike]** : restauration par `undelete` sans boîte de
  dialogue quand l'emplacement d'origine contient déjà un fichier du même
  nom (on refuse alors la restauration de ce fichier et on le signale).
- **Rationale**: écrire l'intention avant l'action est la seule façon de
  tenir la promesse « tout est journalisé » face à un arrêt brutal.
  L'annulation par la corbeille ne demande aucune copie de sauvegarde
  côté Picazoo.
- **Alternatives considered**: déplacer les fichiers dans un dossier de
  Picazoo puis les supprimer plus tard (rejeté : Picazoo stockerait des
  copies des photos et l'utilisateur ne les retrouverait pas dans la
  corbeille) ; manipuler directement `$Recycle.Bin` (rejeté : format interne
  non documenté).

## D10 — Moteur, reprise et état persistant

- **Decision**: un module `duplicates/` dans `picazoo_core`, avec un
  `DuplicateEngine` qui possède **un seul fil** (en mode d'arrière-plan,
  D5) et une connexion d'écriture SQLite dédiée à ses tables.
  - **État en base** : l'avancement est entièrement déduit des tables
    (`item_fingerprint.version` différente de `item.version` ou absente =
    à refaire), comme la 001 avec `thumb_state` ; reprendre après une
    fermeture ne demande rien de plus (FR-030, SC-007). Le point de reprise
    de la recherche complète (D3) est stocké dans `settings`
    (`duplicates_scan_cursor`).
  - **Déclenchement** (FR-033) : au démarrage et à chaque `scanFinished`
    ou `itemUpdated` de la 001, le moteur met à jour sa liste de travail ;
    il démarre seul quand les conditions de D5-4 sont réunies, sauf si
    `duplicates_enabled = 0`.
  - **Écritures** : par lots (une transaction toutes les 500 ms au plus),
    comme le pipeline des vignettes.
  - **Notifications** vers l'interface regroupées toutes les 250 ms
    (`groupsChanged`, `progressChanged`), comme la façade de la 001.
- **Rationale**: un seul fil suffit (le disque est régulé, la partie
  visuelle est légère) et ne prend pas de cœurs au pool des vignettes ; un
  état déduit des tables ne peut pas se désynchroniser.
- **Alternatives considered**: tâches dans le `QThreadPool` de la 001
  (rejeté : partage la priorité et le parallélisme des vignettes) ; état de
  reprise dans un fichier à part (rejeté : un état de plus à garder
  cohérent).

## D11 — Interface

- **Decision**: Qt Quick, comme la 001.
  - Entrée « Doublons » en tête de `FolderList.qml` (compteur de groupes et
    place récupérable), qui remplace la grille par `Duplicates.qml`.
  - Liste des groupes virtualisée (`ListView` sur `DuplicateGroupModel`),
    chaque groupe sur une ligne avec ses copies côte à côte (vignettes de la
    001, informations, étoile de la copie gardée, cases « à la corbeille »).
  - `CompareView.qml` : deux copies en grand avec zoom et déplacement
    synchronisés, sur le `ViewerImageProvider` de la 001.
  - `TrashSummary.qml` : récapitulatif avant confirmation ; `OperationLog.qml` :
    journal et annulation (Outils › Journal des opérations).
  - Réglage « Rechercher les doublons automatiquement » dans Outils.
  Détail dans [contracts/ui-contract.md](contracts/ui-contract.md).
- **Rationale**: réutilise les fournisseurs d'images et le thème de la 001 ;
  reste dans le modèle « une fenêtre, dossiers à gauche » (principe V).
- **Alternatives considered**: fenêtre séparée pour les doublons (rejeté :
  s'écarte du modèle Picasa).

## D12 — Tests et mesures

- **Decision**: Qt Test + CTest, comme la 001-R13.
  - **Unitaires** : pHash et rotations, distance, score de détail, SSIM,
    règle de la copie à garder, regroupement en étoile (aucune chaîne),
    seau à jetons et adaptation du débit (horloge simulée).
  - **Exactitude de la détection** : un générateur (`tests/support/`) produit
    à partir des images libres de `tests/fixtures/` des copies réduites
    (50 %, 25 %), recompressées (qualité 60), converties (PNG, WebP),
    tournées (90°, 180°), éclaircies (+10 %), contrastées (+10 %) ; et des
    fausses paires : rafales simulées (sujet déplacé de quelques pixels,
    même date EXIF à la seconde), photos distinctes de même scène,
    images unies. Mesure de SC-001 et SC-002 ; échec du test si un seuil
    n'est pas tenu.
  - **Intégration sur dossiers temporaires** (principe I) : mise à la
    corbeille réelle dans un dossier temporaire sous le profil, vérification
    que les fichiers sont dans la corbeille, annulation, vérification qu'ils
    sont revenus identiques ; fichier modifié entre analyse et action ;
    groupe dont toutes les copies seraient écartées ; **invariant SC-008** :
    instantané avant/après, seuls les fichiers confirmés ont disparu.
    Les tests nettoient la corbeille de leurs propres éléments seulement.
  - **Performances** : `picazoo-bench --duplicates` sur 20 000 photos
    (SC-003) et mesure du ralentissement d'une copie de fichier concurrente
    (SC-005).
- **Rationale**: le principe I impose des tests automatisés des opérations
  sur les fichiers, sur des dossiers de test ; les seuils de ressemblance
  sont des chiffres à mesurer, pas à supposer.
- **Alternatives considered**: simuler la corbeille par une interface
  factice seulement (gardé pour les tests unitaires du service, mais
  insuffisant : c'est le comportement réel du système qu'il faut garantir).

# Research: Gestion des doublons en lot

Décisions prises pour le plan, à partir de la [spec](spec.md), de la
[proposition de conception](design.md) et du code de la 002.

## R1 — Correspondance d'un chemin avec une règle de dossier

- **Decision**: une règle s'applique à un dossier et à tous ses
  sous-dossiers. Un fichier relève de la règle dont le chemin est le plus
  long préfixe de son dossier, en comparant sans tenir compte de la casse
  et **à une frontière de segment** (`D:\Sauvegarde` couvre
  `D:\Sauvegarde\2019` mais pas `D:\Sauvegarde 2`). Les règles (quelques
  dizaines au plus) sont chargées en mémoire et triées du chemin le plus
  long au plus court ; la première qui correspond gagne. Les règles sont
  rangées par **chemin**, pas par `folder.id`.
- **Rationale**: un renommage de dossier change son identifiant dans la
  001 ; une règle doit pouvoir viser un dossier pas encore indexé ; le
  « plus précis gagne » est la règle attendue (spec, cas limites).
- **Alternatives considered**: `folder.id` (perdu au renommage) ; motifs
  génériques (`*\Sauvegarde\*`) (puissant mais contraire au principe V).

## R2 — Copie suggérée : qualité, règles et protection de la qualité

- **Decision**: `Suggestion::choose(level, copies, rules)` :
  1. chaque copie reçoit la règle de son dossier (R1) ;
  2. candidates = copies « préférées » s'il y en a, sinon copies non
     « écartées » ; si toutes sont écartées → candidates = toutes, drapeau
     `all_avoided` (FR-010) ;
  3. la meilleure candidate selon `KeeperRule::compare` (002) est retenue ;
  4. **protection de la qualité** (clarification FR-006) : si le groupe
     n'est pas « Identique » et qu'une copie hors candidates est RAW alors
     que la retenue ne l'est pas, ou a plus de pixels qu'elle, la
     suggestion de la 002 (la meilleure copie de tout le groupe) est
     conservée et le groupe reçoit le drapeau `preferred_worse` ;
  5. sans aucune règle concernée, la suggestion est celle de la 002.
  La raison affichée devient « Dossier préféré : X » / « Dossier à
  écarter : X » quand une règle a décidé.
- **Rationale**: une seule fonction pure, testable sans base, sert aux
  règles durables **et** à l'action ponctuelle « Garder les copies du
  dossier… » (qui l'appelle avec une règle « préférer » temporaire) : les
  deux se comportent pareil face à un RAW (maquette 2).
- **Alternatives considered**: appliquer la règle sans protection
  (rejeté en clarification) ; règles seulement pour « Identique » (rejeté).

## R3 — Où vit la suggestion

- **Decision**: colonne `dup_member.suggested_keep` (0/1) et colonnes
  `dup_group.suggestion_reason`, `suggestion_detail` (chemin de la règle),
  `flags`. Une fonction `applySuggestions(Database&, groupes)` recalcule ces
  colonnes depuis la base (faits des copies + règles), puis la place
  récupérable, et retire les choix d'un groupe qui n'aurait plus aucune
  copie gardée (FR-017). Elle est appelée par `regroup()` (même transaction)
  et par le `DuplicateStore` quand une règle change. Lecture :
  `keep = COALESCE(dup_choice.keep, dup_member.suggested_keep)`.
- **Rationale**: tous les comptes (barre, totaux de sélection, place,
  filtres par date de la copie gardée) restent en SQL, sans calcul C++ par
  groupe ; le centre reste stable, donc les identifiants de groupe aussi.
- **Alternatives considered**: calcul à la lecture (impose du C++ pour
  chaque total et chaque filtre) ; déplacer le centre (casse FR-007 de la
  002 et les identifiants de groupe).

## R4 — Actions en lot et annulation

- **Decision**: `DuplicateStore::applyBatch(kind, groupes, dossier)` dans
  une transaction : crée une ligne `dup_batch`, applique l'action, écrit
  pour chaque copie touchée `prev_keep` / `new_keep` dans `dup_batch_entry`
  et marque les paires écrites (`dup_decision.batch_id`), puis **une seule**
  notification. Types : `reset`, `keep_folder`, `keep_all`,
  `not_duplicate`. « Ce ne sont pas des doublons » supprime aussi tout de
  suite les lignes du groupe (il disparaît sans attendre le prochain
  regroupement). `undoBatch(id)` rétablit `prev_keep` seulement là où le
  choix vaut encore `new_keep`, supprime les paires encore marquées du lot,
  remet « à revoir » les groupes « Garder toutes » qui n'ont plus de paire
  `keep_all`, et renvoie le nombre de groupes non rétablis (modifiés depuis).
  Les décisions unitaires de la 002 sont inchangées (pas de lot).
- **Rationale**: FR-022 et FR-025 ; la 002 appelait `changed()` à chaque
  décision, ce qui déclencherait des milliers de regroupements en lot.
- **Alternatives considered**: pile d'annulation en mémoire (perdue au
  redémarrage, contraire à FR-022) ; instantané complet des tables (lourd).

## R5 — Requêtes filtrées, sélection et totaux

- **Decision**: `GroupQuery` (niveau, dossier, paire, type, état, taille
  minimale, plage de dates, tri) traduit en une requête SQL paramétrée sur
  `dup_group` avec des `EXISTS` sur les membres. Dossier : `f.path = :p OR
  (f.path > :p || '\' AND f.path < :p || ']')` (`]` suit `\` ; profite de
  l'index unique sur `folder.path`, insensible à la casse). Date : celle de
  tri (`item.sort_date`) de la copie gardée. La sélection est un
  `QSet<GroupId>` dans le modèle, intersecté à chaque rechargement (FR-003) ;
  ses totaux sont une requête agrégée par tranches de 500 identifiants. Le
  compte « fichiers à la corbeille » de la barre devient une requête
  agrégée (corrige le N+1 de la 002).
- **Rationale**: SC-002 avec des requêtes simples ; pas de représentation
  « tout sauf » nécessaire à 10 000 groupes.
- **Alternatives considered**: filtres en C++ sur les groupes chargés
  (charge tout pour chaque filtre).

## R6 — Paires de dossiers et doublons internes

- **Decision**: une requête lit (groupe, dossier, taille, gardée) pour tous
  les membres des groupes « à revoir » ; en mémoire, pour chaque groupe, on
  forme les paires de dossiers distincts et on additionne nombre de groupes
  et place (pour une paire : le plus petit des deux volumes du groupe dans A
  et dans B, ce qu'on libère en gardant un côté). Les paires dont les deux
  chemins se terminent par la même suite de segments (`Images\2019\Été` /
  `D:\Sauvegarde\2019\Été`) sont **remontées** à leurs racines divergentes
  (`Images` / `D:\Sauvegarde`) quand au moins deux sous-paires remontent
  ainsi : la liste reste courte pour une sauvegarde qui reproduit
  l'arborescence. Un dossier présent deux fois dans un groupe compte comme
  « doublon interne ». Calcul à la demande (ouverture du panneau, puis après
  chaque changement des groupes, différé de 500 ms), sur le fil de
  l'interface.
- **Rationale**: ∑ k² sur des groupes de 2 à 5 copies : quelques
  millisecondes pour 10 000 groupes ; un fil dédié n'apporte rien.
- **Alternatives considered**: table matérialisée des paires (à tenir à jour
  à chaque regroupement) ; seuil de 90 % de la conception (remplacé par la
  règle « au moins deux sous-paires de même suffixe », plus simple à tester).

## R7 — Récapitulatif asynchrone et structuré

- **Decision**: `TrashController::prepare(groupes)` lance `TrashService::plan`
  sur un fil de travail (il vérifie l'existence des fichiers et la corbeille
  des volumes) et émet `planReady`. Le plan porte pour chaque entrée le
  niveau du groupe et `examined` (au moins un choix fait à la main). Le
  contrôleur résume : par niveau (« Très ressemblante » non examinées à
  part, non incluses par défaut, FR-018), par dossier d'origine (au plus
  200 fichiers détaillés par dossier, puis « … N autres »). Cocher ou
  décocher un niveau refiltre le plan gardé en mémoire ; « Confirmer »
  exécute le plan filtré. `TrashService::plan` choisit comme copie gardée
  de référence la copie suggérée si elle est gardée, sinon le centre gardé,
  sinon la première gardée.
- **Rationale**: FR-016 à FR-018, SC-002 ; le plan est déjà un objet
  autonome, il suffit de le filtrer.
- **Alternatives considered**: recalculer le plan à chaque case (accès
  disque répétés).

## R8 — Arrêter une opération

- **Decision**: `TrashService::requestStop()` lève un drapeau atomique lu par
  `runTrash` **entre deux fichiers**. Les entrées restantes restent
  `pending` ; l'opération reçoit `stopped = 1` et l'état `partial` (au moins
  un fichier traité) ou `failed` (aucun). Le journal affiche « Arrêtée »
  pour `stopped = 1` et « Non traité : opération arrêtée » pour une entrée
  `pending` d'une opération terminée. L'annulation ne restaure que les
  entrées `trashed`, comme dans la 002.
- **Rationale**: ajouter `stopped` et `skipped_stopped` aux contraintes CHECK
  exigerait de reconstruire `file_operation` ; avec les clés étrangères
  actives, supprimer l'ancienne table effacerait en cascade les entrées du
  journal. Une colonne ajoutée ne fait courir aucun risque.
- **Alternatives considered**: reconstruction des tables du journal
  (rejetée pour ce risque) ; arrêt au milieu d'un fichier (impossible avec
  `IFileOperation`, et contraire au cas limite « jamais à moitié »).

## R9 — Migration 2 → 3

- **Decision**: `003_doublons_lot.sql` : `dup_folder_rule`, `dup_batch`,
  `dup_batch_entry` ; `ALTER TABLE … ADD COLUMN` pour `dup_member`,
  `dup_group`, `dup_decision`, `file_operation` ; initialisation de
  `suggested_keep` depuis le centre ; `schema_version = 3`. Le numéro suit
  l'ordre de livraison (la 003 « images liées » prendra la version suivante).
- **Rationale**: migration non destructive comme la 002 ; sans règle, le
  comportement est exactement celui de la 002.

## R10 — Interface

- **Decision**: vue compacte par défaut au-delà de 20 groupes la première
  fois, choix retenu ensuite (`duplicates_view`). Ligne compacte de 64 px
  (`DuplicateGroupLine.qml`), dépliée sur place en carte de la 002. Filtres
  dans un menu « Filtres ▾ » (type, état, taille minimale, dates par années
  « de … à … ») et puces retirables. « Écartés de la revue » est un mode de
  l'écran (5ᵉ segment), avec ses propres cases et « Remettre dans la
  revue ». Le message « Annuler » dure 10 s ; Ctrl + Z l'actionne tant qu'il
  est affiché. Le panneau « Dossiers » s'ouvre à droite (touche D).
- **Rationale**: suit les maquettes validées ; la plage de dates par années
  suffit pour le tri de photos et évite un sélecteur de dates complet.
- **Alternatives considered**: sélecteur de dates au jour près (plus de code
  QML, peu de valeur ici).

## R11 — Ce qui ne change pas

- La détection, le regroupement, la règle de qualité, la vérification juste
  avant d'agir et la corbeille de la 002.
- `TrashService::planAllIdentical()` reste disponible pour les tests, mais
  n'est plus proposé dans l'interface (FR-015).

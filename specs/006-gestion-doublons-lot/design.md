# Proposition de conception — 006 Gestion des doublons en lot

*Proposition du 2026-11-23, à relire avec la [spec](spec.md) avant
`/speckit-plan`. Ce document sert d'entrée au plan : il fixe l'interface et
les grandes lignes techniques ; le plan, la recherche, le modèle de données et
les contrats en découleront.*

**Maquettes interactives** (Claude Design, privées) :
<https://claude.ai/artifact/NVSjpbxrqGoPVnEjwbuQVN> — 1 · revue en vue
compacte et sélection, 2 · panneau « Dossiers », 3 · récapitulatif d'un gros
lot, 4 · exécution et arrêt, 5 · « Écartés de la revue ». Les schémas en
texte ci-dessous en sont le résumé ; en cas d'écart, les maquettes font foi
pour l'interface.

Les trois points ouverts ont été tranchés le 2026-11-23 (voir la section
Clarifications de la spec) : qualité protégée face aux règles de dossier,
filtres par taille et par date, pas de garde-fou selon le volume.

---

## 1. Ce qui change à l'écran

### 1.1 Écran « Doublons », vue compacte (nouvelle vue par défaut au-delà de 20 groupes)

```text
┌──────────────────────────────────────────────────────────────────────────────────────────┐
│ Doublons                                     [Tous|Identiques|Même image|Très ressembl.] │
│ 1 284 groupes · 3,4 Go       Dossiers ▸   Filtres ▾   Tri : Place ▾    [▤ Compacte|▦ Détaillée] │
│ ┌ Filtres : [Dossier : D:\Sauvegarde ×] [RAW + JPEG ×]   312 groupes affichés sur 1 284 ┐ │
├──────────────────────────────────────────────────────────────────────────────────────────┤
│ ☐ │ IDENTIQUE │ ★▣ ▣ ▣  │ 3 copies · 24 Mo │ ★ IMG_0042.jpg — Images\2019 │ → 2 dans D:\Sauvegarde\2019 │
│ ☑ │ IDENTIQUE │ ★▣ ▣    │ 2 copies · 11 Mo │ ★ IMG_0107.jpg — Images\2019 │ → 1 dans D:\Sauvegarde\2019 │
│ ☑ │ MÊME IMAGE│ ★▣ ▣    │ 2 copies ·  6 Mo │ ★ IMG_0108.CR2 — Images\2019 │ → 1 dans Exports            │
│ ☐ │ TRÈS RESS.│ ★▣ ▣    │ 2 copies ·  3 Mo │ ★ plage.jpg — Images\2020    │ → 1 dans Images\2020  ⚠     │
│   ⋮                                                                                      │
├──────────────────────────────────────────────────────────────────────────────────────────┤
│ 37 groupes sélectionnés · 52 fichiers · 1,2 Go                                     [×]   │
│ [Revenir à la suggestion] [Garder les copies du dossier… ▾] [Garder toutes]              │
│ [Ce ne sont pas des doublons]                              [🗑 Mettre à la corbeille…]  │
└──────────────────────────────────────────────────────────────────────────────────────────┘
```

- **Ligne compacte** : 64 px de haut → 10 à 12 groupes par écran 1080p
  (SC-003). Vignettes 48 px, au plus 5 (« +3 » au-delà). La colonne « → »
  dit d'où viennent les fichiers qui partent : c'est elle qui permet de
  repérer une sauvegarde d'un coup d'œil. ⚠ = groupe signalé (FR-006, FR-010).
- **Clic sur la ligne** (hors case) : la ligne se déplie sur place en carte
  détaillée de la 002 (une seule dépliée à la fois ; Entrée déplie aussi).
- **Case** : sélection ; Maj + clic = plage ; Ctrl + clic = bascule sans
  déplacer le groupe courant.
- **Vue détaillée** : la carte de la 002, avec une case de sélection ajoutée
  dans son en-tête.
- Le choix de vue est retenu dans les réglages.

### 1.2 Barre de sélection

- Remplace la barre d'action de la 002 **dès que la sélection n'est pas
  vide** ; sinon la barre de la 002 reste, mais son bouton devient « Mettre à
  la corbeille… (ce groupe) » et porte sur le **groupe courant seulement**
  (FR-015). Le bouton « Appliquer les suggestions à tous les groupes
  identiques » disparaît : on fait « Identiques » + Ctrl + A.
- « Garder les copies du dossier… » ouvre un menu des dossiers présents dans
  la sélection, triés par nombre de groupes, avec en bas « Créer une règle
  pour ce dossier » (case à cocher, décochée).
- Après une action sur les décisions, message en bas à gauche pendant 10 s :
  « 37 groupes marqués « Garder toutes ». **Annuler** ».

### 1.3 Panneau « Dossiers » (US2)

Ouvert par « Dossiers ▸ », panneau latéral droit de 380 px, la liste des
groupes reste visible à gauche.

```text
┌ Dossiers ─────────────────────────────────────── × ┐
│ Paires de dossiers                                  │
│ ┌─────────────────────────────────────────────────┐ │
│ │ Images\2019  ↔  D:\Sauvegarde\2019              │ │
│ │ 412 groupes · 1,8 Go                            │ │
│ │ [Garder celles de Images\2019 ▾]   [Voir]       │ │
│ └─────────────────────────────────────────────────┘ │
│ │ Images\2020  ↔  D:\Sauvegarde\2020  · 388 · 1,5 Go│
│ │ Images       ↔  Exports              ·  64 · 90 Mo│
│ Doublons internes                                   │
│ │ Images\2021  (IMG_x.jpg / IMG_x (1).jpg) · 22     │
│                                                     │
│ Règles                                              │
│ │ ★ Préférer   Images                        [⋯]   │
│ │ ⤓ Écarter    D:\Sauvegarde                  [⋯]   │
│ │ ⚠ Écarter    E:\Vieux disque — introuvable [⋯]   │
│ [+ Ajouter une règle…]                              │
└─────────────────────────────────────────────────────┘
```

- **Voir** = filtre « paire de dossiers » sur la liste (FR-012).
- **Garder celles de A ▾** : bouton partagé ; le menu propose « Garder celles
  de B » et « Toujours préférer A (créer une règle) ». L'action simple
  modifie les choix des groupes de la paire, comme une action en lot
  (annulable) ; elle ne crée pas de règle.
- Sur une **règle** : menu « Changer en Préférer/Écarter », « Rattacher à un
  autre dossier… », « Supprimer ».
- Le panneau ne touche jamais aux fichiers : il oriente les choix, la mise à
  la corbeille passe toujours par la barre et le récapitulatif.

### 1.4 Récapitulatif pour les gros lots (US4)

```text
┌ Mettre 2 140 fichiers à la corbeille ? ─────────────────────────────────────┐
│ 6,3 Go libérés · 1 612 groupes                                              │
│                                                                             │
│ ☑ Identique            1 498 fichiers · 5,9 Go                              │
│ ☑ Même image             606 fichiers · 380 Mo                              │
│ ☐ Très ressemblante       36 fichiers · 22 Mo   — non vérifiés, non inclus │
│                                                                             │
│ Par dossier d'origine                                    [Par groupe]       │
│ ▸ D:\Sauvegarde\2019     412 fichiers · 1,8 Go                              │
│ ▾ D:\Sauvegarde\2020     388 fichiers · 1,5 Go                              │
│     IMG_2001.jpg   4,1 Mo   ★ gardée : Images\2020\IMG_2001.jpg             │
│     IMG_2002.jpg   3,9 Mo   ★ gardée : Images\2020\IMG_2002.jpg             │
│     ⋮                                                                        │
│ ▸ Exports                 64 fichiers · 90 Mo                               │
│                                                                             │
│ Ne seront pas touchés (3) ▸        Groupes refusés (1) ▸                    │
│ Les fichiers iront dans la corbeille de Windows. Annulable depuis le journal.│
│                                   [Annuler]   [Mettre 2 140 fichiers à la corbeille] │
└─────────────────────────────────────────────────────────────────────────────┘
```

- La case « Très ressemblante » ne s'affiche décochée que pour les groupes
  dont l'utilisateur n'a touché à aucun choix (FR-018) ; les groupes de ce
  niveau qu'il a modifiés comptent dans la ligne cochée.
- Décocher un niveau met à jour les totaux et le libellé du bouton.
- Pas de case « J'ai vérifié » supplémentaire (FR-020, tranché) ;
  « Annuler » reste le bouton par défaut et le bouton de confirmation
  affiche le nombre de fichiers.
- Pendant la préparation (gros lot) : la fenêtre s'ouvre tout de suite avec
  « Préparation du récapitulatif… » et le bouton de confirmation désactivé.

### 1.5 Exécution et arrêt

- La barre du bas montre « Mise à la corbeille : 812 / 2 140 · [Arrêter] » ;
  la liste et la bibliothèque restent utilisables ; les actions qui touchent
  aux fichiers sont désactivées pendant l'opération (une à la fois, 002).
- « Arrêter » : arrêt entre deux fichiers, message « Arrêtée : 812 fichiers
  mis à la corbeille. Voir le journal ». Le journal montre l'état
  « Arrêtée ».

### 1.6 « Écartés de la revue » (US5)

Dernier segment du filtre de niveau, séparé : « … | Très ressemblantes ‖
Écartés ». Deux sections :

- **Actions en lot** : « 23 nov., 14 h 02 — Garder toutes — 37 groupes
  [Annuler cette action] ».
- **Groupes** : lignes compactes grisées avec la décision (« Garder toutes »,
  « Pas des doublons », « Copie retirée ») et sa date ; sélection et
  « Remettre dans la revue » en lot.

### 1.7 Bibliothèque (complément)

- Menu d'un dossier : « Voir les doublons de ce dossier » (FR-014), visible
  seulement si le dossier ou un sous-dossier a des doublons.

### 1.8 Clavier (écran « Doublons »)

| Touche | Action |
|---|---|
| Espace | sur la ligne courante en vue compacte : sélectionner / désélectionner le groupe ; en carte dépliée : comportement de la 002 (copie) |
| Maj + ↑ ↓ | étendre la sélection |
| Ctrl + A | tout sélectionner (groupes affichés) |
| Échap | vider la sélection ; sinon replier la carte ; sinon retour à la bibliothèque |
| Entrée | déplier / replier le groupe courant |
| Suppr | récapitulatif de la sélection (ou du groupe courant) — n'efface rien |
| Ctrl + Z | annuler la dernière action en lot sur les décisions (tant que le message est affiché) |
| D | ouvrir / fermer le panneau « Dossiers » |

Les touches de la 002 (K, C, ← →) continuent d'agir sur la carte dépliée.

---

## 2. Conception technique

### 2.1 Vue d'ensemble

```text
QML  Duplicates.qml ── DuplicateGroupLine.qml (compacte) / DuplicateGroupRow.qml (détaillée)
      │                SelectionBar.qml · FolderPanel.qml · FilterChips.qml · TrashSummary.qml (refait)
      ▼
App  DuplicatesController ── DuplicateGroupModel (+ sélection, filtres, tri)
      │                     FolderPairModel (paires, internes, règles)
      │                     TrashController (+ préparation asynchrone, arrêt)
      ▼
Core DuplicateStore (+ filtres SQL, actions en lot transactionnelles, règles, lots)
     SuggestionRule (KeeperRule de la 002 + règles de dossier)
     FolderPairIndex (calcul des paires, hors fil de l'interface)
     TrashService (+ plan asynchrone, arrêt coopératif)
```

Rien de nouveau ne lit les fichiers de l'utilisateur : tout se calcule depuis
`library.db`.

### 2.2 Suggestion = qualité (002) + règles de dossier

Aujourd'hui la suggestion n'existe qu'implicitement : sans ligne dans
`dup_choice`, le **centre** du groupe est gardé
(`COALESCE(c.keep, m.item_id = center)`). Le centre a un rôle structurel (tous
les membres lui ressemblent, FR-007 de la 002) et ne doit pas changer avec une
règle de dossier.

Proposition : séparer **centre** et **copie suggérée**.

- Nouvelle colonne `dup_member.suggested_keep INTEGER NOT NULL DEFAULT 0`,
  matérialisée quand un groupe est (re)construit et quand les règles changent.
  Les requêtes de comptage deviennent `COALESCE(c.keep, m.suggested_keep)` et
  restent en SQL pur (pas de calcul C++ par groupe pour la barre et les
  totaux).
- `SuggestionRule::suggest(group, copies, rules)` :
  1. trouve pour chaque copie la règle du dossier le plus précis (préfixe de
     chemin, insensible à la casse, frontière `\`) ;
  2. si au moins une copie est « préférée » → candidates = préférées ; sinon
     candidates = non écartées ; si toutes sont écartées → candidates = toutes
     et drapeau `all_avoided` ;
  3. **(FR-006, tranché)** pour un groupe non « Identique », si la
     meilleure candidate est battue sur RAW ou résolution par une copie
     écartée, on garde la suggestion de la 002 et on lève le drapeau
     `preferred_worse` ;
  4. la meilleure candidate selon `KeeperRule::compare` est suggérée.
- `dup_group` gagne `suggestion_reason TEXT NULL` (`folder_preferred`,
  `folder_avoided` ; NULL = raison de la 002 dans `keeper_reason`) et
  `flags INTEGER NOT NULL DEFAULT 0`. On ajoute des colonnes plutôt que de
  toucher aux contraintes CHECK existantes : migration non destructive, comme
  la 002.
- `TrashService::plan` prend comme copie gardée de référence la copie gardée
  (choix ou suggestion), et non plus « le centre s'il est gardé ». Le
  drapeau `identical` (contenu identique à la copie gardée) reste calculé par
  empreinte.

Recalcul quand une règle change : un seul passage SQL + C++ sur les groupes
qui ont une copie sous le dossier de la règle, dans une transaction, puis une
seule notification.

### 2.3 Règles de dossier

```sql
CREATE TABLE dup_folder_rule (
    id         INTEGER PRIMARY KEY,
    path       TEXT    NOT NULL UNIQUE COLLATE NOCASE,
    kind       TEXT    NOT NULL CHECK (kind IN ('prefer', 'avoid')),
    created_at INTEGER NOT NULL
);
```

- Clé par **chemin** et non par `folder.id` : un dossier renommé perd son id
  dans la 001, et une règle doit pouvoir viser un dossier pas encore indexé
  (« écarter `D:\Sauvegarde` » avant que la sauvegarde n'ait des
  sous-dossiers). État « introuvable » = aucun `folder.path` égal ou en
  dessous ; calculé, non stocké.
- Les règles sont peu nombreuses (dizaines) : chargées en mémoire,
  correspondance par tri des préfixes du plus long au plus court.

### 2.4 Filtres et tri en SQL

`DuplicateStore::groups(LevelFilter)` devient
`groups(const GroupQuery&)` :

```cpp
struct GroupQuery {
    LevelFilter level = LevelFilter::All;
    std::optional<QString> folder;               // copie dans ce dossier ou en dessous
    std::optional<std::pair<QString, QString>> pair;
    enum class Type { Any, Photos, Videos, RawJpeg } type = Type::Any;
    enum class State { Any, Untouched, Modified, Flagged } state = State::Any;
    std::optional<qint64> minBytes;              // au moins une copie de cette taille
    std::optional<std::pair<qint64, qint64>> dates; // date de la copie gardée (epoch ms)
    bool setAside = false;                       // « Écartés de la revue »
    enum class Sort { Reclaimable, Copies, Date, Folder } sort = Sort::Reclaimable;
};
```

- Filtre dossier : `EXISTS (SELECT 1 FROM dup_member m JOIN item i … JOIN
  folder f … WHERE m.group_id = g.id AND (f.path = :p OR f.path >= :p || '\'
  AND f.path < :p || ']'))` — comparaison de plage qui profite de l'index
  unique sur `folder.path` (`]` est le caractère qui suit `\`).
- RAW + JPEG : jointure sur le format de l'item (champ de la 001).
- Taille minimale : `EXISTS` sur `item.size_bytes >= :min` parmi les
  membres. Dates : sur la date de tri de la copie gardée (`item.sort_date`
  de la 001 : prise de vue, sinon date du fichier) ; la copie gardée se lit
  par `COALESCE(c.keep, m.suggested_keep) = 1`.
- Dans l'interface, « Filtres ▾ » ouvre un menu : type, état, « Taille
  minimale » (10 Mo, 100 Mo, 1 Go, autre…), « Dates » (cette année, année
  précédente, plage…) ; chaque filtre actif devient une puce retirable.
- La liste ne charge que des identifiants (comme aujourd'hui) ; le détail
  d'une ligne est construit à la demande et mis en cache.

### 2.5 Sélection

- Tenue dans `DuplicateGroupModel` : `QSet<GroupId>` + rôle `selected`. Même
  10 000 groupes, c'est quelques centaines de Ko : pas besoin d'une
  représentation « tout sauf… ».
- `selectAll()` copie la liste d'identifiants déjà chargée pour le filtre ;
  `setQuery()` intersecte la sélection avec la nouvelle liste (FR-003).
- Totaux de la barre : une requête sur une table temporaire de la connexion
  de lecture (`temp.dup_sel(group_id)` remplie en une instruction préparée),
  rejointe à `dup_member`/`dup_choice` : < 50 ms pour 10 000 groupes.
- Corrige au passage `DuplicatesController::refresh()`, qui relit aujourd'hui
  chaque groupe un par un pour compter les fichiers à la corbeille (N+1
  requêtes) : une seule requête d'agrégat.

### 2.6 Actions en lot transactionnelles et annulables

```sql
CREATE TABLE dup_batch (
    id         INTEGER PRIMARY KEY,
    kind       TEXT    NOT NULL CHECK (kind IN ('reset', 'keep_folder', 'keep_all',
                                                'not_duplicate', 'pair_keep', 'restore')),
    label      TEXT    NOT NULL,          -- « Garder toutes — 37 groupes »
    created_at INTEGER NOT NULL,
    undone_at  INTEGER
);

CREATE TABLE dup_batch_entry (           -- état d'avant, pour l'annulation
    batch_id  INTEGER NOT NULL REFERENCES dup_batch(id) ON DELETE CASCADE,
    item_id   INTEGER NOT NULL,
    prev_keep INTEGER,                    -- NULL = pas de choix avant
    new_keep  INTEGER,                    -- pour vérifier que rien n'a changé depuis
    PRIMARY KEY (batch_id, item_id)
);

ALTER TABLE dup_decision ADD COLUMN batch_id INTEGER;   -- paires écrites par un lot
```

- `DuplicateStore::applyBatch(BatchAction, const QList<GroupId>&)` : une
  transaction pour tout le lot (FR-025), **une seule** notification au
  moteur à la fin. Aujourd'hui chaque décision appelle `changed()` et
  déclenche une reconstruction des groupes : en lot, ce serait des milliers
  de reconstructions.
- `undoBatch(id)` : rétablit `prev_keep` pour les items dont le choix vaut
  encore `new_keep`, supprime les `dup_decision` du lot ; renvoie les groupes
  non rétablis (FR-022).
- Les actions unitaires de la 002 restent telles quelles (elles ne créent pas
  de lot).

### 2.7 Paires de dossiers

- `FolderPairIndex` calcule, sur un fil de travail et à partir de la base :
  pour chaque groupe « à revoir », l'ensemble de ses dossiers ; pour chaque
  paire (a < b), nombre de groupes et place récupérable ; pour chaque
  dossier présent deux fois dans un groupe, un « doublon interne ».
  Complexité ∑ k² sur la taille des groupes : négligeable (k ≈ 2–5).
- Recalcul différé (500 ms) après chaque notification du moteur ; le
  panneau garde l'ancienne liste en attendant. Rien n'est stocké.
- Les paires sont aussi proposées au niveau des **dossiers parents communs**
  (`Images\2019 ↔ Sauvegarde\2019` se résume en `Images ↔ Sauvegarde` quand
  toutes les sous-paires suivent la même structure) : la liste reste courte
  sur une sauvegarde qui reproduit l'arborescence. À valider au plan, avec
  une règle simple (on ne remonte que si ≥ 90 % des groupes sous les deux
  parents forment des paires de sous-dossiers de même nom relatif).

### 2.8 Récapitulatif asynchrone et structuré

- `TrashService::plan()` part sur un fil de travail
  (`TrashController::prepareAsync(groupIds)` → signal `planReady`) : la
  vérification des volumes et de la présence des fichiers fait des appels
  disque qu'il ne faut pas faire sur le fil de l'interface quand il y a des
  milliers de fichiers.
- Le plan porte pour chaque groupe son niveau et un drapeau `examined`
  (au moins un `dup_choice` posé à la main) → section « Très ressemblante,
  non vérifiés » (FR-018).
- Le QML ne reçoit plus une `QVariantList` à plat : un `TrashPlanModel`
  (arbre à deux niveaux dossier → fichiers, rempli à la demande quand un
  dossier se déplie) et un résumé par niveau. Décocher un niveau recalcule
  le plan confirmable sans rouvrir la fenêtre.

### 2.9 Arrêt d'une opération

- `TrashService::requestStop()` met un `std::atomic<bool>` que `runTrash`
  lit entre deux fichiers ; le fichier en cours finit son appel à la
  corbeille. Les entrées restantes passent à `skipped_stopped`… or les
  contraintes CHECK de `file_operation_entry.result` et de
  `file_operation.state` ne connaissent pas ces valeurs.
- Proposition : migration qui **reconstruit** ces deux tables (création de
  `*_new`, copie, renommage, dans une transaction) pour ajouter `stopped`
  (opération) et `skipped_stopped` (entrée). Ce sont des tables du journal,
  petites ; la reconstruction est testée sur une base de la 002 remplie. Si
  on préfère ne rien reconstruire : `partial` + colonne `stopped_by_user`,
  au prix d'un état moins lisible. À trancher au plan.

### 2.10 Migration

Un fichier de migration (schéma version 2 → 3) : `dup_folder_rule`,
`dup_batch`, `dup_batch_entry`, colonnes ajoutées (`dup_member.suggested_keep`,
`dup_group.suggestion_reason`, `dup_group.flags`, `dup_decision.batch_id`),
reconstruction du journal (2.9), puis initialisation de `suggested_keep` à
partir du centre (comportement de la 002 à l'identique tant qu'aucune règle
n'existe). Le numéro de fichier suit l'ordre de livraison et non le numéro de
la fonctionnalité : si la 003 passe avant, sa migration prend la version 3.

### 2.11 Tests

| Niveau | Ce qui est vérifié |
|---|---|
| Unitaire | correspondance des règles (préfixe, casse, frontière `\`, dossier le plus précis) ; `SuggestionRule` (préférée, écartée, toutes écartées, préférée moins bonne) ; annulation d'un lot partiellement modifié depuis |
| Intégration | « Tout sélectionner » respecte les filtres ; la corbeille ne porte que sur la sélection (SC-004) ; « Très ressemblante » non vérifiés exclus par défaut ; un lot de 1 000 groupes = une transaction et une reconstruction ; arrêt d'une opération (SC-006) ; migration 2 → 3 sur une base de la 002 remplie |
| Corbeille réelle | lot sur dossiers de test, arrêt à mi-parcours, annulation : contenu des dossiers identique à l'avant (constitution, principe I) |
| Performance | banc 10 000 groupes : sélection, totaux, ouverture du récapitulatif, défilement de la vue compacte (SC-002, SC-003) — hors compilation, machine au repos |

### 2.12 Ordre de livraison proposé

1. **Lot A — US1** : sélection, barre, actions en lot transactionnelles,
   portée corrigée de « Mettre à la corbeille… », correction du N+1.
   Livrable seul : c'est déjà la réponse au manque signalé.
2. **Lot B — US2** : suggestion séparée du centre, règles, paires, panneau.
3. **Lot C — US4** : récapitulatif asynchrone et structuré, arrêt.
4. **Lot D — US3** : vue compacte, filtres combinés, entrée depuis la
   bibliothèque.
5. **Lot E — US5** : « Écartés de la revue », annulation des lots passés.

La vue compacte (D) pourrait passer avant B si l'on veut d'abord le confort
de lecture ; B apporte plus de gain de temps.

### 2.13 Risques

- **Suggestion ≠ centre** : touche au calcul de la place récupérable et au
  plan de la corbeille de la 002. À couvrir par les tests d'intégration
  existants de la 002, qui doivent passer sans changement tant qu'aucune
  règle n'existe.
- **Règle trop large** (« écarter `C:\` ») : sans danger pour les fichiers
  (tout passe par le récapitulatif), mais suggestions absurdes. Le panneau
  affiche pour chaque règle le nombre de groupes qu'elle oriente.
- **Variabilité des délégués QML** (lignes compactes et carte dépliée de
  hauteurs différentes) : surveiller la fluidité du `ListView` ; sinon
  ouvrir la carte dépliée dans un volet plutôt que sur place.

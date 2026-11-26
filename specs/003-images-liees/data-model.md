# Data Model: Images liées

La 003 ajoute des tables à `library.db` par une migration **non
destructive** `005_images_liees.sql` (schéma version 4 → 5). Elle ne
modifie aucune table de la 001 ni de la 002, et n'écrit rien dans
`thumbs.db`. Rien n'est écrit dans les dossiers de l'utilisateur.

Conventions de la 001/002 : dates en epoch ms, `item.id` stable pour les
renommages et déplacements, écriture par une connexion unique par module.

## `naming_group` — Groupe d'images liées

| Champ | Type | Règle |
|---|---|---|
| `id` | INTEGER PK | |
| `root_key` | TEXT NOT NULL UNIQUE COLLATE NOCASE | racine normalisée détectée (N1), clé de reconstruction automatique |
| `display_name` | TEXT NOT NULL | nom suggéré, éditable (FR-007) ; initialisé depuis `root_key` |
| `confidence` | TEXT NOT NULL CHECK (`regular_series` \| `same_root` \| `generic_root`) | N4 |
| `member_count` | INTEGER NOT NULL CHECK (>= 2) | |
| `state` | TEXT NOT NULL DEFAULT 'to_review' CHECK (`to_review` \| `confirmed` \| `rejected`) | |
| `renamed` | INTEGER NOT NULL DEFAULT 0 CHECK (0, 1) | 1 si `display_name` diffère de la racine détectée (survit à une reconstruction) |
| `updated_at` | INTEGER NOT NULL | |

Index : `confidence`, `state`.

Un groupe tombé à moins de 2 membres (tous retirés ou disparus) est
supprimé, sauf s'il porte une `root_key` marquée `rejected` dans
`naming_decision` (voir plus bas) : dans ce cas la ligne `naming_group`
elle-même est aussi supprimée, la décision suffisant à empêcher sa
reconstruction.

## `naming_member` — Élément dans un groupe

| Champ | Type | Règle |
|---|---|---|
| `group_id` | INTEGER NOT NULL FK → `naming_group.id` ON DELETE CASCADE | |
| `item_id` | INTEGER NOT NULL UNIQUE FK → `item.id` ON DELETE CASCADE | un élément appartient à au plus un groupe de nommage |
| `detected_root` | TEXT NOT NULL | racine détectée pour cet élément au moment de son ajout (peut différer de `naming_group.root_key` après une fusion) |
| `manual` | INTEGER NOT NULL DEFAULT 0 CHECK (0, 1) | 1 si ajouté explicitement par l'utilisateur (FR-008) : survit à une racine qui ne correspondrait plus |

Clé primaire : `(group_id, item_id)`.

## `naming_decision` — Décisions de l'utilisateur sur une racine

| Champ | Type | Règle |
|---|---|---|
| `root_key` | TEXT PRIMARY KEY COLLATE NOCASE | racine normalisée concernée |
| `kind` | TEXT NOT NULL CHECK (`rejected` \| `merged_into`) | `rejected` : « Ce n'est pas un groupe » (FR-008) ; `merged_into` : cette racine a été fusionnée dans une autre |
| `target_root_key` | TEXT NULL | pour `merged_into` : la racine du groupe qui a absorbé celle-ci |
| `decided_at` | INTEGER NOT NULL | |

Règles :
- `rejected` : aucun groupe n'est (re)formé pour cette racine, même après un
  nouveau repérage complet ou l'ajout d'un nouvel élément qui y
  correspondrait (FR-016, US3 edge case) ; annulable depuis l'écran de
  revue (US2), ce qui supprime la ligne.
- `merged_into` : utilisée par la fusion de groupes (US2) pour qu'un
  nouveau repérage ne recrée pas deux groupes séparés aux deux racines
  d'origine ; les deux racines pointent alors vers le même
  `naming_group.root_key` cible.
- Une scission (US2) ne passe pas par cette table : elle déplace des
  `naming_member` vers un nouveau `naming_group` à `root_key` synthétique
  (racine d'origine + suffixe impossible dans un nom de fichier). Les éléments
  déplacés sont marqués `manual = 1` : un nouveau repérage ne les réunit pas
  (FR-009).

`label` (TEXT) garde le nom du groupe au moment de la décision, pour la
liste des décisions annulables.

## `naming_exclusion` — Retraits décidés par l'utilisateur

| Champ | Type | Règle |
|---|---|---|
| `item_id` | INTEGER NOT NULL FK → `item.id` ON DELETE CASCADE | élément retiré |
| `root_key` | TEXT NOT NULL COLLATE NOCASE | racine du groupe dont il a été retiré (pour un groupe scindé puis exclu : racine du groupe d'origine) ; il reste proposé ailleurs |
| `rejected_root` | TEXT NULL | racine du groupe rejeté qui a causé ce retrait ; levé quand ce rejet est annulé |

Clé primaire : `(item_id, root_key)`. Sans cette table, un nouveau repérage
remettrait un élément retiré à la main (FR-009).

Un ajout, une scission et une fusion posent `naming_member.manual = 1` sur les
éléments déplacés. Un groupe `confirmed` (toute correction le confirme) garde
tous ses membres, même renommés sur le disque (US4, SC-007).

## `settings` — Nouvelle clé

| Clé | Défaut | Rôle |
|---|---|---|
| `naming_include_generic_roots` | `0` | inclure les racines génériques dans la détection (N2) |

Comme les clés existantes (`Settings::defaultValue`, pas de ligne insérée
par la migration), le défaut est fourni par le code tant qu'aucune ligne
n'existe en base.

## Entités en mémoire (non stockées)

- **NamingCandidate** : résultat intermédiaire du détecteur pur
  (`NamingDetector`) : `{root_key, items: [{item_id, detected_root}],
  confidence}`, avant d'être comparé à l'état déjà en base (groupes
  existants, décisions) par `NamingEngine`.
- **NamingProgress** : `{active: bool}` — pas de notion de pourcentage
  utile (le repérage est quasi instantané), contrairement à
  `DuplicateProgress` (002).

# Data Model: Gestion des doublons en lot

La 006 fait passer `library.db` du schéma 2 au schéma 3 par une migration
**non destructive** `003_doublons_lot.sql` : deux tables pour les lots, une
pour les règles, et des colonnes ajoutées. Aucune table de la 001 ni de la
002 n'est supprimée ou reconstruite. Rien n'est écrit dans les dossiers de
l'utilisateur. Conventions : dates en epoch ms, chemins comme dans
`folder.path` (insensibles à la casse).

## `dup_folder_rule` — Règle de dossier (nouvelle)

| Champ | Type | Règle |
|---|---|---|
| `id` | INTEGER PK | |
| `path` | TEXT UNIQUE COLLATE NOCASE | dossier visé ; s'applique aussi à ses sous-dossiers (research R1) |
| `kind` | TEXT | `prefer` \| `avoid` |
| `created_at` | INTEGER | |

État « introuvable » : calculé (aucun `folder.path` égal ou en dessous),
jamais stocké. Une règle introuvable est ignorée par la suggestion.

## `dup_member` — colonne ajoutée

| Champ | Type | Règle |
|---|---|---|
| `suggested_keep` | INTEGER NOT NULL DEFAULT 0 | 1 pour la copie suggérée à garder (exactement une par groupe) ; initialisé depuis `center_item_id` par la migration |

Copie gardée effective : `COALESCE(dup_choice.keep, dup_member.suggested_keep)`.
`reclaimable_bytes` (002) est recalculée avec cette expression.

## `dup_group` — colonnes ajoutées

| Champ | Type | Règle |
|---|---|---|
| `suggestion_reason` | TEXT NULL | `folder_preferred` \| `folder_avoided` ; NULL = raison de la 002 (`keeper_reason`) |
| `suggestion_detail` | TEXT NULL | chemin de la règle qui a décidé |
| `flags` | INTEGER NOT NULL DEFAULT 0 | bit 0 `all_avoided` (toutes les copies dans des dossiers à écarter) ; bit 1 `preferred_worse` (la copie du dossier préféré est moins bonne) |

## `dup_decision` — colonne ajoutée

| Champ | Type | Règle |
|---|---|---|
| `batch_id` | INTEGER NULL | lot qui a écrit la paire ; NULL pour une décision unitaire. Une décision ultérieure sur la même paire (INSERT OR REPLACE) la détache du lot. |

## `dup_batch` — Action en lot (nouvelle)

| Champ | Type | Règle |
|---|---|---|
| `id` | INTEGER PK | |
| `kind` | TEXT | `reset` \| `keep_folder` \| `keep_all` \| `not_duplicate` |
| `label` | TEXT | texte affiché (« Garder toutes — 37 groupes ») |
| `detail` | TEXT NULL | dossier pour `keep_folder` |
| `group_count` | INTEGER | groupes réellement modifiés |
| `created_at` | INTEGER | |
| `undone_at` | INTEGER NULL | non NULL une fois annulée |

## `dup_batch_entry` — État d'avant d'une copie (nouvelle)

| Champ | Type | Règle |
|---|---|---|
| `batch_id` | INTEGER FK → `dup_batch.id` ON DELETE CASCADE | |
| `item_id` | INTEGER | sans clé étrangère (l'élément peut avoir été mis à la corbeille depuis) |
| `prev_keep` | INTEGER NULL | `dup_choice.keep` avant le lot ; NULL = pas de choix |
| `new_keep` | INTEGER NULL | valeur écrite par le lot ; NULL = choix supprimé |

Clé primaire `(batch_id, item_id)`.

Annulation (`undoBatch`) :

```text
pour chaque entrée : si choix actuel == new_keep  → choix := prev_keep
                     sinon                        → groupe signalé « modifié depuis »
paires dup_decision où batch_id = lot             → supprimées
groupes kept_all sans paire keep_all restante     → to_review
dup_batch.undone_at                               := maintenant
```

## `file_operation` — colonne ajoutée

| Champ | Type | Règle |
|---|---|---|
| `stopped` | INTEGER NOT NULL DEFAULT 0 | 1 si l'utilisateur a arrêté l'opération ; ses entrées non traitées restent `pending` (research R8) |

Libellés : `stopped = 1` → « Arrêtée » ; entrée `pending` d'une opération
terminée → « Non traité : opération arrêtée ».

## Entités calculées (non stockées)

- **Sélection** : `QSet<GroupId>` du modèle de l'écran, limitée aux groupes
  affichés.
- **Paire de dossiers** : `(a, b, groupes, octets, sous-paires)` ; voir
  research R6.
- **Doublon interne** : `(dossier, groupes, octets)`.
- **Écarté de la revue** : groupe `kept_all`, ou ensemble d'éléments reliés
  par des paires `not_duplicate` (composantes connexes), avec la date la
  plus récente de ses décisions.

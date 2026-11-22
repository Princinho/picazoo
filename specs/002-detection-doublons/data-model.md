# Data Model: Détection des doublons

La 002 ajoute des tables à `library.db` par une migration **non
destructive** `002_doublons.sql` (schéma version 1 → 2). Elle ne modifie
aucune table de la 001 et n'écrit rien dans `thumbs.db`, qu'elle lit
seulement (empreintes visuelles, voir [research.md D2](research.md)).
Rien n'est écrit dans les dossiers de l'utilisateur.

Conventions de la 001 : dates en epoch ms, `item.id` stable pour les
renommages et déplacements, écriture par une connexion unique par module.

## `item_fingerprint` — Empreintes d'un élément

| Champ | Type | Règle |
|---|---|---|
| `item_id` | INTEGER PK, FK → `item.id` ON DELETE CASCADE | |
| `version` | INTEGER | = `item.version` au moment du calcul ; une valeur différente rend la ligne périmée (tout est à refaire) |
| `content_state` | TEXT | `not_candidate` \| `pending` \| `done` \| `skipped_online` \| `failed` |
| `content_hash` | BLOB(16) NULL | XXH3-128 du fichier entier ; non NULL ssi `content_state = done` |
| `visual_state` | TEXT | `pending` \| `done` \| `low_detail` \| `not_applicable` \| `failed` |
| `phash_0` | INTEGER NULL | pHash 64 bits de la vignette (rotation 0°) |
| `phash_90` | INTEGER NULL | rotation 90° |
| `phash_180` | INTEGER NULL | rotation 180° |
| `phash_270` | INTEGER NULL | rotation 270° |
| `detail_score` | REAL NULL | énergie AC ; sous le seuil → `visual_state = low_detail` |
| `updated_at` | INTEGER | |

Index : `content_hash` (partiel, `WHERE content_hash IS NOT NULL`),
`content_state`, `visual_state`.

Cycle de vie (`content_state`) :

```text
nouvel élément ou version+1 ──► not_candidate  (taille ou quick_hash unique)
                                   │ un autre élément partage (size_bytes, quick_hash)
                                   ▼
                                pending ──(lecture régulée)──► done
                                   ├──(fichier en ligne seulement)──► skipped_online
                                   └──(illisible)──────────────────► failed
```

Cycle de vie (`visual_state`) :

```text
vidéo ─────────────────────────────────────────────► not_applicable
photo ──► pending ──(vignette finale disponible)──► done | low_detail | failed
```

Règles :
- Une ligne dont `version ≠ item.version` est traitée comme absente.
- `not_candidate` est réévalué quand un élément de même `(size_bytes,
  quick_hash)` apparaît.
- Deux éléments de même `(item.volume_serial, item.file_id)` non NULL sont
  le même fichier (liens physiques) : ils ne sont jamais regroupés.

## `dup_group` — Groupe de doublons

| Champ | Type | Règle |
|---|---|---|
| `id` | INTEGER PK | |
| `center_item_id` | INTEGER FK → `item.id` | copie suggérée à garder (centre de l'étoile, research D7) |
| `keeper_reason` | TEXT | `raw` \| `resolution` \| `less_compressed` \| `has_exif` \| `oldest` \| `path` |
| `level` | TEXT | `identical` \| `same_image` \| `similar` : le plus faible des membres |
| `member_count` | INTEGER | ≥ 2 |
| `reclaimable_bytes` | INTEGER | somme des tailles des membres non gardés (choix actuels) |
| `state` | TEXT | `to_review` \| `kept_all` \| `resolved` |
| `updated_at` | INTEGER | |

Index : `(state, reclaimable_bytes DESC)`, `level`.

Transitions (`state`) :

```text
to_review ──(« Garder toutes »)──► kept_all ──(nouvelle copie entre)──► to_review
to_review ──(mise à la corbeille : il ne reste qu'une copie)──► resolved (groupe supprimé)
to_review ──(« Ce ne sont pas des doublons » sur tout le groupe)──► groupe supprimé
```

Un groupe qui tombe à moins de 2 membres (copie disparue, retirée ou mise à
la corbeille) est supprimé.

## `dup_member` — Copie dans un groupe

| Champ | Type | Règle |
|---|---|---|
| `group_id` | INTEGER FK → `dup_group.id` ON DELETE CASCADE | |
| `item_id` | INTEGER FK → `item.id` ON DELETE CASCADE, UNIQUE | un élément est dans un groupe au plus |
| `level` | TEXT | niveau par rapport au centre ; `identical` pour le centre lui-même |
| `similarity` | REAL NULL | SSIM avec le centre (NULL si `identical`) |
| `rotation` | INTEGER | 0, 90, 180, 270 : rotation qui rapproche ce membre du centre |

Clé primaire : `(group_id, item_id)`.

## `dup_choice` — Copie gardée choisie par l'utilisateur

| Champ | Type | Règle |
|---|---|---|
| `item_id` | INTEGER PK, FK → `item.id` ON DELETE CASCADE | |
| `keep` | INTEGER | 1 = à garder, 0 = à la corbeille |
| `decided_at` | INTEGER | |

Sans ligne, la suggestion s'applique (centre gardé, autres à la corbeille).
Un choix survit à la reconstruction des groupes.

## `dup_decision` — Décisions sur des paires

| Champ | Type | Règle |
|---|---|---|
| `item_a` | INTEGER FK → `item.id` ON DELETE CASCADE | `item_a < item_b` |
| `item_b` | INTEGER FK → `item.id` ON DELETE CASCADE | |
| `kind` | TEXT | `not_duplicate` \| `keep_all` |
| `decided_at` | INTEGER | |

Clé primaire : `(item_a, item_b)`.

Règles :
- `not_duplicate` : la paire n'est plus jamais vérifiée ni regroupée
  (FR-013, US5). « Ce ne sont pas des doublons » sur un groupe écrit toutes
  les paires du groupe ; « retirer une copie » écrit les paires entre cette
  copie et les autres membres.
- `keep_all` : la paire reste regroupée mais le groupe est `kept_all` tant
  qu'il ne contient que des paires `keep_all`.
- Annuler une décision supprime les lignes correspondantes.

## `file_operation` — Opération sur les fichiers (journal)

| Champ | Type | Règle |
|---|---|---|
| `id` | INTEGER PK | |
| `kind` | TEXT | `trash` \| `restore` |
| `undoes_id` | INTEGER NULL FK → `file_operation.id` | pour `restore` : opération annulée |
| `created_at` | INTEGER | écrit **avant** d'agir |
| `finished_at` | INTEGER NULL | |
| `state` | TEXT | `planned` \| `running` \| `done` \| `partial` \| `failed` \| `undone` \| `partially_undone` |
| `file_count` | INTEGER | |
| `bytes` | INTEGER | place libérée (ou restaurée) |

## `file_operation_entry` — Un fichier dans une opération

| Champ | Type | Règle |
|---|---|---|
| `id` | INTEGER PK | |
| `operation_id` | INTEGER FK → `file_operation.id` ON DELETE CASCADE | |
| `item_id` | INTEGER NULL | **sans** clé étrangère : l'élément est retiré de l'index après la mise à la corbeille |
| `original_path` | TEXT | chemin complet au moment de l'action |
| `size_bytes` | INTEGER | |
| `mtime` | INTEGER | |
| `content_hash` | BLOB(16) NULL | si connu |
| `keeper_path` | TEXT NULL | copie gardée du même groupe (pour le récapitulatif et le journal) |
| `result` | TEXT | `pending` \| `trashed` \| `skipped_changed` \| `skipped_no_recycle_bin` \| `skipped_locked` \| `skipped_unavailable` \| `failed` \| `restored` \| `restore_missing` \| `restore_conflict` |
| `message` | TEXT NULL | détail lisible (français) |

Cycle de vie (`result`) :

```text
pending ──► trashed ──(annulation)──► restored
   │                 ├──(absent de la corbeille)──► restore_missing
   │                 └──(un fichier occupe déjà l'emplacement)──► restore_conflict
   ├──► skipped_changed          (modifié, déplacé ou disparu depuis l'analyse — FR-019)
   ├──► skipped_no_recycle_bin   (volume sans corbeille ou effacement définitif refusé — FR-018)
   ├──► skipped_locked           (verrouillé, lecture seule)
   ├──► skipped_unavailable      (racine indisponible)
   └──► failed
```

Règle : au redémarrage, une opération restée `planned` ou `running` est
close en `partial` ; ses entrées `pending` sont vérifiées sur le disque
(fichier présent à son emplacement → `failed` avec message « interrompue »,
présent dans la corbeille → `trashed`).

## `settings` — Nouvelles clés

| Clé | Défaut | Rôle |
|---|---|---|
| `duplicates_enabled` | `1` | analyse automatique (FR-033) |
| `duplicates_paused` | `0` | pause demandée par l'utilisateur (conservée) |
| `duplicates_scan_cursor` | vide | point de reprise de la recherche complète (research D3) |
| `duplicates_level_filter` | `all` | filtre de l'écran de revue |

## Entités en mémoire (non stockées)

- **FingerprintTable** : tableau contigu `{item_id, phash[4], aspect,
  taken_at}` de toutes les photos `visual_state = done`, rechargé au
  démarrage, mis à jour au fil de l'eau (research D3).
- **LumaCache** : LRU de vignettes 64×64 en luminance pour la vérification
  (research D4).
- **DuplicateProgress** : `{phase: waiting|content|visual|grouping|idle,
  done, total, paused, reason}` où `reason` explique une attente
  (`library_busy`, `on_battery`, `disabled`, `user_paused`).
- **TrashPlan** : ce que le récapitulatif affiche avant confirmation :
  fichiers à écarter par groupe, copie gardée, fichiers non supprimables et
  pourquoi, place libérée.

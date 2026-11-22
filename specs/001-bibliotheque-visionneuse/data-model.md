# Data Model: Bibliothèque et visionneuse

Stockage : `%LOCALAPPDATA%\Picazoo\library.db` (index) et
`%LOCALAPPDATA%\Picazoo\thumbs.db` (vignettes), SQLite en mode WAL (voir
[research.md R10](research.md)). Rien n'est écrit dans les dossiers de
l'utilisateur.

Le schéma est pensé pour accueillir 002 (doublons), 003 (groupes) et 004
(réorganisation) **sans migration destructive** : ces fonctionnalités
ajouteront des tables qui référencent `item.id`.

## library.db

### `schema_version`

| Champ | Type | Règle |
|---|---|---|
| `version` | INTEGER | une seule ligne ; 1 pour la 001 |

### `watched_root` — Dossier surveillé

| Champ | Type | Règle |
|---|---|---|
| `id` | INTEGER PK | |
| `path` | TEXT UNIQUE | chemin absolu normalisé (séparateurs `\`, sans `\` final, casse conservée, comparaison insensible à la casse) |
| `volume_serial` | INTEGER NULL | numéro de série du volume au dernier accès réussi |
| `state` | TEXT | `available` \| `unavailable` \| `denied` |
| `last_scan_at` | INTEGER NULL | epoch ms de la dernière analyse complète |
| `added_at` | INTEGER | epoch ms |

Règles :
- Ajouter un dossier déjà contenu dans un autre dossier surveillé est
  accepté, mais il n'est pas parcouru deux fois (R3) : la racine englobante
  le couvre.
- Retirer un dossier surveillé supprime de l'index ses `folder` et ses
  `item` qu'aucune autre racine ne couvre, avec leurs vignettes. **Aucun
  fichier du disque n'est touché.**

### `excluded_path` — Sous-dossier exclu

| Champ | Type | Règle |
|---|---|---|
| `id` | INTEGER PK | |
| `root_id` | INTEGER FK → `watched_root.id` ON DELETE CASCADE | |
| `path` | TEXT | doit être à l'intérieur de la racine `root_id` |

### `folder` — Dossier contenant des éléments

| Champ | Type | Règle |
|---|---|---|
| `id` | INTEGER PK | |
| `root_id` | INTEGER FK → `watched_root.id` | racine qui le couvre |
| `path` | TEXT UNIQUE | chemin absolu normalisé |
| `parent_path` | TEXT | pour le mode arborescence |
| `name` | TEXT | dernier segment du chemin |
| `item_count` | INTEGER | maintenu par l'indexeur |
| `date` | INTEGER NULL | epoch ms : date de l'élément le plus ancien (sert au tri et au regroupement par année) |

Règles :
- Un `folder` n'existe que s'il contient au moins un `item` ; il est retiré
  quand son dernier élément disparaît.
- Le mode arborescence affiche les dossiers intermédiaires sans élément
  (nécessaires pour la hiérarchie) ; ils sont **calculés** à partir de
  `parent_path`, pas stockés.

### `item` — Élément (photo ou vidéo)

| Champ | Type | Règle |
|---|---|---|
| `id` | INTEGER PK | **stable** : conservé lors des renommages et déplacements (FR-008) ; c'est la clé utilisée par 002 à 004 |
| `folder_id` | INTEGER FK → `folder.id` | |
| `file_name` | TEXT | nom avec extension |
| `kind` | TEXT | `image` \| `raw` \| `video` |
| `format` | TEXT | `jpeg`, `png`, `gif`, `bmp`, `tiff`, `webp`, `heic`, `cr2`, `cr3`, `nef`, `arw`, `orf`, `rw2`, `raf`, `dng`, `mp4`, `mov`, `avi`, `mkv` |
| `size_bytes` | INTEGER | |
| `mtime` | INTEGER | date de modification du fichier, epoch ms |
| `volume_serial` | INTEGER NULL | identité NTFS (R4) |
| `file_id` | BLOB NULL | identifiant de fichier NTFS (64 ou 128 bits) ; NULL sur FAT/exFAT |
| `quick_hash` | INTEGER NULL | XXH3-64 de (taille + 64 Ko début + 64 Ko fin) |
| `taken_at` | INTEGER NULL | date de prise de vue EXIF, epoch ms (heure locale de l'appareil, sans fuseau) |
| `sort_date` | INTEGER | `taken_at`, sinon `mtime` (indexé) |
| `width` | INTEGER NULL | dimensions **après** application de l'orientation |
| `height` | INTEGER NULL | |
| `orientation` | INTEGER | valeur EXIF 1 à 8 ; 1 par défaut |
| `camera` | TEXT NULL | « Marque Modèle » |
| `duration_ms` | INTEGER NULL | vidéos uniquement |
| `meta_state` | TEXT | `pending` \| `done` \| `failed` |
| `thumb_state` | TEXT | `pending` \| `provisional` \| `done` \| `unreadable` \| `no_preview` |
| `version` | INTEGER | incrémentée quand le contenu change (taille ou date) ; invalide la vignette |
| `last_seen_at` | INTEGER | epoch ms, dernier parcours où le fichier a été vu |

Index : `(folder_id, sort_date)`, `(folder_id, file_name)`,
`(volume_serial, file_id)`, `(quick_hash, size_bytes)`, `sort_date`.

Cycle de vie (`thumb_state`) :

```text
découverte ─► pending ──(vignette intégrée trouvée)──► provisional ──► done
                 │                                        │
                 └──────────────(génération)─────────────►┤
                                                          ├─► unreadable   (fichier corrompu)
                                                          └─► no_preview   (RAW sans aperçu intégré)
contenu modifié (taille/date) : version+1, thumb_state → pending
```

Règles de rapprochement au rattrapage (R4) :
1. Même chemin, même `size_bytes` et même `mtime` → inchangé.
2. Même chemin, taille ou date différente → `version + 1`, métadonnées et
   vignette à refaire.
3. Nouveau chemin : on cherche un élément **disparu** du même parcours avec
   le même `(volume_serial, file_id)`, sinon le même `(quick_hash,
   size_bytes)` → on met à jour `folder_id` et `file_name` en conservant
   `id` (renommage/déplacement).
4. Chemin disparu sans correspondance à la fin du parcours → élément
   supprimé de l'index, avec sa vignette.
5. Les éléments d'une racine `unavailable` ne sont **jamais** supprimés.

### `settings` — Préférences d'affichage

| Clé | Valeur par défaut |
|---|---|
| `thumb_size` | `160` (px, de 64 à 320) |
| `folder_sort` | `date_desc` (`date_desc`, `date_asc`, `name_asc`) |
| `item_sort` | `taken_asc` (`taken_asc`, `name_asc`) |
| `folder_view` | `by_year` (`by_year`, `tree`) |
| `last_folder_id` | vide |
| `onboarding_done` | `0` |

Table : `key TEXT PK, value TEXT`.

## thumbs.db

### `thumb`

| Champ | Type | Règle |
|---|---|---|
| `item_id` | INTEGER | = `item.id` (pas de clé étrangère inter-fichiers ; nettoyage par l'indexeur) |
| `version` | INTEGER | = `item.version` au moment de la génération |
| `kind` | TEXT | `provisional` (vignette intégrée) \| `final` (320 px) |
| `width` | INTEGER | |
| `height` | INTEGER | |
| `jpeg` | BLOB | JPEG qualité 85, orientation appliquée |

Clé primaire : `(item_id, kind)`. Une vignette dont la `version` ne
correspond plus à celle de `item` est ignorée et régénérée.

`thumbs.db` peut être supprimé à tout moment sans perte : il est entièrement
reconstruit à partir des fichiers.

## Entités en mémoire (non stockées)

- **GridRow** : ligne de la grille, `{type: header|items, folder_id,
  first_index, count, y, height}`, recalculée quand la largeur, la taille des
  vignettes, le tri ou le contenu changent.
- **ScanProgress** : `{phase: discovering|indexing|thumbnails|idle, done,
  total, paused}` affiché par l'indicateur discret (US1-2).
- **ViewerState** : élément courant, niveau de zoom, position,
  préchargements ±2.

# Data Model: Ergonomie des visionneuses et clic droit

**Feature**: [spec.md](spec.md) · **Research**: [research.md](research.md)

Une seule modification persistante (le journal sait d'où vient une
opération) ; le reste est de l'état en mémoire.

## Persistant : migration `004_origine_operations.sql` (schéma 3 → 4)

```sql
ALTER TABLE file_operation ADD COLUMN source TEXT NOT NULL DEFAULT 'duplicates'
    CHECK (source IN ('duplicates', 'library', 'viewer'));
```

- Non destructive : les opérations existantes (toutes issues des doublons)
  prennent `duplicates`.
- `Migrator` : `kLibrarySchemaVersion = 4`. La visionneuse autonome applique
  la migration elle-même si elle ouvre la base la première (research T2).
- Une restauration (`kind = 'restore'`) reprend le `source` de l'opération
  qu'elle annule.
- `file_operation_entry` est inchangée : une entrée « bibliothèque » ou
  « visionneuse » a `keeper_path = NULL` ; une entrée « visionneuse » d'un
  fichier hors des dossiers surveillés a `item_id = NULL` (déjà permis).

Règles :

| Règle | Source |
|---|---|
| Une mise à la corbeille écrit l'opération et ses entrées (`pending`) avant de toucher au premier fichier. | Constitution I, 002 |
| Aucune entrée ne passe à `trashed` sans `RecycleResult::Recycled`. | 002 D8 |
| `recoverInterrupted` ne reprend que les opérations `planned` / `running` créées il y a plus de 60 s. | research T2 |

## En mémoire : `ZoomMotion` (core/browse)

Un par image affichée (visionneuse intégrée : 1 ; autonome : 1 par
emplacement, 4).

| Champ | Type | Rôle |
|---|---|---|
| `imageSize`, `viewSize`, `margin` | tailles | entrées de `ZoomMath` (ajustement, bornes) |
| `scale` | réel | échelle affichée (1 = 100 %) |
| `offset` | point | coin haut gauche de l'image dans la vue |
| `target` | réel | échelle visée ; toujours dans `[fit, min(plafond, kMaxScale)]` |
| `anchor`, `start`, `end` | points / réels | ancrage du mouvement en cours (`zoomStepOffset`) |
| `fitMode` | booléen | à l'ajustement (suit les changements de taille de vue) |
| `moving` | booléen | la poursuite n'a pas encore rejoint la cible |
| `immediate` | booléen | animations Windows désactivées : τ = 0, sans accélération |
| `gesture` | `Gesture` ou rien | geste en cours (ci-dessous) |
| `cadence` | réel | crans par seconde, moyenne glissante (demi-vie 100 ms) |

### `Gesture`

| Champ | Valeurs |
|---|---|
| `source` | `Wheel`, `Key`, `Button`, `Pinch` |
| `direction` | +1 avant, −1 arrière |
| `startScale` | échelle **affichée** au début du geste |
| `ceiling` | `1.0` si `direction = +1` et `startScale < 0.9`, sinon aucun |
| `lastInputMs` | instant de la dernière entrée |

### Transitions

```text
          entrée (cran, touche, bouton, pincement)
 ┌──────┐ ─────────────────────────────────────────▶ ┌────────────────┐
 │ Repos│                                            │ Geste en cours  │
 │      │ ◀───────── advance() : cible rejointe ──── │ (cible bornée)  │
 └──────┘            et geste expiré                 └────────────────┘
     ▲                                                   │   │
     │  stop() : clic, glisser, changement de photo,     │   │ cran en sens inverse,
     └──────────── cran en sens inverse ─────────────────┘   │ autre source, > 300 ms
                                                             ▼ (molette), appui non répété
                                                       nouveau geste (plafond réévalué)
```

- Un geste de molette expire 300 ms après son dernier cran ; un geste
  clavier à la fin de la répétition automatique (prochain appui non
  répété) ; un geste bouton à chaque clic ; un pincement à sa fin.
- `setTarget(absolu)` (« Ajuster », « 100 % », touches 0 / 1, double-clic)
  termine le geste et vise la valeur demandée, sans plafond ni accélération.
- `reset()` (changement de photo) : ajustement immédiat, sans geste.

## En mémoire : `WallpaperSnapshot` (core/platform/win)

| Champ | Rôle |
|---|---|
| `monitors` : liste de `{ id, path }` | fond de chaque écran avant le changement |
| `position` | disposition Windows (remplir, ajuster…) ; inchangée par Picazoo |
| `slideshow` | vrai si un diaporama Windows était actif |
| `restorable` | faux si un ancien chemin n'existe plus ou si `slideshow` |

Vit le temps du message « Annuler » ; jamais écrit sur le disque.

## En mémoire : menus contextuels

Aucune donnée nouvelle : les menus lisent la sélection (`ItemSelection`),
l'élément courant (`ViewerController.current`, `ViewerSession.currentPath`)
et un cache par volume « a une corbeille » (`QHash<racine du volume, bool>`,
rempli hors du fil de l'interface, vidé au changement de disque).

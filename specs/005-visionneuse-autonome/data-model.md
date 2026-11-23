# Modèle de données — Visionneuse autonome (005)

Tout vit en mémoire, sauf les réglages. Rien n'est écrit dans la
bibliothèque de la 001.

## BrowseEntry (élément du parcours)

| Champ | Type | Origine |
|---|---|---|
| `path` | chemin absolu | listage (V6) |
| `name` | nom du fichier | listage |
| `format` | `Format` de la 001 | extension |
| `size` | octets | listage |
| `modified` | date de modification | listage |

Règles : seuls les formats d'images pris en charge (`kindOf(format) ==
Kind::Image`) ; ni fichiers cachés ni fichiers système ; ni sous-dossiers.

## FolderBrowser (parcours)

| Champ | Type | Rôle |
|---|---|---|
| `folder` | chemin | dossier parcouru |
| `entries` | liste ordonnée de `BrowseEntry` | ordre de l'Explorateur ou ordre naturel |
| `orderSource` | `Natural` \| `Explorer` | origine de l'ordre |
| `current` | index | photo affichée |
| `missingCurrent` | booléen | la photo affichée a disparu du disque |

Transitions :

- `open(path)` : `entries = [path]`, `current = 0` (affichage immédiat),
  puis listage → `entries` complet en ordre naturel, `current` recalé sur
  `path` ; puis ordre de l'Explorateur reçu → réordonnancement,
  `orderSource = Explorer`, `current` recalé sur le même fichier.
- `next()` / `previous()` : `current ± 1` modulo la taille (boucle,
  FR-008) ; `wrapped` signalé quand on passe de la fin au début.
- `first()` / `last()` ; `goTo(index)`.
- `refresh(listing)` : fichiers restants dans leur ordre ; nouveaux insérés
  (ordre naturel) ou ajoutés à la fin (ordre de l'Explorateur) ; photo
  affichée disparue → `missingCurrent = true`, puis passage à la suivante.
- Dossier vide de photos prises en charge, ou photo ouverte non prise en
  charge : `entries` ne contient que la photo ouverte, marquée non
  affichable.

## ZoomState (photo affichée)

| Champ | Type | Règle |
|---|---|---|
| `imageSize` | taille pleine résolution, orientée | du décodage |
| `viewSize` | taille de l'écran | |
| `scale` | réel | `fitScale ≤ scale ≤ 16` ; `fitScale = min(1, vue/image)` |
| `offset` | point | borné : bords de l'image jamais en deçà des bords de l'écran ; centré si l'image est plus petite |
| `fitMode` | booléen | vrai après ouverture, `Ctrl 0` ou retour au minimum |

Changer de photo remet `fitMode = true`.

## ViewerSettings (`viewer.ini`)

| Clé | Valeurs | Défaut |
|---|---|---|
| `overlay/opaque` | booléen | faux |
| `strip/visible` | booléen | vrai |
| `assoc/answer` | `none` \| `later` \| `never` \| `accepted` | `none` |
| `assoc/askedAt` | date ISO | — |
| `assoc/formats` | liste d'extensions choisies | toutes |

Écrit seulement quand une valeur change ; quelques centaines d'octets.
« Plus tard » repropose au plus tôt 7 jours après. La proposition n'apparaît
pas si Picazoo ouvre déjà les JPEG.

## FileAssociation (lu à la demande, détenu par Windows)

| Champ | Type |
|---|---|
| `extension` | `.jpg`, `.heic`… |
| `registered` | Picazoo figure dans « Ouvrir avec » |
| `isDefault` | Picazoo est l'application effective (`QueryCurrentDefault`) |

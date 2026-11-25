# Contrat de l'interface : menus, clavier, plein écran, zoom

Libellés exacts en français. « (grisé si …) » : l'entrée reste visible,
désactivée, avec une info-bulle qui dit pourquoi (FR-067).

## Menus contextuels (FR-060 à FR-076)

### Vignette — un élément (`ItemContextMenu.qml`)

| Entrée | Raccourci | Remarque |
|---|---|---|
| Ouvrir | Entrée | visionneuse intégrée |
| Ouvrir dans la visionneuse autonome | | grisé pour une vidéo |
| Ouvrir avec l'application par défaut | | |
| — | | |
| Copier | Ctrl+C | photo : image + fichier ; vidéo : fichier |
| Copier le chemin | Ctrl+Maj+C | |
| Afficher dans l'Explorateur | Ctrl+Maj+E | |
| Voir les doublons de cette photo | | grisé si elle n'est dans aucun groupe |
| — | | |
| Définir comme fond d'écran | | grisé : vidéo, RAW, HEIC, WebP… |
| Propriétés | Alt+Entrée | |
| Mettre à la corbeille… | Suppr | grisé : disque sans corbeille |

### Vignettes — sélection de N éléments

Première ligne non cliquable : « N éléments sélectionnés ». Mêmes entrées ;
s'appliquent à tous : Copier (fichiers seulement), Copier le chemin (un par
ligne), Propriétés, Mettre à la corbeille… ; grisées : Ouvrir, Ouvrir dans la
visionneuse autonome, Ouvrir avec…, Afficher dans l'Explorateur, Voir les
doublons, Définir comme fond d'écran.

### Zone vide de la grille

Agrandir les vignettes (Ctrl++) · Réduire les vignettes (Ctrl+−) · Tout
sélectionner (Ctrl+A) · — · Ouvrir le dossier dans l'Explorateur (grisé si
aucun dossier n'est affiché).

### Dossier de la liste

Afficher dans l'Explorateur · Copier le chemin · — · Voir les doublons de ce
dossier (existant). Ligne d'année : pas de menu.

### Image — visionneuse intégrée (`Viewer.qml` → `ImageContextMenu.qml`)

Précédente (←) · Suivante (→) · — · Zoom avant (↑) · Zoom arrière (↓) ·
Ajuster (0) · 100 % (1) · — · Plein écran / Quitter le plein écran (F11) ·
Informations (I) · — · Copier l'image (Ctrl+C) · Copier le chemin
(Ctrl+Maj+C) · Afficher dans l'Explorateur (Ctrl+Maj+E) · Ouvrir avec
l'application par défaut · — · Définir comme fond d'écran · Propriétés
(Alt+Entrée) · Mettre à la corbeille… (Suppr) · — · Retour à la bibliothèque
(Échap).

Vidéo : entrées de zoom et fond d'écran grisées ; « Copier l'image » devient
« Copier ».

### Image — visionneuse autonome (`src/viewer/qml/ImageContextMenu.qml`)

Comme ci-dessus, avec les raccourcis propres à cette visionneuse (Ajuster :
Ctrl+0 ; pas de raccourci pour 100 %), et : pas de « Plein écran » (la visionneuse couvre
l'écran) ; « Ouvrir dans Picazoo » ajouté avant « Ouvrir avec une autre
application… » ; dernière entrée « Fermer (Échap) ».

### Doublons

Menus existants de la copie et du groupe, inchangés (FR-066).

## Ouverture et fermeture des menus

- Clic droit : le menu s'ouvre sous le curseur, dans la fenêtre. Sur une
  vignette non sélectionnée, elle devient la sélection d'abord (déjà le
  cas) ; sur une vignette sélectionnée, la sélection est gardée.
- Touche Menu ou Maj+F10 : menu de l'élément courant, ouvert à côté de la
  vignette courante (bibliothèque) ou au centre de l'image (visionneuses).
- Échap ou clic ailleurs : le menu se ferme, rien d'autre. Échap ne ferme
  pas la visionneuse quand un menu est ouvert.
- Un clic droit ne zoome, ne déplace et n'ouvre jamais rien ; il arrête un
  élan de zoom en cours.

## Mise à la corbeille (FR-071 à FR-073)

Dialogue modal :

> **Mettre « IMG_0042.jpg » à la corbeille ?**
> Le fichier part dans la corbeille de Windows. Vous pourrez l'annuler.
> [Annuler] (par défaut, Entrée) [Mettre à la corbeille]

Pour N fichiers : « Mettre 12 fichiers à la corbeille ? » avec le total en
octets. Après : message en bas « 12 fichiers mis à la corbeille ·
[Annuler] » pendant 8 s (Ctrl+Z tant qu'il est visible) ; l'opération figure
au journal (« depuis la bibliothèque » / « depuis la visionneuse »).

## Fond d'écran (FR-074)

Message : « Fond d'écran changé · [Annuler] » pendant 8 s ; si l'ancien fond
ne peut pas être rétabli : « Fond d'écran changé. L'ancien fond (diaporama)
ne pourra pas être rétabli automatiquement. »

## Clavier dans les visionneuses (FR-017 à FR-019)

| Touche | Visionneuse intégrée | Visionneuse autonome |
|---|---|---|
| ← / → | précédente / suivante (inchangé) | idem (inchangé) |
| ↑ / ↓ | zoom avant / arrière d'un cran, au centre | idem |
| Ctrl + / Ctrl − | idem ↑ / ↓ (inchangé) | idem |
| Suppr | mettre à la corbeille… | idem |
| Ctrl+C / Ctrl+Maj+C | copier l'image / le chemin | idem |
| Alt+Entrée | propriétés | idem |
| Menu, Maj+F10 | menu de l'image | idem |

Sur une vidéo, ↑ / ↓ ne font rien. Dans la grille, les flèches déplacent
toujours la sélection ; Suppr, Ctrl+C, Ctrl+Maj+C, Alt+Entrée y agissent sur
la sélection.

## Bouton « Quitter le plein écran » (FR-001 à FR-005)

- Visionneuse intégrée en plein écran seulement ; en haut à droite, 16 px
  du bord ; pastille ronde de 48 px comme les boutons précédent / suivant,
  icône « réduire » et info-bulle « Quitter le plein écran (F11) ».
- Apparaît au moindre mouvement du pointeur (< 100 ms), s'efface en fondu
  (250 ms) après 2 s d'immobilité, jamais tant qu'il est survolé ou qu'un
  menu est ouvert.
- Clic : fenêtre rendue à son état d'avant (fenêtrée ou agrandie), même
  photo, même zoom, même position.

## Zoom (FR-010 à FR-016, FR-040 à FR-047)

- L'indicateur affiche `round(percent) + " %"` ; à la butée, « 100 % ».
- Ni la molette, ni ↑, ni Ctrl +, ni le bouton zoom avant ne franchissent
  100 % dans un geste commencé sous 90 %.
- Le double-clic, « Ajuster », « 100 % », 0 et 1 gardent leur effet, avec la
  nouvelle animation (≈ 0,3 s) mais sans élan.

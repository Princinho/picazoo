# Contrat d'interface — Visionneuse autonome

## Le voile

- Recouvre l'écran de la souris, barre des tâches comprise ; noir à 78 %
  (mode semi-transparent) ou 100 % (mode opaque, F11). Fondu d'entrée et de
  sortie de 150 ms.
- Photo centrée, ajustée à l'écran avec une marge de 48 px (0 en mode
  opaque), jamais agrandie au-delà de sa taille réelle à l'ouverture.
- En haut à gauche : nom du fichier, puis « 12 sur 87 ». En haut à droite :
  « ⋯ » (menu) et « ✕ » (Fermer).
- En bas, centré : barre de commandes, puis bande de vignettes.
- Commandes, nom et bande s'estompent après 2,5 s sans mouvement de la
  souris (pas pendant qu'un menu ou une carte est ouvert) et reviennent au
  moindre mouvement.
- Bords gauche et droit (15 % de la largeur) : un bouton ◀ / ▶ apparaît au
  survol ; un clic dans la zone passe à la précédente / suivante, sauf si
  l'image est zoomée.

## Barre de commandes

◀ Précédente · ▶ Suivante · − · « 63 % » · + · Ajuster · 100 % ·
Informations · Afficher dans l'Explorateur · Ouvrir dans Picazoo · Voile
opaque.

## Menu « ⋯ »

- Ouvrir vos photos avec Picazoo… (carte d'association, V11)
- Associations actuelles… (tableau format → « Picazoo » / « autre
  application »)
- Inscrire Picazoo dans « Ouvrir avec » (visible seulement si l'inscription
  manque, cas du zip portable)
- Afficher la bande de vignettes (case à cocher)

## Raccourcis

| Touche | Action |
|---|---|
| → / ← | suivante / précédente (boucle) |
| Début / Fin | première / dernière |
| Molette | zoom ancré sur le curseur |
| Ctrl + / Ctrl − / Ctrl 0 | zoom avant / arrière / ajuster |
| Double-clic | ajuster ⇄ 100 % au point cliqué |
| Glisser | déplacer l'image zoomée |
| F11 | voile opaque ⇄ semi-transparent |
| I | informations |
| Ctrl+Maj+E | afficher dans l'Explorateur |
| Échap | fermer (ferme d'abord un menu ou une carte ouverts) |

## Messages

| Cas | Texte |
|---|---|
| Fichier illisible | « Cette image est illisible. » + « Ouvrir avec une autre application » |
| Format non pris en charge | « Picazoo ne sait pas afficher ce type de fichier. » + même bouton |
| Vidéo | « Les vidéos se regardent dans Picazoo. » + « Ouvrir dans Picazoo » · « Ouvrir avec le lecteur par défaut » |
| RAW sans aperçu | « Aperçu indisponible pour ce fichier RAW. » + « Ouvrir avec une autre application » |
| Photo disparue | « Cette photo a été supprimée ou déplacée. » (puis suivante) |
| Retour au début | bandeau bref « Retour à la première photo » (1,2 s) |

## Carte d'association (FR-030)

Titre : « Ouvrir vos photos avec Picazoo ? » — texte : « Quand vous
double-cliquez sur une photo dans l'Explorateur, elle s'ouvrira ici. Vous
confirmerez dans les réglages de Windows. » — cases par famille de formats
(JPEG, PNG, GIF, BMP, TIFF, WebP, HEIC, RAW), toutes cochées — boutons
« Choisir dans Windows… », « Plus tard », « Ne plus demander ». Après
« Choisir dans Windows… », la carte rappelle les extensions à choisir dans
la page qui s'ouvre.

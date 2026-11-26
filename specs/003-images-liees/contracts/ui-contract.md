# Contrat d'interface utilisateur — 003

Ce que l'utilisateur voit et peut faire pour les images liées, en
complément du [contrat de la 001](../../001-bibliotheque-visionneuse/contracts/ui-contract.md)
et sur le modèle déjà connu de l'écran « Doublons » de la
[002](../../002-detection-doublons/contracts/ui-contract.md). Tous les
textes sont en français.

## Écran

| Écran | Quand | Contenu |
|---|---|---|
| **Images liées** | clic sur l'entrée « Images liées » de la liste des dossiers | remplace la grille : barre de filtres, liste des groupes |

Pas d'écran de comparaison ni de récapitulatif : aucune action de la 003 ne
touche à un fichier, donc rien à confirmer avant d'agir.

## Liste des dossiers

- Entrée **« Images liées »**, sous l'entrée « Doublons » (002) si les deux
  existent, visible dès qu'au moins un groupe existe : « Images liées —
  24 groupes ».
- Aucun indicateur de progression dans la barre du haut : le repérage est
  quasi instantané (N3), contrairement à l'analyse des doublons.

## Écran « Images liées »

- **Barre de filtres** : « Tous | Séries régulières | Même racine » (+
  « Racines génériques » si le réglage correspondant est activé).
- **Un groupe** = une ligne :
  - en-tête : nom suggéré du groupe (éditable en place), badge de
    confiance (Série régulière — vert ; Même racine — bleu ; Racine
    généralement utilisée par l'appareil — gris), nombre d'éléments ;
  - vignettes des éléments du groupe, dans l'ordre du nom de fichier ;
  - menu du groupe : « Renommer », « Fusionner avec… » (ouvre un
    sélecteur d'un autre groupe), « Scinder… » (sélection des éléments à
    détacher), « Ce n'est pas un groupe » ;
  - menu d'un élément : « Retirer de ce groupe », « Ouvrir », « Afficher
    dans l'Explorateur ».
  - bouton « Ajouter des éléments… » : ouvre un sélecteur parmi les
    éléments de la bibliothèque non déjà présents dans ce groupe ;
  - bouton « Confirmer » (groupe à revoir) : le groupe est gardé tel quel ;
    toute correction le confirme aussi.
- **« Décisions (n) »** : liste des exclusions et des fusions, chacune avec
  « Annuler la décision » (FR-009). **« Rechercher maintenant »** relance un
  repérage complet.
- Double-clic sur un élément : visionneuse de la 001 ; Échap revient à
  l'écran « Images liées », sur le même groupe.
- Aucun groupe trouvé : message « Aucune image liée trouvée pour
  l'instant. » (US1, scénario 4).

## Réglages (Outils)

- Case à cocher « Inclure les noms donnés par l'appareil (IMG_, DSC_…) dans
  les images liées » (`naming_include_generic_roots`, décoché par défaut).

## Panneau d'informations (visionneuse / bibliothèque)

- Si l'élément affiché appartient à un groupe d'images liées : mention
  discrète « Fait partie du groupe "<nom>" (n éléments) — Voir le groupe »,
  indépendante de la mention équivalente des doublons (002) si les deux
  s'appliquent au même élément.

## Accessibilité

- Mêmes conventions que la 002 : `Accessible.name` sur l'entrée de la
  liste des dossiers (« Images liées — 24 groupes »), sur chaque ligne de
  groupe (nom, confiance, nombre d'éléments) et sur les actions de menu.

# Contrat d'interface utilisateur — 006

Complète le [contrat de la 002](../../002-detection-doublons/contracts/ui-contract.md).
Les maquettes interactives (lien dans [design.md](../design.md)) font foi pour
la disposition ; ce contrat fixe les comportements et les textes.

## Écran « Doublons »

- **En-tête** : titre, « 1 284 groupes · 6,3 Go » ; segments de niveau
  « Tous | Identiques | Même image | Très ressemblantes » avec leur compte,
  séparés de **« Écartés »**.
- **Barre d'outils** : « Dossiers » (ouvre le panneau), « Filtres ▾ »,
  « Tri : … ▾ », bascule « Compacte | Détaillée » (retenue).
- **Puces de filtres** : une par filtre actif (« Dossier : … », « Paire : A ↔
  B », « Type : RAW + JPEG », « État : … », « Taille ≥ 10 Mo », « Dates :
  2019 – 2021 »), chacune avec ×, puis « 312 groupes affichés sur 1 284 ».
- **Menu Filtres** : Type (Tous, Photos, Vidéos, RAW + JPEG) ; État (Tous,
  Suggestion intacte, Choix modifié, Signalés) ; Taille minimale (Aucune,
  1 Mo, 10 Mo, 100 Mo, 1 Go) ; Dates (Toutes, Cette année, L'an dernier,
  Plage d'années…).
- **Tri** : Place récupérable, Nombre de copies, Date, Dossier.
- **En-tête de liste** : case « Tout sélectionner » (groupes affichés) et
  noms de colonnes en vue compacte.

### Ligne compacte (64 px)

case · badge de niveau · jusqu'à 5 vignettes (gardées entourées, la
suggérée avec une étoile, les autres atténuées, « +N ») · « 3 copies » /
« 24 Mo récupérables » · ★ nom et dossier de la copie gardée · « → 2 dans
D:\Sauvegarde\2019 » (ou « 1 dans A · 1 dans B ») · signalement éventuel
(« À vérifier : retouche probable », « La copie du dossier préféré est moins
bonne », « Toutes les copies sont dans des dossiers à écarter ») · chevron.
Clic hors case : déplie la carte détaillée de la 002 sous la ligne.

### Carte détaillée (002, complétée)

Case de sélection dans l'en-tête ; « Suggérée : <raison> » sur la copie
suggérée (raison de dossier comprise) ; le reste inchangé.

### Barre du bas

- **Sans sélection** : « Groupe courant : <nom> · N fichiers · X » ;
  « Tout sélectionner » ; « Mettre à la corbeille… (ce groupe) ».
- **Avec sélection** : « 37 groupes sélectionnés » / « 52 fichiers à la
  corbeille · 1,2 Go libérés » ; × (vider) ; « Revenir à la suggestion »,
  « Garder les copies du dossier… ▾ » (dossiers présents dans la sélection),
  « Garder toutes », « Ce ne sont pas des doublons », « Mettre à la
  corbeille… ».
- **Pendant une opération** : « Mise à la corbeille : 812 / 2 140 » + barre
  de progression + « Arrêter ».

### Message « Annuler »

Après une action en lot, 10 s : « 37 groupes marqués « Garder toutes ».
Annuler ». Après un changement de filtre qui retire des groupes de la
sélection : « 2 groupes retirés de la sélection : masqués par le filtre. »
(sans Annuler).

## Panneau « Dossiers » (droite, 400 px)

- **Paires de dossiers** : « A ↔ B — 412 groupes · 1,8 Go » ; clic = filtre
  paire ; la paire filtrée montre « Garder celles de A » et un menu :
  « Garder celles de B », « Toujours préférer A (règle) », « Toujours écarter
  B (règle) ».
- **Doublons internes** : « Images\2022 — 22 groupes · 61 Mo » ; clic =
  filtre dossier.
- **Règles** : « Préférer / Écarter », chemin, « N groupes orientés » ou
  « Dossier introuvable : règle ignorée » ; menu « Changer en …»,
  « Rattacher à un autre dossier… », « Supprimer » ; « Ajouter une
  règle… » (choix d'un dossier puis du type). Pied : « Les règles changent
  seulement la copie suggérée. Un choix fait à la main reste prioritaire. »

## Récapitulatif

- Titre « Mettre 2 140 fichiers à la corbeille ? », sous-titre « 6,3 Go
  libérés · 1 248 groupes » ; pendant la préparation : « Préparation du
  récapitulatif… » et bouton de confirmation désactivé.
- Lignes de niveau avec case : Identique, Même image, Très ressemblante
  (vérifiées), Très ressemblante non vérifiées (« Non vérifiées : non
  incluses », décochée par défaut).
- « Par dossier d'origine » : dossiers dépliables (fichier, poids, « gardée :
  <chemin> »), au plus 200 fichiers par dossier puis « … N autres fichiers ».
- « Ne seront pas touchés (N) » dépliable ; groupes refusés (002).
- Boutons : « Annuler » (par défaut), « Mettre 2 140 fichiers à la
  corbeille » ; aucune confirmation supplémentaire selon le volume.

## « Écartés de la revue »

- **Actions en lot** : « 23 nov., 14 h 02 — Garder toutes — 37 groupes »,
  « Annuler cette action » ; après annulation : « Annulée : 35 groupes remis
  dans la revue. 2 groupes modifiés depuis : non rétablis. »
- **Groupes écartés** : case, nom, dossiers, décision (« Garder toutes »,
  « Pas des doublons »), date ; « Remettre dans la revue » (sélection).

## Journal des opérations (complément)

État « Arrêtée » ; entrée « Non traité : opération arrêtée » ; bouton
« Arrêter » pendant une opération.

## Bibliothèque (complément)

Clic droit sur un dossier : « Voir les doublons de ce dossier » → écran
« Doublons » filtré sur ce dossier.

## Clavier (écran « Doublons »)

| Touche | Action |
|---|---|
| ↑ ↓ | groupe précédent / suivant |
| Maj + ↑ ↓ | étendre la sélection |
| Espace | vue compacte, ligne repliée : sélectionner / désélectionner ; carte dépliée : comportement de la 002 |
| Ctrl + A | tout sélectionner (groupes affichés) |
| Entrée | déplier / replier |
| Échap | vider la sélection ; sinon replier ; sinon retour à la bibliothèque |
| Suppr | récapitulatif de la sélection ou du groupe courant (n'efface rien) |
| Ctrl + Z | annuler la dernière action en lot (message affiché) |
| D | ouvrir / fermer « Dossiers » |
| K, C, ← → | sur la carte dépliée, comme la 002 |

## Garanties

- Aucune action de ces écrans ne touche un fichier sans le récapitulatif et
  le bouton de confirmation.
- Une action en lot ne porte que sur des groupes affichés et sélectionnés.
- Le fil de l'interface ne lit aucun fichier de l'utilisateur.

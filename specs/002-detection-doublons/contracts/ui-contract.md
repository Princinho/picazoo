# Contrat d'interface utilisateur — 002

Ce que l'utilisateur voit et peut faire pour les doublons, en complément du
[contrat de la 001](../../001-bibliotheque-visionneuse/contracts/ui-contract.md).
Tous les textes sont en français.

## Écrans

| Écran | Quand | Contenu |
|---|---|---|
| **Doublons** | clic sur l'entrée « Doublons » de la liste des dossiers | remplace la grille : barre de filtres, liste des groupes, barre d'action |
| **Comparaison** | « Comparer » sur deux copies d'un groupe | deux copies en grand côte à côte, zoom et déplacement synchronisés |
| **Récapitulatif** | « Mettre à la corbeille… » | fenêtre de confirmation listant tout ce qui va se passer |
| **Journal des opérations** | Outils › Journal des opérations… | opérations passées, détail par fichier, « Annuler » |

## Liste des dossiers

- Entrée **« Doublons »** en tête de liste, au-dessus des années, visible dès
  qu'un groupe existe : « Doublons — 128 groupes · 3,4 Go ».
- Pendant l'analyse, un petit indicateur sur l'entrée ; l'indicateur discret
  de la barre du haut (001) affiche aussi « Doublons : 4 210 / 20 000 ».
- Quand l'analyse attend : « Doublons : en attente de la fin de l'analyse
  de la bibliothèque » / « … en pause sur batterie » / « … en pause ».

## Écran « Doublons »

- **Barre de filtres** : « Tous | Identiques | Même image | Très
  ressemblantes » ; tri « Place récupérable | Nombre de copies | Date ».
- **Un groupe** = une ligne :
  - en-tête : badge de niveau (Identique — vert ; Même image — bleu ; Très
    ressemblante — orange, avec « à vérifier »), nombre de copies, place
    récupérable ;
  - les copies côte à côte, chacune avec : vignette, nom, dossier
    (raccourci, chemin complet au survol), dimensions, poids, format, date ;
  - la copie gardée porte une **étoile** et la mention de la raison
    (« Plus grande résolution », « RAW d'origine »…) ; les autres ont une
    case cochée **« À la corbeille »** ;
  - une copie indisponible (disque débranché) est grisée, sa case est
    désactivée ;
  - menu du groupe : « Comparer », « Revenir à la suggestion », « Garder
    toutes », « Ce ne sont pas des doublons » ; menu d'une copie : « Garder
    celle-ci », « Retirer de ce groupe », « Ouvrir », « Afficher dans
    l'Explorateur ».
- **Barre d'action** (bas) : « 214 fichiers à la corbeille · 2,1 Go » +
  bouton **« Mettre à la corbeille… »** ; bouton « Appliquer les
  suggestions à tous les groupes identiques… » (visible si le filtre montre
  des groupes identiques).
- Double-clic sur une copie : visionneuse de la 001 ; Échap revient à
  l'écran « Doublons », sur le même groupe.

## Récapitulatif (avant toute action)

- Titre : « Mettre 214 fichiers à la corbeille ? »
- Pour chaque groupe : la copie gardée (étoile) et les fichiers qui partent
  (nom, dossier, poids).
- Section « Ne seront pas touchés » si besoin : fichiers non supprimables
  par Picazoo (« Ce disque n'a pas de corbeille »), indisponibles,
  verrouillés — avec « Afficher dans l'Explorateur ».
- Groupes refusés : « Gardez au moins une copie de chaque photo. »
- Pied : « Les fichiers iront dans la corbeille de Windows. Vous pourrez
  les restaurer depuis le journal des opérations. »
- Boutons : **« Mettre à la corbeille »** (non sélectionné par défaut),
  « Annuler » (sélectionné par défaut : Entrée n'efface rien).

## Comparaison

- Deux copies côte à côte, nom et informations sous chacune ; molette ou
  Ctrl + / Ctrl − zoome les deux ; glisser déplace les deux ; 0 : ajuster ;
  1 : taille réelle.
- Boutons « Garder celle-ci » sous chaque copie ; Échap revient à l'écran
  « Doublons ».

## Journal des opérations

- Liste : date, « 214 fichiers mis à la corbeille · 2,1 Go », état
  (« Fait », « Fait en partie », « Annulé »).
- Détail : chaque fichier, emplacement d'origine, résultat en clair.
- **« Annuler cette opération »** : restaure depuis la corbeille ce qui y
  est encore, puis affiche le compte rendu.

## Visionneuse (complément de la 001)

- Panneau d'informations : « 2 doublons — Voir » quand l'élément est dans un
  groupe (FR-015) ; « Voir » ouvre l'écran « Doublons » sur ce groupe.

## Réglages

- Outils › Doublons : case **« Rechercher les doublons automatiquement »**
  (cochée par défaut) ; bouton « Rechercher maintenant » ; pause/reprise
  aussi depuis l'indicateur de la barre du haut.

## Raccourcis clavier (écran « Doublons »)

| Touche | Action |
|---|---|
| ↑ ↓ | groupe précédent / suivant |
| ← → | copie précédente / suivante dans le groupe |
| Espace | coche / décoche « À la corbeille » sur la copie sélectionnée |
| K | garder la copie sélectionnée |
| C | comparer la copie sélectionnée avec la copie gardée |
| Entrée | ouvrir la copie en grand |
| Suppr | ouvre le récapitulatif (n'efface rien par lui-même) |
| Échap | retour à la bibliothèque |

## Messages

| Situation | Message |
|---|---|
| Aucun doublon | « Aucun doublon trouvé. » (+ « L'analyse continue… » si elle n'est pas finie) |
| Toutes les copies écartées | « Gardez au moins une copie de chaque photo. » |
| Fichier changé depuis l'analyse | « Ce fichier a changé depuis l'analyse. Il n'a pas été touché ; son groupe va être recalculé. » |
| Disque sans corbeille | « Ce disque n'a pas de corbeille : Picazoo ne supprime pas ce fichier. Vous pouvez le supprimer vous-même depuis l'Explorateur. » |
| Fichier verrouillé | « Ce fichier est utilisé par un autre programme ou protégé en écriture. Il n'a pas été touché. » |
| Annulation, fichier absent de la corbeille | « Ce fichier n'est plus dans la corbeille : il ne peut pas être restauré. » |
| Annulation, emplacement occupé | « Un fichier du même nom existe déjà à cet emplacement : celui-ci n'a pas été restauré. » |
| Fichier en ligne seulement | dans le détail d'analyse : « Disponible uniquement en ligne — non analysé pour ne pas le télécharger. » |

## Garanties

- Aucune action de ces écrans ne supprime ni ne déplace de fichier sans
  passer par le récapitulatif et le bouton « Mettre à la corbeille » ; la
  touche Suppr et l'application en lot ouvrent le récapitulatif, jamais
  l'action directement.
- Le fil de l'interface ne lit aucun fichier de l'utilisateur et ne calcule
  aucune empreinte.

# Contrat d'interface utilisateur — 001

Ce que l'utilisateur voit et peut faire. Les tests d'acceptation et la
validation manuelle ([quickstart.md](../quickstart.md)) s'appuient sur ce
contrat. Tous les textes sont en français.

## Écrans

| Écran | Quand | Contenu |
|---|---|---|
| **Accueil (premier lancement)** | `settings.onboarding_done = 0` | « Où sont vos photos ? » ; choix 1 : « Images, Bureau et Documents (recommandé) » ; choix 2 : « Choisir des dossiers… » ; bouton « Commencer » |
| **Bibliothèque** | écran principal | barre du haut ; liste des dossiers à gauche ; grille continue à droite ; barre du bas |
| **Visionneuse** | double-clic ou Entrée sur une vignette | élément en grand sur fond sombre, barre d'outils, panneau d'informations repliable |
| **Dossiers surveillés** | menu Outils › Dossiers surveillés… | liste des racines avec leur état ; ajouter, retirer, exclure un sous-dossier |

## Bibliothèque

- **Barre du haut** : menu Fichier / Affichage / Outils ; indicateur
  d'analyse discret (« Analyse : 4 210 / 20 000 » + bouton pause/reprise),
  invisible au repos.
- **Liste des dossiers** : bascule « Par année | Arborescence » en tête de
  liste ; chaque dossier montre son nom, son nombre d'éléments et sa date ;
  une racine indisponible est grisée avec « (indisponible) ».
- **Grille** : un en-tête par dossier (nom, date, nombre, chemin au survol),
  puis ses vignettes ; une vidéo porte un symbole ▶ et sa durée
  (`m:ss` ou `h:mm:ss`) ; un fichier illisible a une vignette « image
  illisible », un RAW sans aperçu une vignette « aperçu indisponible ».
- **Barre du bas** : curseur de taille des vignettes ; nombre d'éléments
  sélectionnés ; menus de tri (dossiers, éléments).
- **Menu contextuel d'un élément** : « Ouvrir », « Afficher dans
  l'Explorateur », « Ouvrir avec l'application par défaut ». Aucune
  commande qui modifie un fichier dans la 001.

## Raccourcis clavier

| Touche | Bibliothèque | Visionneuse |
|---|---|---|
| ← → ↑ ↓ | déplace la sélection | ← → : précédent / suivant (passe au dossier voisin) |
| Entrée | ouvre l'élément sélectionné | — |
| Échap | vide la sélection | retour à la bibliothèque, sur la vignette de l'élément courant |
| Ctrl+clic / Maj+clic | ajoute / étend la sélection | — |
| Ctrl+A | sélectionne tout le dossier courant | — |
| Ctrl+molette ou Ctrl + / Ctrl − | taille des vignettes | zoom |
| Molette | défilement | zoom centré sur le pointeur |
| 0 | — | ajuster à la fenêtre |
| 1 | — | taille réelle (100 %) |
| F11 ou F | — | plein écran |
| I | — | affiche / masque les informations |
| Espace | — | lecture / pause (vidéo) |
| Ctrl+Maj+E | afficher dans l'Explorateur | afficher dans l'Explorateur |
| F5 | réanalyser les dossiers surveillés | — |

## Messages

| Situation | Message |
|---|---|
| Format non affichable ou non lisible | « Picazoo ne sait pas afficher ce fichier. » + bouton « Ouvrir avec l'application par défaut » |
| Dossier sans droit d'accès | dans Dossiers surveillés : « Accès refusé — ce dossier est ignoré. » (aucune fenêtre bloquante pendant l'analyse) |
| Racine indisponible | dans la liste : « (indisponible) » ; dans Dossiers surveillés : « Disque ou dossier introuvable. Il sera de nouveau pris en compte dès qu'il sera accessible. » |
| Retrait d'un dossier surveillé | confirmation : « Retirer « X » de Picazoo ? Ses photos ne seront plus affichées. Aucun fichier ne sera supprimé de votre ordinateur. » |
| Stockage de Picazoo inaccessible ou disque plein | « Picazoo ne peut plus enregistrer sa bibliothèque (espace disque insuffisant ?). Vos photos ne sont pas concernées. » |

## Garanties

- Aucune action de cet écran ne crée, ne modifie, ne déplace, ne renomme ni
  ne supprime de fichier dans les dossiers de l'utilisateur (FR-010,
  SC-006).
- Le thread de l'interface ne fait ni lecture de disque ni décodage : aucune
  action ne fige la fenêtre (SC-003).

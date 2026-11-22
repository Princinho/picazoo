# Quickstart — valider la 005

Prérequis et compilation : comme la 001
([quickstart de la 001](../001-bibliotheque-visionneuse/quickstart.md)).
La compilation produit en plus `build\windows-release\picazoo-viewer.exe`.

## Tests automatisés

```powershell
ctest --preset windows-release -R "folderlisting|browseorder|zoommath|folderbrowser|associations|singleinstance"
```

Attendu : tout passe ; `tst_associations` écrit seulement sous une clé de
test de `HKCU` et la supprime ; `tst_folderbrowser` vérifie qu'aucun fichier
du dossier de test n'a changé.

## Validation manuelle

Préparer `C:\PicazooTest\Visionneuse` : 30 photos (`photo1.jpg` à
`photo30.jpg`, dont une verticale, un HEIC, un RAW, un GIF animé, un JPEG
tronqué, une petite icône PNG), une vidéo MP4 et un fichier texte.

1. **Ouverture (US1)** : clic droit sur `photo2.jpg` › Ouvrir avec ›
   `picazoo-viewer.exe`. → Voile sur le bureau, photo en moins d'une
   seconde ; aucune fenêtre de bibliothèque.
2. **Instance unique** : la visionneuse ouverte, ouvrir `photo7.jpg` de la
   même façon. → La même fenêtre passe à `photo7.jpg`.
3. **Parcours (US2)** : fenêtre de l'Explorateur triée par nom : → mène à
   `photo8.jpg` ; vingt appuis rapides suivent sans retard ; après
   `photo30.jpg`, « Retour à la première photo ». Trier la fenêtre par taille
   décroissante, rouvrir une photo : le parcours suit cet ordre. La vidéo et
   le texte n'apparaissent jamais.
4. **Zoom (US3)** : curseur sur un détail, 5 crans de molette : le détail
   reste sous le curseur et devient net ; glisser ; double-clic → ajusté.
   L'icône PNG s'affiche à sa taille réelle.
5. **Voile** : F11 → voile opaque, F11 → semi-transparent ; désactiver les
   effets de transparence de Windows → voile opaque. Désactiver les
   animations de Windows → passages immédiats.
6. **Association (US4)** : installer avec l'installateur : les JPEG
   s'ouvrent toujours avec l'application habituelle, Picazoo est dans
   « Ouvrir avec ». Première ouverture : « Ne plus demander » → rien ne
   change dans Windows. Menu ⋯ › Ouvrir vos photos avec Picazoo… → page des
   applications par défaut de Windows.
7. **Lecture seule** : comparer `dir /s /a` de `C:\PicazooTest` et de
   `%LOCALAPPDATA%\Picazoo` avant et après les étapes 1 à 5 : seul
   `viewer.ini` a changé.
8. **Application complète (US5)** : sur une photo d'un dossier surveillé,
   « Ouvrir dans Picazoo » → la bibliothèque montre la photo sélectionnée.
   Sur une photo hors bibliothèque → proposition d'ajouter le dossier ;
   refuser → dossiers surveillés inchangés.

## Mesure du temps d'ouverture

```powershell
$env:PICAZOO_VIEWER_TIMING=1; .\build\windows-release\picazoo-viewer.exe C:\PicazooTest\Visionneuse\photo2.jpg
```

Le journal de débogage (DebugView) donne le temps entre le lancement et la
première image affichée (seuil : 1 s, 2 s à froid).

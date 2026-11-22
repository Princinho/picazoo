# Contrat de ligne de commande

## `picazoo-viewer.exe`

| Appel | Effet | Code de sortie |
|---|---|---|
| `picazoo-viewer.exe "<chemin>"` | ouvre la photo (ou la transmet à la visionneuse déjà ouverte) | 0 |
| `picazoo-viewer.exe "<c1>" "<c2>" …` | ouvre `<c1>` ; les autres sont ignorés | 0 |
| `picazoo-viewer.exe` | sans chemin : message « Ouvrez une photo depuis l'Explorateur » puis fermeture | 0 |
| `picazoo-viewer.exe --register [--machine]` | inscrit Picazoo auprès de Windows (HKCU ou HKLM), sans interface | 0, sinon 1 |
| `picazoo-viewer.exe --unregister [--machine]` | retire cette inscription | 0, sinon 1 |

Variables de développement : `PICAZOO_DATA_DIR` (emplacement de
`viewer.ini`), `PICAZOO_SCREENSHOT=<fichier.png>[;délai ms]` (capture puis
fermeture), `PICAZOO_REGISTRY_ROOT=<sous-clé de HKCU>` (tests : redirige
l'inscription), `PICAZOO_VIEWER_TIMING=1` (journal des temps d'ouverture
sur la sortie de débogage).

Un chemin relatif est résolu par rapport au dossier courant ; un argument
`file:///…` est accepté.

## `picazoo.exe`

| Appel | Effet |
|---|---|
| `picazoo.exe` | comme avant ; si Picazoo est déjà ouvert, le ramène au premier plan |
| `picazoo.exe --show "<chemin>"` | montre la photo dans la bibliothèque, ou propose d'ajouter son dossier (V12) ; transmis à l'instance déjà ouverte le cas échéant |

## Canal d'instance unique

Tube local `Picazoo.Viewer.<id de session>` (resp. `Picazoo.App.<id>`) ;
message : chemin absolu en UTF-8 terminé par `\n`. Aucune autre commande
n'est acceptée ; un message mal formé est ignoré.

# Picazoo

Gestionnaire et lecteur de photos et de vidéos pour Windows, dans l'esprit de
Picasa : on indique les dossiers à surveiller, Picazoo trouve tout seul les
photos et les vidéos, les affiche par dossier en vignettes, et un double-clic
les ouvre en grand.

**Picazoo ne modifie, ne déplace et ne renomme jamais vos fichiers, et ne les
supprime que lorsque vous le confirmez : il les envoie alors à la corbeille de
Windows, jamais en effacement définitif, et l'opération peut être annulée.** Il
n'écrit rien dans vos dossiers ; ses données (index, vignettes, réglages,
journal) sont dans `%LOCALAPPDATA%\Picazoo`.

Feuille de route : [docs/feuille-de-route.md](docs/feuille-de-route.md).
Spécifications : [specs/](specs/).

## Télécharger

Page des versions : <https://github.com/Princinho/picazoo/releases>

- **Version stable** (`Picazoo X.Y.Z`) : publiée à chaque étape validée.
- **Dernière version de développement** (préversion `dev`) : reconstruite
  automatiquement à chaque modification de `main`. Plus récente, mais elle peut
  contenir des défauts.

Chaque version existe en deux formats, pour Windows 10 et 11 (64 bits) :

- `Picazoo-…-win64-setup.exe` : installateur (menu Démarrer, désinstallation
  depuis les paramètres de Windows) ;
- `Picazoo-…-win64.zip` : version portable ; décompresser puis lancer
  `picazoo\bin\picazoo.exe`. Si Windows signale une DLL `MSVCP140` ou
  `VCRUNTIME140` manquante, installer les
  [bibliothèques Visual C++](https://aka.ms/vs/17/release/vc_redist.x64.exe)
  (l'installateur s'en charge quand il est lancé pour tous les utilisateurs).

La signature des versions par SignPath Foundation est en cours de mise en place
(voir la [politique de signature du code](#politique-de-signature-du-code-code-signing-policy)).
Tant qu'une version n'est pas signée, Windows SmartScreen peut afficher
« Windows a protégé votre ordinateur » : cliquer sur « Informations
complémentaires » puis « Exécuter quand même ». Une version signée n'affiche plus
« Éditeur inconnu », mais SmartScreen peut encore avertir pendant les premières
versions, le temps que leur réputation se construise.

## Fonctionnalité 001 — bibliothèque et visionneuse

- Dossiers surveillés (Images, Bureau, Documents par défaut), exclusions.
- Analyse en arrière-plan avec pause et reprise ; bibliothèque tenue à jour en
  direct et rattrapée au démarrage ; disques débranchés conservés.
- Liste des dossiers par année ou en arborescence ; grille continue avec un
  en-tête par dossier ; taille des vignettes, tris, sélection multiple.
- Formats : JPEG, PNG, GIF, BMP, TIFF, WebP, HEIC ; RAW (CR2, CR3, NEF, ARW,
  ORF, RW2, RAF, DNG) par leur aperçu intégré ; vidéos MP4, MOV, AVI, MKV.
- Visionneuse : ajustement, zoom, déplacement, plein écran, informations,
  précédent / suivant d'un dossier à l'autre, lecture des vidéos.

## Fonctionnalité 005 — visionneuse autonome

Comme la Visionneuse de photos Picasa : `picazoo-viewer.exe`, installé à côté
de `picazoo.exe`, ouvre une photo double-cliquée dans l'Explorateur dans un
voile semi-transparent posé sur le bureau, sans ouvrir la bibliothèque.

- Parcours des images du dossier dans l'ordre de la fenêtre de l'Explorateur
  (ordre naturel des noms à défaut), en boucle ; transitions animées.
- Zoom à la molette centré sur le curseur, déplacement, double-clic
  ajuster ⇄ 100 %, voile opaque avec F11, informations (I), bande des voisines.
- « Ouvrir dans Picazoo » montre la photo dans la bibliothèque, ou propose
  d'ajouter son dossier.
- **Rien n'est imposé** : l'installateur inscrit Picazoo dans « Ouvrir avec »
  sans changer les applications par défaut ; la visionneuse propose une fois de
  vous emmener au réglage de Windows, où c'est vous qui choisissez. Avec la
  version portable, l'inscription se fait à la main :

```powershell
.\picazoo\bin\picazoo-viewer.exe --register
```

  et se retire avec `--unregister`.

## Politique de signature du code (Code signing policy)

Free code signing provided by [SignPath.io](https://about.signpath.io), certificate by [SignPath Foundation](https://signpath.org)

(Signature de code gratuite fournie par SignPath.io, certificat de SignPath Foundation.)

- **Ce qui est signé** : `picazoo.exe`, `picazoo-viewer.exe` et l'installateur
  `Picazoo-…-win64-setup.exe`. Ils sont construits par
  [GitHub Actions](.github/workflows/build.yml) à partir du code de ce dépôt, sur
  `main` (préversion `dev`) ou sur un tag `vX.Y.Z` (version stable), jamais à partir
  d'une pull request. Chaque demande de signature est approuvée à la main.
- **Ce qui n'est pas signé avec ce certificat** : les bibliothèques d'autres projets
  livrées avec Picazoo (Qt, FFmpeg de Qt Multimedia, libheif, libde265, LibRaw,
  libjpeg-turbo, SQLite, xxHash, bibliothèques Visual C++). Elles sont livrées
  telles quelles, avec la signature de leur éditeur quand elles en ont une.
- **Rôles** :
  - auteurs (*committers*) et relecteurs (*reviewers*) :
    [Princinho](https://github.com/Princinho) ;
  - approbateurs (*approvers*) : [Princinho](https://github.com/Princinho).

  Toute contribution extérieure passe par une pull request, relue par un membre de
  l'équipe avant d'être fusionnée. Les membres de l'équipe utilisent
  l'authentification à plusieurs facteurs sur GitHub et sur SignPath.
- **Vie privée** : This program will not transfer any information to other
  networked systems unless specifically requested by the user or the person
  installing or operating it. (Ce programme ne transmet aucune information à
  d'autres systèmes en réseau, sauf si l'utilisateur ou la personne qui l'installe
  ou l'utilise le demande expressément.) Picazoo n'a ni télémétrie, ni compte, ni
  recherche de mises à jour ; il ne se connecte à aucun serveur. La seule
  communication est locale, entre `picazoo-viewer.exe` et `picazoo.exe` sur le même
  PC.

Détails et marche à suivre : [docs/signature.md](docs/signature.md).

## Prérequis

- Windows 10 ou 11, 64 bits.
- Visual Studio Build Tools 2026 (charge « Développement Desktop en C++ » :
  MSVC, CMake, Ninja et vcpkg).
- Qt 6.12 LTS, kit `msvc2022_64`, avec Qt Multimedia, Qt Image Formats et Qt
  Shader Tools. La variable `QT_DIR` pointe vers le kit, par exemple
  `C:\Qt\6.12.0\msvc2022_64`.

## Compiler, tester, mesurer

Dans un « Developer PowerShell for VS 2026 », à la racine du dépôt :

```powershell
cmake --preset windows-release
```

```powershell
cmake --build --preset windows-release
```

```powershell
ctest --preset windows-release
```

La première configuration compile les dépendances vcpkg (libjpeg-turbo,
libheif + libde265, LibRaw, SQLite, xxHash, TinyEXIF) : comptez une vingtaine de
minutes.

## Fonctionnalité 002 — détection des doublons

- Copies identiques (même contenu) et même photo en une autre taille, un autre
  format, une autre compression, tournée ou légèrement retouchée, avec un niveau
  de confiance en mots : « Identique », « Même image », « Très ressemblante ».
  Les vidéos sont comparées par leur contenu. Une photo RAW et son JPEG sont
  regroupés, le RAW étant gardé.
- Écran « Doublons » (en tête de la liste des dossiers) : copie suggérée à
  garder et sa raison, comparaison côte à côte, « Ce ne sont pas des doublons »,
  « Garder toutes » ; les choix sont retenus.
- Mise à la corbeille après un récapitulatif, au moins une copie toujours
  gardée, vérification juste avant d'agir, journal et annulation
  (Outils › Journal des opérations).
- Analyse discrète en arrière-plan : seulement quand la bibliothèque est à
  jour, lectures à débit limité et à faible priorité, pause sur batterie ou dans
  la visionneuse, reprise après fermeture ; désactivable dans Outils.

## Fonctionnalité 006 — gestion des doublons en lot

- Sélection de groupes (case, Maj + clic, Ctrl + A sur les groupes affichés) et
  actions en lot : revenir à la suggestion, garder les copies d'un dossier,
  « Garder toutes », « Ce ne sont pas des doublons », mise à la corbeille ; chaque
  action en lot s'annule (message « Annuler », Ctrl + Z, ou « Écartés »).
- Panneau « Dossiers » : paires de dossiers qui partagent des copies (une
  sauvegarde en double se règle en une action), doublons internes, règles
  durables « toujours préférer / écarter ce dossier » ; une règle n'écarte jamais
  un RAW ou une copie de plus grande résolution hors « Identique ».
- Vue compacte (un groupe par ligne, dépliable), filtres combinés (dossier, paire,
  type, état, taille minimale, années) et tris ; « Voir les doublons de ce
  dossier » depuis le menu d'un dossier.
- Récapitulatif par niveau et par dossier d'origine, « Très ressemblante » non
  vérifiées exclues par défaut ; la corbeille ne porte que sur les groupes
  sélectionnés (ou le groupe courant) ; une opération en cours peut être arrêtée.

## Fonctionnalité 007 — ergonomie des visionneuses et clic droit

- **Plein écran** (visionneuse intégrée) : un bouton « Quitter le plein écran »
  apparaît en haut à droite dès que la souris bouge.
- **Zoom** plus doux, avec de l'élan après un tour de molette rapide, dans les
  deux visionneuses ; un geste de zoom avant commencé sous 90 % s'arrête à
  100 % (un nouveau geste va au-delà).
- **Clavier** : ↑ / ↓ zooment, ← / → changent de photo ; Suppr met à la
  corbeille, Ctrl + C copie l'image, Ctrl + Maj + C copie le chemin,
  Alt + Entrée ouvre les Propriétés, touche Menu ou Maj + F10 ouvre le menu.
- **Clic droit** partout : vignettes (une ou plusieurs), zone vide de la grille,
  dossiers, image des deux visionneuses. Copier, copier le chemin, Propriétés de
  Windows, définir comme fond d'écran (annulable), mettre à la corbeille (après
  confirmation, au journal des opérations et annulable, y compris depuis la
  visionneuse autonome).

Photos de test pour essayer l'écran « Doublons » à la main :

```powershell
.\build\windows-release\picazoo-testdata.exe --duplicates C:\PicazooTest\Doublons
```

Pour lancer l'application depuis le dossier de compilation, les DLL de Qt doivent
être trouvées : ajoutez `$env:QT_DIR\bin` au `PATH`, ou installez le dossier
autonome :

```powershell
cmake --install build/windows-release --prefix dist/picazoo
```

L'application installée est `dist\picazoo\bin\picazoo.exe`.

Banc d'essai (photos générées dans un dossier temporaire, jamais vos dossiers) :

```powershell
.\build\windows-release\picazoo-bench.exe --items 20000 --viewer --live
```

```powershell
.\build\windows-release\picazoo-bench.exe --items 100000 --warm --index-only
```

```powershell
.\build\windows-release\picazoo-bench.exe --items 20000 --duplicates
```

Les tests qui ont besoin de vrais fichiers RAW, HEIC ou vidéo les cherchent dans
[tests/fixtures](tests/fixtures/README.md) et sont ignorés s'ils manquent.

## Licence

Picazoo est un logiciel libre, distribué sous la licence publique générale GNU,
version 3 (GPL-3.0) : voir [LICENSE](LICENSE).

## Licences des composants

Qt (LGPLv3, liaison dynamique), libheif et libde265 (LGPLv3), LibRaw (LGPL 2.1 /
CDDL), libjpeg-turbo (BSD/IJG), SQLite (domaine public), TinyEXIF (BSD-2),
xxHash (BSD-2). L'encodeur x265 (GPL) de libheif est volontairement désactivé.

## Intégration continue et publication

Le workflow [.github/workflows/build.yml](.github/workflows/build.yml) tourne sur
GitHub Actions (Windows) : il installe Qt 6.12 depuis download.qt.io
([scripts/ci/install_qt.py](scripts/ci/install_qt.py)), compile les dépendances
vcpkg au commit du `builtin-baseline` de `vcpkg.json` (mises en cache), compile en
Release, lance tous les tests, puis fabrique le zip portable et l'installateur
([installer/picazoo.iss](installer/picazoo.iss), Inno Setup).

- **Pull request** : compilation et tests seulement.
- **Push sur `main`** : la préversion `dev` est remplacée par la nouvelle version.
- **Tag `vX.Y.Z`** : une version stable est publiée.

Publier une version stable :

```powershell
git tag v0.2.0
```

```powershell
git push origin v0.2.0
```

La version affichée par l'application (titre de la fenêtre) vient de l'option
CMake `PICAZOO_VERSION`, fixée par la CI d'après le tag ou le commit.
La publication utilise le jeton fourni par GitHub Actions. La signature
(SignPath) ne s'active que si le secret `SIGNPATH_API_TOKEN` existe ; sans lui, les
paquets sont publiés non signés ([docs/signature.md](docs/signature.md)).

# Picazoo

Gestionnaire et lecteur de photos et de vidéos pour Windows, dans l'esprit de
Picasa : on indique les dossiers à surveiller, Picazoo trouve tout seul les
photos et les vidéos, les affiche par dossier en vignettes, et un double-clic
les ouvre en grand.

**Picazoo ne modifie, ne déplace, ne renomme et ne supprime jamais vos
fichiers.** Il n'écrit rien dans vos dossiers ; ses données (index, vignettes,
réglages) sont dans `%LOCALAPPDATA%\Picazoo` et peuvent être supprimées à tout
moment (elles se reconstruisent).

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

Les exécutables ne sont pas signés : Windows SmartScreen peut afficher
« Windows a protégé votre ordinateur ». Cliquer sur « Informations
complémentaires » puis « Exécuter quand même ».

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

Les tests qui ont besoin de vrais fichiers RAW, HEIC ou vidéo les cherchent dans
[tests/fixtures](tests/fixtures/README.md) et sont ignorés s'ils manquent.

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
Aucun secret n'est nécessaire : la publication utilise le jeton fourni par GitHub
Actions.

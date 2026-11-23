# Signature du code avec SignPath Foundation

Objectif : que l'installateur et les exécutables de Picazoo soient signés, pour que
Windows n'affiche plus « Éditeur inconnu ». La signature est fournie gratuitement aux
projets open source par [SignPath Foundation](https://signpath.org) (service
[SignPath.io](https://about.signpath.io)). Le certificat est au nom de « SignPath
Foundation », pas au nôtre.

La signature seule ne fait pas disparaître tout de suite l'écran de SmartScreen
« Windows a protégé votre ordinateur » : SmartScreen se fie aussi à la réputation, qui
s'accumule au fil des téléchargements de versions signées.

## Ce qui est en place dans le dépôt

| Élément | Rôle |
|---|---|
| [.github/workflows/build.yml](../.github/workflows/build.yml) | Compile, envoie nos `.exe` à SignPath, fabrique le zip et l'installateur avec les fichiers signés, fait signer l'installateur, vérifie, publie. |
| [installer/signpath/picazoo-binaires.xml](../installer/signpath/picazoo-binaires.xml) | Configuration d'artefact SignPath pour `picazoo.exe` et `picazoo-viewer.exe`. |
| [installer/signpath/picazoo-installateur.xml](../installer/signpath/picazoo-installateur.xml) | Configuration d'artefact SignPath pour `Picazoo-…-win64-setup.exe`. |
| [scripts/ci/verify_signatures.ps1](../scripts/ci/verify_signatures.ps1) | `signtool verify /pa` sur tous les `.exe` et `.dll` livrés ; résultat dans le résumé de l'exécution. |
| [src/core/platform/win/picazoo_versioninfo.rc.in](../src/core/platform/win/picazoo_versioninfo.rc.in), directives `VersionInfo*` de [installer/picazoo.iss](../installer/picazoo.iss) | Nom (« Picazoo ») et version de produit des fichiers. SignPath les contrôle avant de signer. |
| Section « Politique de signature du code » du [README](../README.md#politique-de-signature-du-code-code-signing-policy) | Page exigée par SignPath Foundation : mention de SignPath, rôles, vie privée. |

### Déroulement dans la CI

1. Compilation et tests (comme avant).
2. Dossier autonome `dist/picazoo` (`cmake --install`).
3. **Signature des binaires** : `picazoo.exe` et `picazoo-viewer.exe` sont téléversés
   comme artefact GitHub, envoyés à SignPath (configuration `picazoo-binaires`), puis
   les fichiers signés remplacent les originaux dans `dist/picazoo/bin`.
4. Zip portable et installateur Inno Setup, fabriqués à partir des fichiers signés.
5. **Signature de l'installateur** (configuration `picazoo-installateur`).
6. **Vérification** : nos fichiers doivent être signés, sinon l'exécution échoue. Les
   fichiers d'autres projets sont seulement comptés.
7. Publication (préversion `dev` ou version stable).

Les étapes 3, 5 et le mode strict de l'étape 6 ne s'exécutent que si :

- l'événement est un **push sur `main` ou sur un tag `v*`** (jamais une pull request,
  ni un lancement manuel) ;
- **et** le secret `SIGNPATH_API_TOKEN` existe.

Sinon elles sont sautées et les paquets sont publiés non signés, comme aujourd'hui.

### Ce qui n'est pas signé, et pourquoi

Les conditions de SignPath Foundation (« Sign your own binaries only ») interdisent de
signer avec notre certificat les binaires d'autres projets. On peut en revanche les
livrer non signés dans un paquet signé. Restent donc tels quels :

- les DLL et plugins de Qt (dont FFmpeg de Qt Multimedia), signés ou non par leur éditeur ;
- les DLL compilées par vcpkg (libheif, libde265, LibRaw, libjpeg-turbo, SQLite,
  xxHash) ;
- `vc_redist.x64.exe` (signé par Microsoft).

Picazoo ne produit pas de DLL à lui (`picazoo_core` est une bibliothèque statique) : il
n'y a donc que deux exécutables à signer, plus l'installateur. SignPath Foundation se
réserve le droit d'exiger plus tard que tous les fichiers d'un paquet signé soient
signés. Ce n'est pas le cas aujourd'hui.

Le programme de désinstallation créé par Inno Setup (`unins000.exe`) n'est pas signé
non plus. Il faudrait une signature appelée pendant la compilation de l'installateur
(directive `SignTool`), ce que ne permet pas l'envoi d'un artefact à SignPath.

### Versions de fichier

SignPath Foundation demande que les fichiers signés portent le nom du projet et une
même version de produit, contrôlés par SignPath avant la signature. La CI calcule une
version numérique `a.b.c.d` (sortie `numeric` de l'étape « Version ») :

- tag `v0.2.0` → `0.2.0.0` ;
- push sur `main` → `<version de project() dans CMakeLists.txt>.<numéro d'exécution>`,
  par exemple `0.1.0.57`.

Cette version est écrite dans `ProductVersion` et `FileVersion` des deux `.exe` et de
l'installateur, et passée à SignPath (paramètre `version`). La version affichée par
l'application (`dev-1a2b3c4`, `0.2.0`) ne change pas : elle reste dans le titre de la
fenêtre et dans le champ « Commentaires » des propriétés du fichier.

## Ce qu'il vous reste à faire

### 1. Préparer le dépôt

- **Licence** : SignPath Foundation exige une licence open source approuvée par l'OSI
  (sans double licence commerciale), présente dans le dépôt. Le dépôt n'a pas encore de
  fichier `LICENSE`. À choisir et à ajouter avant la candidature. La GPL-3.0 est
  compatible avec les composants LGPL utilisés (Qt, libheif, libde265, LibRaw).
- **Authentification à plusieurs facteurs** sur votre compte GitHub (obligatoire pour
  chaque membre de l'équipe, sur GitHub comme sur SignPath).
- **Fusionner cette branche** sur `main` : la section « Politique de signature du
  code » doit être visible sur la page du projet au moment de la candidature.
- Vérifier qu'au moins une version est publiée (la préversion `dev` suffit à montrer le
  logiciel « sous la forme à signer »), et que le README décrit bien ce que fait
  Picazoo.

Points d'attention pour l'acceptation :

- SignPath Foundation n'a aucune obligation d'accepter un projet. Elle vérifie la
  **réputation** du projet : un dépôt récent, sans étoiles ni utilisateurs visibles,
  peut être refusé ou mis en attente. Des versions publiées, un README clair et un
  peu d'activité aident.
- La règle « pas de double licence commerciale pour aucun composant » vise d'abord le
  projet lui-même. Qt est proposé par son éditeur sous LGPL ou sous licence
  commerciale, et Picazoo l'utilise sous LGPL. D'autres projets signés par SignPath
  Foundation utilisent Qt, mais posez la question dans la candidature si vous avez un
  doute.

### 2. Candidater

Formulaire : <https://signpath.org/apply>. À fournir notamment :

- le dépôt : `https://github.com/Princinho/picazoo` ;
- la page des versions : `https://github.com/Princinho/picazoo/releases` ;
- le lien vers la politique de signature :
  `https://github.com/Princinho/picazoo#politique-de-signature-du-code-code-signing-policy` ;
- la licence choisie ;
- le système de compilation : GitHub Actions, sur des machines hébergées par GitHub
  (`windows-latest`).

Conditions complètes : <https://signpath.org/terms>.

### 3. Configurer SignPath (après acceptation)

Sur <https://app.signpath.io>, dans l'organisation créée pour Picazoo :

1. Activer l'authentification à plusieurs facteurs sur votre compte.
2. **Projet** : celui que SignPath Foundation a créé (sinon en créer un), par exemple
   avec le slug `picazoo`.
3. **Trusted build system** : lier au projet le système prédéfini **GitHub.com**.
   Facultatif : installer l'application GitHub de SignPath sur le dépôt (elle sert à
   l'évaluation du journal d'audit).
4. **Artifact configurations** : en créer deux, en collant le contenu des fichiers du
   dépôt :
   - slug `picazoo-binaires` ← [installer/signpath/picazoo-binaires.xml](../installer/signpath/picazoo-binaires.xml)
     (la mettre par défaut) ;
   - slug `picazoo-installateur` ← [installer/signpath/picazoo-installateur.xml](../installer/signpath/picazoo-installateur.xml).

   Les slugs sont écrits dans `build.yml` : s'ils diffèrent, modifier les entrées
   `artifact-configuration-slug`.
5. **Signing policies** : SignPath Foundation fournit en général `test-signing`
   (certificat de test, sans approbation) et `release-signing` (certificat de SignPath
   Foundation, approbation manuelle). Vous y êtes **approbateur**. Si l'interface le
   permet, limiter `release-signing` aux références `refs/heads/main` et
   `refs/tags/v*`.
6. **Jeton d'API** : créer un utilisateur CI (ou utiliser le vôtre) avec le rôle
   *Submitter* sur la politique de signature, et générer son jeton d'API.
7. Noter l'**ID de l'organisation** (affiché dans les paramètres de l'organisation).

### 4. Ajouter les identifiants dans GitHub

Dans le dépôt : *Settings* > *Secrets and variables* > *Actions*.

| Nom | Type | Valeur |
|---|---|---|
| `SIGNPATH_API_TOKEN` | **secret** | le jeton d'API de l'étape 3.6 |
| `SIGNPATH_ORGANIZATION_ID` | variable | l'ID de l'organisation |
| `SIGNPATH_PROJECT_SLUG` | variable | par exemple `picazoo` |
| `SIGNPATH_SIGNING_POLICY_SLUG` | variable | `release-signing` (ou `test-signing` pour un premier essai) |

Ou avec `gh` (le jeton est demandé de façon masquée) :

```powershell
gh variable set SIGNPATH_ORGANIZATION_ID --repo Princinho/picazoo --body "<ID de l'organisation>"
```

```powershell
gh variable set SIGNPATH_PROJECT_SLUG --repo Princinho/picazoo --body "picazoo"
```

```powershell
gh variable set SIGNPATH_SIGNING_POLICY_SLUG --repo Princinho/picazoo --body "release-signing"
```

```powershell
gh secret set SIGNPATH_API_TOKEN --repo Princinho/picazoo
```

Ajouter les trois variables **avant** le secret : c'est le secret qui active la
signature, dès le push suivant sur `main`.

### 5. Premier essai, sur un tag

1. Créer un tag d'essai et le pousser :

   ```powershell
   git tag v0.1.0-essai-signature
   ```

   ```powershell
   git push origin v0.1.0-essai-signature
   ```

2. Dans GitHub Actions, l'exécution s'arrête sur « Signature des binaires
   (SignPath) » : SignPath vous envoie la demande à approuver (lien aussi dans le
   journal de l'étape). L'approuver dans l'heure (au-delà, l'étape échoue ; il suffit
   de relancer l'exécution).
3. Même chose pour « Signature de l'installateur (SignPath) ».
4. Lire le résumé de l'exécution, section **Signatures** : `picazoo.exe`,
   `picazoo-viewer.exe` et l'installateur doivent être « signé : oui », signataire
   « SignPath Foundation ».
5. Télécharger l'installateur depuis la version créée, clic droit > *Propriétés* >
   *Signatures numériques* : la signature doit être valide.
6. Supprimer la version d'essai et son tag :

   ```powershell
   gh release delete v0.1.0-essai-signature --repo Princinho/picazoo --yes --cleanup-tag
   ```

Si SignPath refuse la demande à cause des métadonnées (nom ou version du produit),
comparer la version indiquée par l'étape « Version » du journal avec *Propriétés* >
*Détails* du fichier, puis ajuster `installer/signpath/*.xml` dans l'interface de
SignPath.

### 6. Ensuite

- Chaque push sur `main` demande **deux approbations** (binaires puis installateur),
  sinon la préversion `dev` n'est pas publiée. Si c'est trop lourd, ne signer que les
  tags : dans `build.yml`, retirer `github.ref == 'refs/heads/main' ||` de la variable
  `SIGNER`.
- Chaque nouvelle personne ajoutée à l'équipe doit être listée dans la section
  « Politique de signature du code » du README, avec son rôle.

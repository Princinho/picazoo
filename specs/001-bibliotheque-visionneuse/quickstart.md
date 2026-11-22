# Quickstart — valider la 001

Guide pour compiler Picazoo et vérifier que la fonctionnalité 001 tient ses
promesses. Les détails techniques sont dans [research.md](research.md), le
modèle dans [data-model.md](data-model.md) et le comportement attendu dans
[contracts/ui-contract.md](contracts/ui-contract.md).

## Prérequis

- Windows 10 ou 11, 64 bits.
- Visual Studio Build Tools 2026 avec la charge « Développement Desktop en
  C++ » (MSVC, CMake, Ninja et vcpkg fournis) — **déjà installé**.
- Qt 6.12 LTS (dernier correctif), kit `msvc2022_64` ou plus récent, avec
  Qt Multimedia, Qt Image Formats et Qt Shader Tools — **à installer**.
  La variable `CMAKE_PREFIX_PATH` pointe vers ce kit (exemple :
  `C:\Qt\6.12.x\msvc2022_64`).

## Compiler

Depuis un « Developer PowerShell for VS 2026 », à la racine du dépôt :

```powershell
cmake --preset windows-release
```

```powershell
cmake --build --preset windows-release
```

La première configuration télécharge et compile les dépendances vcpkg
(LibRaw, libheif, libjpeg-turbo, SQLite, xxHash, TinyEXIF), ce qui prend
plusieurs minutes. Les suivantes sont rapides.

## Tests automatisés

```powershell
ctest --preset windows-release
```

Attendu : tous les tests passent, dont les tests d'intégration qui
vérifient qu'**aucun fichier des dossiers de test n'a changé** (SC-006).

## Mesures de performance

```powershell
.\build\windows-release\picazoo-bench.exe --items 20000 --viewer --live
```

```powershell
.\build\windows-release\picazoo-bench.exe --items 100000 --warm --index-only
```

| Mesure | Seuil | Critère |
|---|---|---|
| Premiers dossiers visibles après le choix des dossiers (20 000) | < 5 s | SC-001 |
| Analyse complète avec vignettes (20 000) | < 15 min | SC-001 |
| Ouverture d'une bibliothèque déjà analysée (100 000) | < 3 s | SC-002 |
| Ouverture d'une photo de 24 Mpx / passage à la suivante | < 1 s / < 0,5 s | SC-004 |
| Apparition d'une photo copiée dans un dossier surveillé | < 10 s | SC-005 |

Le banc d'essai génère ses images dans un répertoire temporaire et le
supprime à la fin ; il ne lit jamais tes vrais dossiers.

## Validation manuelle

Préparer un dossier `C:\PicazooTest` qui contient trois sous-dossiers de
photos (dont une photo prise à la verticale, un HEIC et un RAW), une vidéo
MP4 et une MKV, un fichier JPEG tronqué et un sous-dossier vide.

1. **Premier lancement (US1)** : lancer `picazoo.exe`, choisir « Choisir des
   dossiers… » puis `C:\PicazooTest`. → Les trois sous-dossiers
   apparaissent pendant l'analyse, pas le dossier vide ; le JPEG tronqué a
   une vignette « image illisible ».
2. **Visionneuse (US2)** : double-cliquer la 3e photo, appuyer 4 fois sur →,
   zoomer, appuyer sur Échap. → Retour sur la 7e vignette, sélectionnée ;
   la photo verticale s'affiche dans le bon sens ; I montre les
   informations.
3. **Vidéo (US3)** : les vidéos ont ▶ et leur durée ; la MKV se lit avec le
   son ; Espace met en pause.
4. **Mise à jour en direct (US4)** : Picazoo ouvert, copier 5 photos dans
   un sous-dossier, en supprimer une, en renommer une. → Tout est à jour en
   moins de 10 s. Fermer Picazoo, déplacer une photo vers un autre
   sous-dossier, relancer. → Elle apparaît au bon endroit.
5. **Disque indisponible (US4)** : ajouter un dossier sur une clé USB, fermer
   Picazoo, retirer la clé, relancer. → Le dossier est marqué
   « (indisponible) » et ses photos restent connues ; rebrancher la clé.
   → Il redevient normal.
6. **Préférences (US5)** : vignettes au plus grand, dossiers triés par nom,
   liste en arborescence ; fermer et relancer. → Les trois réglages sont
   conservés.
7. **Rien n'a bougé (SC-006)** : avant l'étape 1, enregistrer un instantané :

   ```powershell
   Get-ChildItem C:\PicazooTest -Recurse -File | Get-FileHash | Export-Csv $env:TEMP\avant.csv
   ```

   Après l'étape 6 (en excluant les changements faits à la main à
   l'étape 4), comparer avec un nouvel instantané : aucun fichier créé,
   modifié ni supprimé par Picazoo.

# Quickstart — valider la 002

Guide pour vérifier que la détection des doublons tient ses promesses. Les
choix techniques sont dans [research.md](research.md), le modèle dans
[data-model.md](data-model.md) et le comportement attendu dans
[contracts/ui-contract.md](contracts/ui-contract.md) et
[contracts/core-api.md](contracts/core-api.md).

## Prérequis

Ceux de la [001](../001-bibliotheque-visionneuse/quickstart.md) (Visual
Studio Build Tools 2026, Qt 6.12 LTS). La 002 n'ajoute aucune dépendance.
La corbeille de Windows doit être active sur le disque `C:` (réglage par
défaut).

## Compiler et tester

```powershell
cmake --preset windows-release
```

```powershell
cmake --build --preset windows-release
```

```powershell
ctest --preset windows-release
```

Attendu : tous les tests passent, dont :

- les tests d'exactitude de la détection, qui échouent si SC-001 (≥ 95 %
  des copies retrouvées, 100 % des identiques) ou SC-002 (aucun faux
  « Même image », < 1 % de faux « Très ressemblante ») n'est pas tenu ;
- les tests d'intégration de la corbeille, qui mettent de vrais fichiers de
  test à la corbeille puis les restaurent, et vérifient l'invariant SC-008
  (seuls les fichiers confirmés ont disparu, tous sont récupérables).

Ces tests travaillent dans un dossier temporaire et ne retirent de la
corbeille que leurs propres fichiers.

## Mesures de performance

Ne pas lancer pendant une compilation : le portable de développement est
sensible à la chauffe.

```powershell
.\build\windows-release\picazoo-bench.exe --items 20000 --duplicates
```

```powershell
.\build\windows-release\picazoo-bench.exe --items 5000 --duplicates --concurrent-copy
```

| Mesure | Seuil | Critère |
|---|---|---|
| Analyse complète des doublons, bibliothèque déjà indexée (20 000) | < 30 min | SC-003 |
| Défilement et ouverture d'une photo pendant l'analyse | fluide, < 1 s | SC-004 |
| Ralentissement d'une copie de fichier concurrente sur le même disque | ≤ 20 % | SC-005 |
| Apparition du groupe d'une copie ajoutée | < 1 min | SC-006 |
| Relance après interruption : éléments relus | 0 | SC-007 |

## Validation manuelle

Préparer `C:\PicazooTest\Doublons` avec le générateur de données de test
(nouvel outil de la 002, `tools/make-testdata`) :

```powershell
.\build\windows-release\picazoo-testdata.exe --duplicates C:\PicazooTest\Doublons
```

Il crée un original et ses copies (identiques renommées dans d'autres
sous-dossiers, réduite, recompressée, PNG, tournée, éclaircie), une paire
RAW + JPEG, une rafale de 5 photos, des images unies et 20 photos
distinctes. Ajouter ce dossier aux dossiers surveillés.

1. **Instantané avant (SC-008)** :

   ```powershell
   Get-ChildItem C:\PicazooTest -Recurse -File | Get-FileHash | Export-Csv $env:TEMP\avant-002.csv
   ```

2. **Analyse automatique (US4, FR-033)** : sans rien faire, attendre la fin
   de l'analyse de la bibliothèque. → L'indicateur passe à « Doublons »,
   puis l'entrée « Doublons » apparaît en tête de la liste. Pendant ce
   temps, faire défiler et ouvrir des photos : aucun à-coup.
3. **Copies identiques (US1)** : ouvrir « Doublons », filtre
   « Identiques ». → Le groupe de l'original et de ses copies renommées
   est là, avec la place récupérable.
4. **Même image et retouche (US3)** : filtre « Tous ». → Un seul groupe
   réunit l'original, la copie réduite, la recompressée, le PNG et la
   tournée (« Même image ») et l'éclaircie (« Très ressemblante ») ; la
   copie gardée est l'originale, raison « Plus grande résolution ». La
   rafale et les images unies n'apparaissent dans aucun groupe. La paire
   RAW + JPEG forme un groupe « Même image » où le RAW est gardé (« RAW
   d'origine »).
5. **Comparer (US3)** : C sur la copie recompressée. → Deux copies côte à
   côte ; zoomer sur l'une zoome l'autre.
6. **Mettre à la corbeille (US2)** : « Mettre à la corbeille… ». → Le
   récapitulatif liste exactement les fichiers cochés et la copie gardée ;
   Entrée ferme sans rien faire. Rouvrir, cliquer « Mettre à la
   corbeille ». → Les fichiers sont dans la corbeille de Windows, la
   bibliothèque ne les montre plus.
7. **Garde-fous (US2)** : dans un groupe, décocher la copie gardée et cocher
   toutes les autres. → « Gardez au moins une copie de chaque photo. »
   Modifier une copie avec un autre logiciel puis lancer sa mise à la
   corbeille. → Elle n'est pas touchée et le compte rendu le dit.
8. **Annuler (US2)** : Outils › Journal des opérations › « Annuler cette
   opération ». → Les fichiers reviennent à leur emplacement, identiques
   (comparer à l'instantané).
9. **Décisions retenues (US5)** : « Ce ne sont pas des doublons » sur un
   groupe, fermer et relancer Picazoo, « Rechercher maintenant ». → Le
   groupe ne revient pas.
10. **Disque sans corbeille (FR-018)** : copier deux photos identiques sur
    une clé USB surveillée et tenter d'en écarter une. → Le récapitulatif
    la range dans « Ne seront pas touchés — Ce disque n'a pas de
    corbeille » ; elle reste sur la clé.
11. **Batterie et pause (FR-029, FR-031)** : débrancher le secteur pendant
    une analyse. → « Doublons : en pause sur batterie », plus d'activité
    disque de Picazoo dans le Gestionnaire des tâches ; rebrancher. → Elle
    reprend.
12. **Instantané après (SC-008)** : refaire l'instantané et comparer. →
    Les fichiers de l'étape 6, restaurés à l'étape 8, sont revenus
    identiques ; seuls diffèrent la copie modifiée à la main à l'étape 7
    (et ce que Picazoo aurait mis à la corbeille après l'annulation, s'il y
    en a). Aucun fichier n'a été créé par Picazoo.

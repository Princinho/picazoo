# Quickstart: valider la gestion des doublons en lot

## Prérequis

Ceux du [README](../../README.md) (Build Tools 2026, Qt 6.12, `QT_DIR`), dans
un « Developer PowerShell for VS 2026 ».

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

Tests de la 006 :

| Test | Ce qu'il prouve |
|---|---|
| `tst_duplicates_core` | règles de dossier (préfixe, casse, frontière, plus précis) ; suggestion avec protection de la qualité ([research R1, R2](research.md)) |
| `tst_duplicates_batch` | actions en lot en une transaction, annulation partielle sûre, filtres (dossier, paire, type, taille, dates), totaux de sélection, paires et doublons internes, règles, « Écartés de la revue », migration 2 → 3 |
| `tst_trash` | la corbeille ne porte que sur le plan demandé (SC-004), copie gardée de référence = copie suggérée, arrêt d'une opération et annulation (SC-006) |

## Validation manuelle (à faire sur une copie de test, jamais sur ses vraies photos)

1. Créer `Images\2019` (50 photos), `Sauvegarde\2019` (leurs 50 copies), et
   5 copies internes dans `Images\2019`. Les surveiller ; attendre la fin de
   l'analyse.
2. Ouvrir « Doublons » → vue compacte ; ouvrir « Dossiers » : la paire
   `Images\2019 ↔ Sauvegarde\2019` (50 groupes) et le doublon interne
   apparaissent.
3. « Garder celles de Images\2019 » → message « Annuler » ; les 50 lignes
   montrent la sauvegarde « → corbeille ». « Annuler » rétablit, refaire.
4. Ctrl + A sur la paire filtrée → « Mettre à la corbeille… » : récapitulatif
   par dossier, 50 fichiers ; confirmer. `Sauvegarde\2019` est vide de ces
   copies, `Images\2019` intact (SC-001, à chronométrer).
5. Journal → « Annuler cette opération » : tout revient.
6. Refaire l'opération et cliquer « Arrêter » tout de suite : état
   « Arrêtée », entrées « Non traité » ; l'annulation restaure ce qui est
   parti.
7. Marquer 3 groupes « Garder toutes » en lot ; « Écartés » : le lot et les
   3 groupes y sont ; « Annuler cette action » : ils reviennent.

## Mesures (SC-002, SC-003)

Sur une bibliothèque de 10 000 groupes, hors compilation, machine au repos :
chronométrer « Tout sélectionner » (compte affiché), l'ouverture du
récapitulatif et le défilement de la vue compacte. `PICAZOO_LOG_FILE`
enregistre les durées journalisées par le contrôleur (`selectAll`,
`prepare`).

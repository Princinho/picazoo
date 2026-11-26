# Quickstart: valider la réduction de RAM au scan initial

## Prérequis

Ceux du [README](../../README.md) (Build Tools 2026, Qt 6.12, `QT_DIR`), dans
un « Developer PowerShell for VS 2026 ». Pour la validation manuelle sur
machine de référence : un ordinateur à 8 Go de RAM (ou une VM limitée à 8 Go),
le Gestionnaire des tâches ouvert sur l'onglet Détails, colonne « Mémoire ».

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

Tests de la 008 :

| Test | Ce qu'il prouve |
|---|---|
| `tst_memorybudget` (nouveau) | les 5 garanties de `computeMemoryBudget` ([contrat](contracts/core-api.md)) : jamais 0 thread, jamais plus que le plafond CPU, pas de bridage sur une machine généreuse, budget cohérent avec 1,2-1,5 Go sur la machine de référence, monotonie |
| `tst_decoding` (complété) | la miniature HEIC devient la vignette finale quand elle suffit (`QSKIP` si `tests/fixtures/sample.heic` absent) |
| `tst_thumbnailpipeline` (complété ou nouveau) | `applyMemoryBudget` réduit le parallélisme sans perte ni doublon d'élément, pas d'oscillation à la hausse en cours de scan |
| `tst_database` (complété si nécessaire) | `Database::open` avec un `cacheBytes` explicite applique bien ce `cache_size` |
| tests 001–007 | inchangés et verts (aucune migration de schéma, aucun changement de comportement fonctionnel visible) |

## Mesurer le pic de mémoire (SC-001, SC-002, SC-003, SC-006)

```powershell
.\build\windows-release\picazoo-bench.exe --items 100000
```

Lire la ligne « Pic de mémoire pendant le scan » (nouvelle, voir
[core-api.md](contracts/core-api.md#picazoo-bench--mesure-du-pic-de-mémoire-research-m1))
et la ligne « Analyse complète avec vignettes » déjà existante (temps, pour
SC-003). Comparer avant/après ce changement (`git stash` sur la branche pour
rejouer le même scénario avec le code d'avant, si une mesure de référence
n'a pas déjà été prise avant implémentation).

Pour une mesure proche d'une vraie bibliothèque mixte (JPEG + HEIC + RAW +
vidéos), utiliser les photos de test déjà prévues pour les doublons :

```powershell
.\build\windows-release\picazoo-testdata.exe --duplicates C:\PicazooTest\Doublons
```

### Mesures effectuées (T018, 2026-11-26)

Deux exécutions de `picazoo-bench.exe --items 100000` sur une machine de
développement partagée (pas la machine de référence à 8 Go dédiée à la
validation manuelle) :

| Exécution | RAM disponible (approx.) | Pic de mémoire | Découverte | Analyse complète (ramenée à 20 000) | Débit |
|---|---|---|---|---|---|
| 1 | ~4,3 Go | 142,68 Mo (seuil 1500) OK | 63,79 s (seuil 60) ÉCHEC | 18,82 min (seuil 15) ÉCHEC | 17,7 vignettes/s |
| 2 | ~1,6 Go | 159,89 Mo (seuil 1500) OK | 73,53 s (seuil 60) ÉCHEC | 25,61 min (seuil 15) ÉCHEC | 13,0 vignettes/s |

**Pic de mémoire (SC-001/SC-002)** : validé dans les deux cas, très large
marge sous le seuil de 1,5 Go — l'objectif principal de cette fonctionnalité
est confirmé.

**Seuils de temps (SC-003/SC-006)** : non concluants sur cette machine. Les
deux exécutions se sont déroulées avec d'autres applications ouvertes
(navigateur, éditeur, etc.), avec une RAM disponible mesurée à ~1,6-4,3 Go
seulement sur une machine à ~15 Go au total. `computeMemoryBudget` (B2)
réduit délibérément `thumbnailThreads` quand la RAM disponible est faible,
ce qui ralentit le débit de vignettes par rapport aux « 9 threads de calcul »
affichés en en-tête (figure CPU, pas le budget réellement appliqué) — la
seconde exécution, avec moins de RAM disponible, est d'ailleurs plus lente
que la première. Une comparaison avant/après fiable pour SC-003/SC-006
nécessite une machine dédiée et calme, comme prévu par la validation
manuelle (étapes 1-9 ci-dessous), pas cette machine de développement
partagée.

ou générer un dossier de test avec de vraies photos HEIC haute résolution
(iPhone) si disponibles, à défaut de quoi `picazoo-bench --items 100000`
(JPEG synthétique uniquement) donne déjà une mesure utile du budget de
threads et de caches, mais ne vérifie pas H1 (miniature HEIC).

## Validation manuelle (SC-001, SC-002, SC-004)

1. Sur une machine à 8 Go de RAM (ou VM limitée), ouvrir un navigateur et un
   traitement de texte, puis le Gestionnaire des tâches sur l'onglet
   Détails.
2. Pointer Picazoo (première fois) vers un dossier avec plusieurs dizaines
   de milliers de photos, dont une bonne proportion de HEIC haute résolution
   si possible.
3. Observer la colonne « Mémoire » de `picazoo.exe` pendant tout le scan :
   elle ne doit pas dépasser ~1,5 Go (SC-001, SC-002).
4. Pendant ce temps, utiliser le navigateur et le traitement de texte :
   aucun ralentissement perceptible, aucun avertissement Windows de mémoire
   insuffisante (SC-004).
5. Une fois le scan terminé, revérifier la colonne « Mémoire » dans la
   minute qui suit : elle doit être redescendue nettement sous le pic
   observé (SC-005).
6. Chronométrer le temps jusqu'à ce que toutes les vignettes soient visibles
   dans la grille ; comparer avec une mesure prise avant cette
   fonctionnalité sur la même bibliothèque et la même machine (écart ≤ 30 %,
   SC-003).

## Non-régression (US3)

7. Lancer un scan sur une grande bibliothèque de test, fermer Picazoo à
   mi-parcours (pas Ctrl+C brutal, fermeture normale), le relancer : le scan
   reprend et se termine, tous les éléments finissent avec une vignette,
   sans doublon ni élément manquant.
8. Ajouter un nouveau dossier surveillé après le premier scan : les mêmes
   limites de mémoire s'appliquent (observable au Gestionnaire des tâches).
9. Sur une machine disposant de nettement plus de 8 Go de RAM, relancer le
   même scénario `picazoo-bench --items 100000` et vérifier que le temps du
   scan n'est pas dégradé par rapport à la mesure d'avant cette
   fonctionnalité (SC-006).

## Piloter sans souris (pour Claude)

Cette fonctionnalité ne touche à aucun écran QML : pas de nouveau pilotage
nécessaire. Les mesures de RAM et de temps se vérifient par
`picazoo-bench` (sortie texte) et par les tests Qt Test
(`tst_memorybudget`, `tst_decoding`, `tst_thumbnailpipeline`). La validation
manuelle sur machine à 8 Go de RAM (étapes 1 à 6) reste à faire par
l'utilisateur, comme les mesures de ressenti des fonctionnalités précédentes.

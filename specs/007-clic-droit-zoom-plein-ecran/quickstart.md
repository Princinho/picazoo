# Quickstart: valider l'ergonomie des visionneuses et le clic droit

## Prérequis

Ceux du [README](../../README.md) (Build Tools 2026, Qt 6.12, `QT_DIR`), dans
un « Developer PowerShell for VS 2026 ». Pour la corbeille et le fond
d'écran : un dossier de test (jamais ses vraies photos) sur un disque fixe
avec corbeille, plus si possible une clé USB (disque sans corbeille).

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

Tests de la 007 (les sorties Qt Test se lisent avec `-o fichier,txt`) :

| Test | Ce qu'il prouve |
|---|---|
| `tst_zoommotion` (nouveau) | les 10 garanties de `ZoomMotion` ([contrat](contracts/core-api.md)) : butée à 100 % sous 90 %, pas d'accumulation, pas de butée ≥ 90 % ni en arrière, durée d'un pas, arrêt de l'élan < 600 ms, rapide > lent, interruption, ancrage, mode immédiat |
| `tst_zoommath` | inchangé et vert (l'ancrage n'a pas changé) |
| `tst_trash` (complété) | `planItems` sans copie gardée ; fichier modifié laissé en place ; source `library` au journal ; annulation ; reprise après arrêt brutal qui ignore une opération de moins de 60 s |
| `tst_filetrash` (nouveau) | `FileTrash` sur base neuve (migration 0 → 4) et sur base existante pendant qu'une autre connexion écrit (WAL) ; corbeille réelle sur dossier temporaire ; restauration |
| `tst_database` (complété) | migration 3 → 4 : `source = 'duplicates'` pour l'existant |
| tests 002 / 006 | inchangés et verts |

## Validation manuelle

### Plein écran (US1)

1. Ouvrir une photo dans la visionneuse intégrée, F11. Ne rien toucher
   2 s : rien d'autre que la photo à l'écran.
2. Bouger la souris : le bouton « Quitter le plein écran » apparaît en haut
   à droite, sur une photo claire puis sur une photo sombre.
3. Le survoler 5 s : il reste. S'en écarter et attendre : il s'efface.
4. Zoomer à 200 %, déplacer l'image, cliquer le bouton : fenêtre rendue,
   même photo, même zoom, même position (SC-001, chronométrer).
5. Refaire depuis une fenêtre agrandie : elle redevient agrandie.

### Butée et douceur du zoom (US2, US4)

6. Dans chaque visionneuse, photo de 6 000 px ajustée (≈ 30 %) : tourner la
   molette vite et longtemps → l'indicateur s'arrête sur « 100 % » ; 20 fois
   (SC-002).
7. Relâcher, attendre, un cran : le zoom dépasse 100 % ; 20 fois depuis 90 %
   ou plus (SC-003).
8. Depuis 80 %, maintenir ↑ : arrêt à 100 % ; relâcher, ↑ : 125 %.
9. Un cran isolé : la photo glisse vers sa taille en ≈ 0,3 s. Un tour rapide
   puis lâcher : elle continue un court instant en ralentissant. Comparer côte
   à côte avec la Visionneuse de photos Picasa sur les mêmes photos (SC-008).
10. Pendant un élan, cliquer ou faire glisser : arrêt immédiat. Lâcher un
    glisser en mouvement : l'image s'arrête net.
11. Désactiver « Afficher les animations dans Windows » : le zoom est
    immédiat, sans élan.

### Flèches (US3)

12. Dans chaque visionneuse, inspecter 20 photos au clavier seul : ↑ ↑ ↓ →
    (SC-007). Sur une vidéo, ↑ / ↓ ne font rien.

### Menus contextuels (US5)

13. Clic droit sur : une vignette, une sélection de 3, la zone vide, un
    dossier, une ligne d'année, l'image de chaque visionneuse, une copie et
    un groupe de doublons. Vérifier les entrées du
    [contrat d'interface](contracts/ui-contract.md) et qu'aucun clic droit
    ne zoome ni ne déplace l'image.
14. Touche Menu et Maj+F10 sur la vignette courante et dans chaque
    visionneuse. Échap ferme le menu sans fermer la visionneuse.
15. Copier une photo, coller dans WordPad (image) puis dans un dossier de
    l'Explorateur (le fichier est **copié**, l'original reste en place).
    Copier le chemin de 3 éléments, coller dans le Bloc-notes : 3 lignes.
16. Sur le dossier de test : « Mettre à la corbeille… » sur une vignette,
    puis Entrée → rien n'est fait (« Annuler » par défaut). Recommencer et
    confirmer : le fichier est dans la corbeille de Windows, au journal
    « depuis la bibliothèque », et « Annuler » le restaure (il revient dans
    la grille).
17. Même chose depuis la visionneuse autonome ouverte depuis l'Explorateur,
    Picazoo fermé : la visionneuse passe à la photo suivante ; ouvrir
    Picazoo → le journal montre l'opération « depuis la visionneuse » et
    permet de l'annuler.
18. Sur la clé USB : « Mettre à la corbeille… » est grisé avec son
    info-bulle.
19. « Définir comme fond d'écran » sur un JPEG : le bureau change ;
    « Annuler » rétablit l'ancien fond, sur chaque écran. Sur un RAW ou un
    HEIC : entrée grisée.
20. « Propriétés » sur une vignette puis sur une sélection de 3 : la fenêtre
    Propriétés de Windows s'ouvre.

### Performance (SC-004)

21. Sélectionner 1 000 vignettes (Ctrl+A dans un grand dossier), clic droit :
    le menu s'ouvre sans délai perceptible (< 0,1 s). Pendant ce temps,
    aucune compilation ni mesure en parallèle (machine sensible à la
    chauffe).

## Piloter les écrans sans souris (pour Claude)

La fenêtre n'est jamais exposée quand Claude lance l'application : les
`FrameAnimation` et `Timer` QML n'y tournent pas. La logique de zoom se
valide donc par `tst_zoommotion` ; les menus et le bouton de plein écran se
vérifient par capture (`PICAZOO_SCREENSHOT`, `PICAZOO_DEV_SCREEN`,
`PICAZOO_VIEWER_SCRIPT`) et les erreurs QML par `PICAZOO_LOG_FILE`. Le
ressenti du zoom (étapes 9 à 11) reste à valider par l'utilisateur.

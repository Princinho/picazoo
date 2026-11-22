# Research: Bibliothèque et visionneuse

Fonctionnalité 001 — décisions techniques. Le critère dominant est la
**vitesse** (demande explicite du propriétaire, 2026-11-22), sous la
contrainte absolue du principe I (ne jamais toucher aux fichiers).

Aucun `NEEDS CLARIFICATION` ne reste ouvert. Les points marqués
**[à valider au spike]** sont des hypothèses fortes à confirmer par la
première tâche technique (squelette + mesures), pas des inconnues de
conception.

---

## R1 — Langage et cadre d'interface

- **Decision**: C++20, Qt 6.12 LTS, interface en Qt Quick (QML) rendue par la
  carte graphique ; le cœur (indexation, décodage, base) est en C++ pur
  autour de QtCore.
- **Rationale**: les bibliothèques les plus rapides du domaine (libjpeg-turbo,
  LibRaw, libheif, SQLite, FFmpeg) sont en C/C++ : on les appelle sans
  couche de liaison ni ramasse-miettes. Qt Quick dessine la grille sur le
  GPU et ne crée que les vignettes visibles. Qt Multimedia lit MKV, AVI, MOV
  et MP4 via FFmpeg, qui est son moteur par défaut sous Windows depuis la
  6.5. Qt 6.12 est la LTS de septembre 2026 (5 ans de maintenance). Qt est
  multiplateforme, ce qui laisse ouverte la porte macOS/Linux voulue par la
  constitution.
- **Alternatives considered**:
  - Rust + Slint : vitesse équivalente, plus sûr en mémoire ; rejeté parce
    que la lecture vidéo intégrée serait à construire et que l'écosystème
    d'interface est moins mûr.
  - .NET 10 + Avalonia : plus confortable pour le propriétaire ; rejeté parce
    que la vitesse prime (ramasse-miettes, grille virtualisée et vidéo
    intégrée moins abouties).
  - Rust + Tauri (web) : WebView2 ne lit pas l'AVI et lit mal le MKV, donc
    transcodage nécessaire ; et chaque image passe par une couche de plus.
  - Qt Widgets (QListView en mode icônes) : rapide aussi, mais pas
    d'en-têtes de section natifs et un rendu moins fluide que Qt Quick pour
    les animations de taille de vignettes.

## R2 — Chaîne de compilation et dépendances

- **Decision**: CMake + Ninja + MSVC (Visual Studio Build Tools 2026, déjà
  installés avec CMake, Ninja et vcpkg). Qt 6.12 LTS en binaires officiels
  (Qt Online Installer, ou `aqtinstall` si on veut éviter le compte Qt),
  composants `qtbase`, `qtdeclarative`, `qtmultimedia`, `qtimageformats`,
  `qtshadertools`. Les autres dépendances passent par un manifeste
  `vcpkg.json` (triplet `x64-windows`, liaison dynamique).
- **Rationale**: toute la chaîne MSVC est déjà sur la machine ; vcpkg fige les
  versions dans le dépôt ; les binaires Qt officiels embarquent déjà leur
  FFmpeg pour Qt Multimedia, ce qui évite de recompiler Qt (plusieurs
  heures). Prendre le dernier correctif 6.12.x (avis de sécurité Qt
  CVE-2026-8461 sur le FFmpeg embarqué).
- **Alternatives considered**: Qt compilé par vcpkg (rejeté : compilation
  très longue, et deux FFmpeg à faire cohabiter) ; MinGW (rejeté : MSVC est
  la cible de référence des binaires Qt et de vcpkg sous Windows).

| Dépendance | Rôle | Licence |
|---|---|---|
| Qt 6.12 (Core, Quick, Multimedia, ImageFormats) | interface, vidéo, PNG/GIF/BMP/TIFF/WebP | LGPLv3 (liaison dynamique) |
| libjpeg-turbo (API TurboJPEG 3) | décodage JPEG réduit, encodage des vignettes | BSD/IJG |
| libheif + libde265 | HEIC (et sa vignette intégrée) | LGPLv3 |
| LibRaw | aperçu intégré des RAW, métadonnées RAW | LGPL 2.1 / CDDL |
| SQLite 3 | index et cache de vignettes | domaine public |
| TinyEXIF | lecture EXIF (date, orientation, appareil, vignette) | BSD-2 |
| xxHash (XXH3) | empreinte rapide des fichiers | BSD-2 |

Toutes les licences sont compatibles avec n'importe quelle licence pour
Picazoo, à condition de lier dynamiquement les bibliothèques LGPL.
**[à valider au spike]** : disponibilité de `tinyexif` dans vcpkg ; sinon,
TinyEXIF est inclus dans `third_party/` (2 fichiers).

## R3 — Parcours des dossiers (découverte)

- **Decision**: parcours Win32 `FindFirstFileExW` avec `FindExInfoBasic` et
  `FIND_FIRST_EX_LARGE_FETCH`, en largeur, sur un thread d'E/S dédié. Chaque
  entrée fournit déjà taille, date de modification et attributs, sans
  appel supplémentaire. On ignore les fichiers et dossiers cachés ou
  système, `Thumbs.db`, `desktop.ini` et les points de réanalyse (raccourcis,
  jonctions, liens symboliques : `FILE_ATTRIBUTE_REPARSE_POINT`). Un
  dossier surveillé contenu dans un autre n'est parcouru qu'une fois
  (racines normalisées puis dédupliquées). Le filtre de type se fait sur
  l'extension (liste FR-003/FR-004), sans ouvrir le fichier.
- **Rationale**: c'est le parcours le plus rapide disponible sans droits
  administrateur (de l'ordre de 100 000 entrées en quelques secondes sur
  SSD). Les dossiers sont publiés vers l'interface dès qu'ils sont
  terminés : premiers dossiers visibles en moins de 5 s (SC-001).
- **Alternatives considered**: `std::filesystem` (rejeté : un appel système
  de plus par fichier pour les attributs) ; journal USN de NTFS (rejeté :
  exige des droits administrateur) ; Windows Search (rejeté : dépend d'un
  service tiers, couverture incomplète).

## R4 — Identité d'un fichier et rattrapage au démarrage

- **Decision**: un élément est identifié par `(numéro de série du volume,
  identifiant de fichier NTFS)` obtenu par `GetFileInformationByHandle`, en
  ouvrant le fichier avec `FILE_READ_ATTRIBUTES` seulement (aucun accès au
  contenu, aucun verrou). On garde aussi une **empreinte rapide** : XXH3 de
  (taille + 64 Ko du début + 64 Ko de la fin). Au démarrage, on refait le
  parcours (R3) et on compare `(chemin, taille, date de modification)` :
  - même chemin, même taille et même date → rien à faire ;
  - chemin nouveau dont l'identifiant NTFS (ou à défaut l'empreinte)
    correspond à un élément disparu → **renommage/déplacement**, l'élément
    est conservé avec ses données (FR-008) ;
  - taille ou date changée → l'élément est rafraîchi (nouvelle vignette) ;
  - chemin disparu sans correspondance → suppression de l'index.
- **Rationale**: l'identifiant NTFS survit aux renommages et déplacements sur
  un même volume ; l'empreinte couvre les déplacements entre volumes et les
  systèmes FAT/exFAT (clés USB, cartes SD) qui n'ont pas d'identifiant
  stable. Lire 128 Ko par fichier reste négligeable par rapport au
  décodage. L'empreinte sert aussi de premier filtre aux doublons exacts
  pour la 002.
- **Alternatives considered**: hachage complet du fichier (rejeté : lit toute
  la bibliothèque, des dizaines de Go) ; chemin seul (rejeté : un renommage
  ferait perdre les données calculées).

## R5 — Suivi des changements en direct

- **Decision**: un `ReadDirectoryChangesW` récursif (E/S asynchrones) par
  racine surveillée, sur un thread dédié. Les événements sont regroupés
  pendant 500 ms, puis traités comme un mini-rattrapage (R4) limité aux
  dossiers touchés. Un fichier en cours de copie (taille qui change, ou
  ouverture en lecture partagée refusée) est réessayé jusqu'à stabilité
  (edge case). En cas de débordement du tampon, on refait un parcours de la
  racine concernée. Une racine qui devient inaccessible passe à l'état
  « indisponible » (FR-009) et un nouvel essai a lieu toutes les 30 s, ainsi
  qu'à la réception de `WM_DEVICECHANGE`.
- **Rationale**: c'est l'API native, récursive, qui ne coûte rien au repos ;
  `QFileSystemWatcher` n'est pas récursif et consomme un handle par dossier.
  Un délai de 500 ms + le traitement tient largement dans les 10 s (SC-005).
- **Alternatives considered**: `QFileSystemWatcher` (rejeté, voir ci-dessus) ;
  interrogation périodique (rejetée : coûteuse et lente à réagir).

## R6 — Vignettes : génération, stockage, affichage

- **Decision**:
  1. **Affichage immédiat** : dès la découverte, si le fichier contient une
     vignette intégrée (EXIF des JPEG, vignette HEIC, aperçu RAW), on
     l'extrait — quelques Ko lus — et elle sert d'image provisoire.
  2. **Vignette définitive** : 320 px sur le grand côté, orientation EXIF
     appliquée, encodée en JPEG qualité 85 par TurboJPEG, stockée en BLOB
     dans `thumbs.db` (SQLite à part de l'index), clé `(item_id,
     version)`. 320 px couvre le curseur de taille jusqu'à ~300 px à
     l'écran ; au-delà, on affiche l'aperçu de la visionneuse (R8).
  3. **Mémoire** : un cache LRU de vignettes décodées (budget 256 Mo, environ
     5 000 vignettes) devant `thumbs.db` ; les textures GPU sont gérées par
     Qt Quick.
  4. **Priorités** : file à trois niveaux — visibles à l'écran, puis
     ±2 écrans autour, puis le reste de la bibliothèque. Faire défiler
     réordonne la file ; une tâche qui n'est plus visible est rétrogradée,
     pas annulée.
- **Rationale**: SQLite lit et écrit les petits BLOB (< 100 Ko) plus vite que
  des milliers de petits fichiers sur NTFS, et l'ensemble reste un seul
  fichier facile à effacer et à reconstruire. Séparer `thumbs.db` de
  `library.db` évite que les écritures de vignettes bloquent les lectures
  d'index. La vignette provisoire donne une grille pleine en quelques
  secondes, même pendant une analyse de 20 000 photos.
- **Alternatives considered**: cache de miniatures de Windows
  (`IShellItemImageFactory`) (rejeté : lent pour les RAW/HEIC sans
  extensions, résultat variable selon la machine, réservé à Windows) ; un
  fichier par vignette (rejeté : lent et 100 000 fichiers à gérer) ; WebP
  pour les vignettes (rejeté : encodage plus lent que TurboJPEG pour un gain
  de place sans intérêt ici).

## R7 — Décodage par format

- **Decision**:
  - **JPEG** : TurboJPEG 3 avec décodage réduit dans le DCT (facteurs 1/2,
    1/4, 1/8). Une photo de 24 Mpx décodée à 1/8 ne coûte qu'une fraction du
    décodage complet.
  - **HEIC** : libheif ; on prend la vignette intégrée pour la grille, et
    l'image principale décodée pour la visionneuse.
  - **RAW** : LibRaw `open_file` + `unpack_thumb` : on récupère l'aperçu
    intégré (en général un JPEG pleine taille ou presque), décodé ensuite
    par TurboJPEG. Pas de `dcraw_process` (pas de développement du RAW,
    conformément à la clarification). Sans aperçu exploitable → vignette
    « aperçu indisponible » (FR-004).
  - **PNG, GIF, BMP, TIFF, WebP** : `QImageReader` avec `setScaledSize`
    (réduction native pour les formats qui la gèrent). Pour un GIF animé,
    seule la première image en vignette ; l'animation se joue dans la
    visionneuse.
  - **Métadonnées** : TinyEXIF sur le segment EXIF (JPEG, et bloc EXIF de
    HEIC fourni par libheif) ; LibRaw pour les RAW. Date retenue : date de
    prise de vue EXIF, sinon date de modification du fichier.
- **Rationale**: chaque format passe par le décodeur le plus rapide disponible
  et on ne lit jamais plus de pixels que nécessaire.
- **Alternatives considered**: tout via `QImageReader` (rejeté : pas de HEIC
  sous Windows, pas de RAW, décodage JPEG moins rapide que TurboJPEG en
  accès direct) ; ImageMagick/Magick.NET (rejeté : lourd et lent pour ce
  besoin).

## R8 — Visionneuse : ouverture en moins d'une seconde, suivante en moins d'une demi-seconde

- **Decision**: à l'ouverture, on décode d'abord à la **taille de l'écran**
  (facteur de réduction TurboJPEG le plus proche par excès), affiché tout de
  suite ; l'image pleine résolution n'est décodée qu'au premier zoom
  au-delà de 100 % de cet aperçu. On **précharge** les éléments ±1 et ±2 à
  la taille de l'écran (cache de 5 images). Pour une image de plus de
  50 Mpx, le zoom utilise un décodage par tuiles (régions TurboJPEG/libheif)
  au lieu d'une texture géante. La vignette de 320 px s'affiche
  instantanément en attendant l'aperçu (pas d'écran noir).
- **Rationale**: une photo de 24 Mpx décodée à 1/2 ou 1/4 pour un écran
  1080p ou 1440p se décode en quelques dizaines de ms ; le préchargement
  rend le passage à la suivante quasi instantané (SC-004).
- **Alternatives considered**: tout décoder en pleine résolution (rejeté :
  plusieurs centaines de ms et ~100 Mo de mémoire par image).

## R9 — Grille continue avec en-têtes de dossiers (100 000 éléments)

- **Decision**: un `ListView` QML dont les lignes sont calculées par un
  modèle C++ « aplati » (`GridRowModel`) : chaque ligne est soit un
  en-tête de dossier, soit une rangée de N vignettes d'un même dossier,
  N dépendant de la largeur et de la taille des vignettes. Hauteurs de ligne
  connues à l'avance → défilement et saut vers un dossier en O(log n). Les
  vignettes sont servies par un `QQuickAsyncImageProvider`
  (`image://thumb/<id>/<version>`) qui lit le cache LRU puis `thumbs.db`.
  La liste des dossiers est un second modèle (`FolderTreeModel`) avec ses
  deux modes, par année et arborescence (FR-011).
- **Rationale**: `GridView` n'a pas d'en-têtes de section ; le modèle aplati
  garde la virtualisation complète (seules les lignes visibles existent) et
  redimensionner la grille ne fait que recalculer un découpage en mémoire.
- **Alternatives considered**: `GridView` avec un dossier par délégué
  (rejeté : un dossier de 5 000 photos deviendrait un seul élément géant,
  non virtualisé) ; `TableView` (rejeté : pas pensé pour des lignes
  hétérogènes).

## R10 — Base de données locale

- **Decision**: SQLite en mode WAL, `synchronous=NORMAL`, accès par l'API C
  avec requêtes préparées. Deux fichiers sous `%LOCALAPPDATA%\Picazoo\` :
  `library.db` (index, voir data-model.md) et `thumbs.db` (vignettes). Une
  connexion d'écriture unique (thread de l'indexeur), qui insère par lots de
  1 000 dans une transaction ; des connexions de lecture pour l'interface.
  Le schéma porte un numéro de version et des migrations numérotées. Les
  préférences (taille des vignettes, tris, mode de la liste) sont dans la
  table `settings` de `library.db`.
- **Rationale**: SQLite est la base locale la plus rapide et la plus fiable
  ici ; le WAL permet de lire pendant qu'on écrit ; les lots rendent
  l'insertion de 100 000 lignes quasi instantanée. Rien n'est écrit dans les
  dossiers de l'utilisateur (principe I, FR-010).
- **Alternatives considered**: `QtSql` (rejeté : couche de plus sans gain) ;
  base embarquée clé-valeur type LMDB (rejetée : on a besoin de requêtes et
  de tris).

## R11 — Vidéo

- **Decision**: lecture par Qt Multimedia (`MediaPlayer` + `VideoOutput` en
  QML, moteur FFmpeg). Vignette et durée : en arrière-plan, un
  `QMediaPlayer` sans affichage, branché sur un `QVideoSink`, se positionne
  à 10 % de la durée et capture une image, puis lit la durée dans les
  métadonnées. Format non lisible → message clair et ouverture avec
  l'application par défaut (`QDesktopServices::openUrl`, FR-022).
- **Rationale**: un seul FFmpeg (celui de Qt), pas de conflit de DLL ; les
  vidéos sont une petite partie des bibliothèques, donc un coût de quelques
  centaines de ms par vidéo à l'indexation est acceptable.
  **[à valider au spike]** : coût réel par vidéo ; si plus de 500 ms, on
  liera directement libavformat/libavcodec à la même version que le
  FFmpeg livré par Qt.
- **Alternatives considered**: libVLC (rejeté : un second moteur vidéo
  lourd) ; FFmpeg de vcpkg en plus de celui de Qt (rejeté pour l'instant :
  risque de conflit de DLL).

## R12 — Parallélisme et réactivité

- **Decision**: trois rôles séparés : (1) un thread d'E/S pour la découverte
  et la surveillance ; (2) un pool de calcul (`QThreadPool`, nombre de
  cœurs moins 1) pour le décodage et l'encodage des vignettes ; (3) le
  thread de l'interface, qui ne fait jamais d'E/S disque ni de décodage. Le
  pool est mis en pause pendant l'interaction dans la visionneuse pour que
  le décodage de l'image affichée passe en premier. Pause et reprise de
  l'analyse (FR-005) : l'état est en base (éléments sans vignette
  `thumb_state = pending`), donc reprendre après une fermeture ne demande
  rien de plus.
- **Rationale**: c'est la condition de SC-003 (défilement fluide pendant
  l'analyse) et de SC-004.
- **Alternatives considered**: `QtConcurrent::map` sur tout (rejeté : pas de
  priorités ni de réordonnancement selon le défilement).

## R13 — Tests et mesures

- **Decision**: Qt Test + CTest. Trois familles :
  - **unitaires** (cœur : identité, empreinte, découpage en lignes, EXIF,
    choix du facteur de décodage) ;
  - **intégration** sur des dossiers de test générés dans un répertoire
    temporaire (découverte, rattrapage, renommage, disque indisponible) ;
    chaque test d'intégration vérifie **l'invariant SC-006** : instantané
    (chemins, tailles, dates, hachages) avant et après, qui doivent être
    identiques ;
  - **performances** : un exécutable `picazoo-bench` qui génère 20 000 et
    100 000 éléments et mesure SC-001, SC-002 et SC-004.

  Des images de test réelles (RAW, HEIC, vidéo, JPEG pivoté) sont rangées
  dans `tests/fixtures/`, libres de droits et de petite taille.
- **Rationale**: le principe I impose des tests automatisés des opérations
  sur les fichiers ; la vitesse est une exigence, donc mesurée et non
  supposée.
- **Alternatives considered**: GoogleTest ou Catch2 (rejetés : une dépendance
  de plus, alors que Qt Test sait aussi tester les modèles et le QML).

---

## Mesures (2026-11-22)

Machine : portable, Intel Core Ultra 5 225U (processeur basse consommation hybride :
12 cœurs, 14 threads), SSD, Windows 11, version Release. Banc d'essai :
`bench/picazoo-bench.cpp`. Les photos de test sont générées (dégradé + bruit fort, ce qui
représente un pire cas pour le décodage JPEG) puis multipliées par liens physiques.

| Critère | Mesure | Seuil | Résultat |
|---|---|---|---|
| SC-001 premiers dossiers visibles | 0,25 s | < 5 s | ✅ |
| SC-001 analyse complète, 20 000 photos de 12 Mpx | 7,96 min (41,9 vignettes/s) | < 15 min | ✅ |
| SC-001 analyse complète, 20 000 photos de 24 Mpx bruitées | 15,26 min (21,8 vignettes/s) | < 15 min | ⚠️ +2 % |
| SC-002 ouverture d'une bibliothèque de 100 000 éléments | 1,06 s | < 3 s | ✅ |
| SC-004 ouverture d'une photo de 24 Mpx (aperçu net) | 0,43 s | < 1 s | ✅ |
| SC-004 suivante, même sans préchargement | 0,28 s | < 0,5 s | ✅ |
| SC-005 photo copiée visible dans la bibliothèque | 1,84 s (0,6 s dans `tst_watcher`) | < 10 s | ✅ |
| Découverte de 20 000 / 100 000 éléments (indicatif, pas un critère de la spec) | 18 s / 199 s | — | — |

Constats :

- **Le décodage JPEG domine** : 125 à 330 ms par photo de 24 Mpx décodée à 1/8 selon
  l'état thermique (le décodage de Huffman ne peut pas être réduit) ; tout le reste
  (métadonnées, empreinte, lecture, encodage) coûte moins de 20 ms.
- **Plus de threads n'est pas plus rapide sur ce processeur** : 23 vignettes/s à 8 threads,
  18 à 12 (cœurs basse consommation, limite thermique). D'où
  `platform::recommendedWorkerThreads()` (9 ici au lieu de 13), qui a aussi rendu la
  découverte presque trois fois plus rapide (20 000 éléments en 18 s au lieu de 50 s), car
  elle n'est plus étouffée par le calcul des vignettes.
- **R11 (vidéo)** et **R9 (fluidité du défilement en images/s)** : non mesurés (voir les
  notes de tasks.md).
- **Pistes si l'on veut plus de marge sur SC-001** : utiliser l'aperçu MPF que beaucoup
  d'appareils enregistrent dans leurs JPEG (souvent 1 920 px) au lieu de décoder l'image
  entière ; accélérer la découverte des très grandes bibliothèques (199 s pour 100 000
  éléments, en concurrence avec les écritures des vignettes dans SQLite).

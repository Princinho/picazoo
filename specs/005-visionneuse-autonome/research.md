# Recherche — Visionneuse autonome (005)

Décisions techniques de la phase 0. Chacune : décision, raison, alternatives.

## V1. Un exécutable séparé, `picazoo-viewer.exe`

- **Décision** : nouvelle cible `picazoo-viewer` (Qt Quick), installée dans
  `bin\` à côté de `picazoo.exe`. Elle lie `picazoo_core` (bibliothèque
  statique) mais n'utilise que `media/`, `browse/` et `platform/win/` : le
  lieur n'embarque ni `Library`, ni l'index, ni SQLite, ni Qt Multimedia.
- **Raison** : démarrage plus court (moins de DLL, pas de base à ouvrir) et
  garantie structurelle de FR-002 et FR-035 : la visionneuse ne peut pas
  toucher la bibliothèque, elle n'en a pas le code. Une association de
  fichiers pointe vers un exécutable précis, sans option à interpréter.
- **Alternatives** : `picazoo.exe --viewer` (un seul binaire, mais charge
  Qt Multimedia, et le moindre écart dans `main` rouvrirait la bibliothèque) ;
  réutiliser `Viewer.qml` tel quel (lié à `ViewerController` et aux
  identifiants de la bibliothèque).

## V2. Démarrage en moins d'une seconde

- **Décision** :
  1. l'instance unique (V4) est réglée avant toute création d'interface : une
     seconde instance transmet son chemin et quitte en quelques millisecondes ;
  2. le décodage de la photo demandée part sur le pool d'images dès que le
     chemin est connu, en parallèle du chargement du QML ;
  3. le QML est compilé à la construction (`qt_add_qml_module`, qmlcachegen)
     et `QML_DISABLE_DISK_CACHE=1` empêche toute écriture de cache ;
  4. le voile s'affiche en fondu (150 ms) dès la première image du moteur ; la
     photo apparaît dès qu'elle est décodée (aperçu à la taille de l'écran,
     décodage JPEG réduit d'un facteur 1/2, 1/4 ou 1/8) ;
  5. listage du dossier, ordre de l'Explorateur, vignettes de la bande et
     proposition d'association ne commencent qu'après le premier affichage.
- **Raison** : sur l'ordinateur de référence, une photo de 24 Mpx se décode
  à la taille de l'écran en ~80 ms (mesures de la 001) ; le chargement de Qt
  Quick et de Direct3D domine. Les mener en parallèle tient la seconde.
- **Alternatives** : processus résident en arrière-plan (ouverture
  instantanée mais mémoire occupée en permanence, contraire à « légère »).

## V3. Le voile semi-transparent

- **Décision** : une `Window` Qt Quick sans bordure
  (`Qt.FramelessWindowHint`), affichée en plein écran sur l'écran de la
  souris, de couleur transparente, avec un tampon alpha
  (`QQuickWindow::setDefaultAlphaBuffer(true)`). Le voile est un rectangle
  noir à 78 % d'opacité (100 % en mode opaque), animé. Le moteur Direct3D 11
  de Qt compose les fenêtres translucides via DirectComposition. Une fenêtre
  qui couvre exactement l'écran est traitée par Windows comme plein écran : la
  barre des tâches s'efface tant qu'elle est active, mais la fenêtre reste dans
  la barre des tâches et Alt+Tab (pas de `Qt.Tool`).
  La préférence de Windows « Effets de transparence »
  (`HKCU\…\Themes\Personalize\EnableTransparency`) force le mode opaque.
- **Raison** : c'est l'aspect de la Visionneuse de photos Picasa choisi par
  l'utilisateur ; Qt le permet sans code natif de composition.
- **Alternatives** : capture d'écran du bureau floutée en fond (fige le
  bureau et lit l'écran : refusé) ; `WS_EX_LAYERED` avec OpenGL (plan B si
  DirectComposition échoue sur une machine).

## V4. Une seule visionneuse à la fois

- **Décision** : mutex nommé `Local\Picazoo.Viewer` pour élire l'instance
  principale, puis `QLocalServer` « `Picazoo.Viewer.<session>` ». Une
  instance secondaire appelle `AllowSetForegroundWindow(ASFW_ANY)`, envoie le
  chemin en UTF-8 et quitte ; elle réessaie la connexion pendant 2 s si
  l'instance principale est en train de démarrer. L'instance principale
  passe à la nouvelle photo et se met au premier plan. Si plusieurs chemins
  arrivent en moins de 500 ms (sélection multiple ouverte dans
  l'Explorateur), seul le premier est retenu.
- **Raison** : sur Windows, `QLocalServer::listen` ne détecte pas toujours
  un nom déjà pris ; le mutex rend l'élection sûre. L'Explorateur lance un
  processus par fichier sélectionné : le regroupement évite d'en ouvrir dix.
- **Alternatives** : `FindWindow` + `WM_COPYDATA` (dépend du titre de la
  fenêtre) ; DDE (obsolète).

## V5. L'ordre de la fenêtre de l'Explorateur

- **Décision** : au lancement, la fenêtre au premier plan est notée
  (`GetForegroundWindow`) avant toute création de fenêtre. Sur un thread
  COM STA d'arrière-plan : `IShellWindows` énumère les fenêtres de
  l'Explorateur (un élément par onglet sous Windows 11) ; pour chacune,
  `IServiceProvider` → `SID_STopLevelBrowser` → `IShellBrowser` →
  `QueryActiveShellView` → `IFolderView2`. Le dossier affiché
  (`IPersistFolder2::GetCurFolder`) doit être celui de la photo ; s'il y en a
  plusieurs, on préfère celle dont la fenêtre était au premier plan et dont
  la vue est visible (onglet actif). Le bureau (`FindWindowSW` avec
  `SWC_DESKTOP`) est traité comme une fenêtre de plus. Les éléments sont lus
  dans l'ordre d'affichage : `IFolderView::Items(SVGIO_ALLVIEW |
  SVGIO_FLAG_VIEWORDER)`, nom d'analyse de chaque élément. L'ordre obtenu est
  fusionné avec le listage du disque (V6) : les images de la vue dans leur
  ordre, puis celles que la vue ne montre pas (filtre), dans l'ordre naturel.
- **Raison** : lire l'ordre d'affichage plutôt que la colonne de tri reproduit
  exactement ce que voit l'utilisateur, regroupements compris, sans
  recalculer des propriétés (date de prise de vue, dimensions…) pour chaque
  fichier. La fusion (pure, testable) protège des écarts entre la vue et le
  disque.
- **Alternatives** : `IFolderView2::GetSortColumns` puis tri local (rapide
  pour nom, date, taille, mais il faudrait lire chaque propriété du Shell pour
  les autres colonnes, et les regroupements seraient perdus) ; tri par nom
  seul (refusé par l'utilisateur).
- **Repli** : ordre naturel des noms (`StrCmpLogicalW`, la comparaison de
  l'Explorateur) quand aucune fenêtre ne correspond, quand la lecture échoue
  ou dépasse 2 s ; le parcours est utilisable dès le listage et se réordonne
  une fois, sans bouger la photo affichée, quand l'ordre de l'Explorateur
  arrive.

## V6. Lister le dossier

- **Décision** : `FindFirstFileExW` (`FindExInfoBasic`,
  `FIND_FIRST_EX_LARGE_FETCH`) sur le seul dossier, sans sous-dossiers ;
  fichiers `HIDDEN` et `SYSTEM` écartés, liens symboliques non suivis ;
  formats d'images de `media/Formats` (les vidéos et les formats inconnus
  sont écartés). Taille et date de modification sont lues au passage, pour le
  panneau d'informations.
- **Raison** : ~5 ms pour 5 000 fichiers sur SSD ; aucun fichier n'est ouvert.

## V7. Suivre les changements du dossier

- **Décision** : `QFileSystemWatcher` sur le dossier (notification Win32
  non récursive), regroupement des événements sur 300 ms, nouveau listage,
  puis mise à jour du parcours : les fichiers restants gardent leur ordre, les
  nouveaux sont insérés à leur place dans l'ordre naturel si le parcours suit
  l'ordre naturel, sinon ajoutés à la fin ; si la photo affichée a disparu, un
  message s'affiche et la suivante est montrée après 1,5 s.
- **Raison** : simple, suffisant pour un seul dossier, et sans effet sur le
  disque.

## V8. Décodage, cache et mémoire

- **Décision** : fournisseur `image://file/<génération>/<screen|full|thumb>/
  <chemin encodé>` sur un pool de 2 threads, via `Decoder::decode` de la
  001 (orientation lue par `Decoder::readMetadata`). Cache LRU en mémoire de
  8 aperçus à la taille de l'écran (~8 Mo chacun en 1080p, ~33 Mo en 4K), une
  seule image pleine résolution (celle affichée, côté plafonné à 16 384 px),
  et 64 vignettes de 160 px pour la bande (vignette EXIF intégrée si
  présente). Préchargement des voisines ±2, et +3 dans le sens du parcours.
  Fichiers ouverts avec `FILE_SHARE_READ | FILE_SHARE_WRITE |
  FILE_SHARE_DELETE` et refermés après décodage.
- **Raison** : tient SC-009 (< 500 Mo, même en 4K) et SC-003 (la suivante est
  déjà décodée) ; aucun fichier n'est verrouillé.
- **Alternatives** : décodage par régions pour le zoom (gain mémoire sur les
  panoramas, plus tard si besoin).

## V9. Transitions animées

- **Décision** : quatre emplacements d'image QML recyclés. Chaque
  emplacement garde un chemin ; aller vers une photo réutilise l'emplacement
  qui la contient déjà (voisine préchargée, texture déjà envoyée à la carte
  graphique) ou en recycle un. La transition, 250 ms en `OutCubic`, combine
  fondu enchaîné et léger glissement (40 px dans le sens du parcours) ; si la
  photo n'est pas prête, la transition attend au plus 120 ms puis part avec
  l'aperçu disponible. Une nouvelle demande pendant une transition termine
  l'animation en cours instantanément et lance la suivante : pas de file
  d'animations. Animations coupées si
  `SystemParametersInfo(SPI_GETCLIENTAREAANIMATION)` est faux.
- **Raison** : changer la source d'un seul `Image` provoque toujours un
  écran vide le temps du chargement ; garder les voisines prêtes est la seule
  façon d'avoir une transition nette dès la première image.

## V10. Zoom ancré sur le curseur

- **Décision** : `ZoomMath` (C++, testé) porte le modèle : échelle `z`
  (1 = taille réelle), translation `t`, point écran `s = t + z·p`. Pour un
  zoom autour d'un point écran `a`, le point image `p = (a − t)/z` est figé,
  et chaque image de l'animation recalcule `t = a − z·p`. Un cran de molette
  (120 unités) multiplie l'échelle par 1,25 (proportionnellement pour les
  pavés tactiles), animation de 120 ms ; les crans successifs repartent de
  l'échelle en cours. Bornes : de l'ajustement à l'écran (ou 1 si l'image est
  plus petite) à 16. Déplacement par glisser, borné pour qu'un bord de l'image
  ne quitte pas le bord de l'écran ; image centrée quand elle est plus petite
  que l'écran. Double-clic : ajustement ⇄ 100 % au point cliqué. Pincement
  (`PinchHandler`) avec le même ancrage. Au-delà de la définition de
  l'aperçu, l'image pleine résolution est demandée et remplace l'aperçu
  quand elle est prête.
- **Raison** : le zoom de la 001 recalcule le décalage seulement à la fin et
  ne garde pas le point sous le curseur pendant l'animation (SC-006).

## V11. Inscription auprès de Windows, jamais imposée

- **Décision** : `picazoo-viewer.exe --register [--machine]` et
  `--unregister [--machine]` écrivent ou retirent, dans `HKCU` (ou `HKLM`
  pour une installation pour tous) : l'identifiant de programme
  `Picazoo.Image` (commande `"…\picazoo-viewer.exe" "%1"`, icône, nom),
  `Applications\picazoo-viewer.exe\SupportedTypes`, la valeur `Picazoo.Image`
  sous `.<ext>\OpenWithProgids` pour chaque extension, et les
  `Capabilities` déclarées dans `RegisteredApplications`. Aucune clé
  `UserChoice` n'est jamais écrite ; la valeur par défaut des extensions
  n'est jamais changée. `SHChangeNotify(SHCNE_ASSOCCHANGED)` après chaque
  modification. L'installateur Inno Setup appelle `--register` à
  l'installation et `--unregister` à la désinstallation. La proposition
  (FR-030) ouvre `ms-settings:defaultapps?registeredAppUser=Picazoo` (page
  des applications par défaut de Picazoo ; à défaut
  `ms-settings:defaultapps`), où l'utilisateur choisit lui-même, format par
  format. L'état actuel vient de
  `IApplicationAssociationRegistration::QueryCurrentDefault`.
- **Raison** : depuis Windows 10, une application ne peut plus se déclarer
  application par défaut (la clé `UserChoice` est protégée par un hachage) :
  le réglage de Windows est le seul chemin propre, et il garantit que rien
  n'est imposé. Le code d'inscription est le même pour l'installateur et
  pour le zip portable (`--register` lancé à la main ou depuis le menu de la
  visionneuse). Les tests le redirigent vers une clé temporaire.
- **Alternatives** : écrire les clés dans le script Inno Setup (deux sources
  de vérité, rien pour le zip) ; `SHOpenWithDialog` par extension (une boîte
  de dialogue par format).

## V12. Passer à l'application complète

- **Décision** : « Ouvrir dans Picazoo » lance `picazoo.exe --show
  "<chemin>"` (même dossier `bin\`) puis ferme la visionneuse. `picazoo.exe`
  reçoit à son tour une instance unique (mutex `Local\Picazoo.App` +
  `QLocalServer`) : s'il est déjà ouvert, le chemin lui est transmis et il
  passe au premier plan. Il cherche le dossier de la photo dans la
  bibliothèque (`Library::folders`, puis l'élément par son nom) : trouvé, la
  vignette est sélectionnée et montrée ; dossier dans une racine surveillée
  mais pas encore indexé, l'analyse est en cours et un message le dit ; hors
  bibliothèque, une question propose d'ajouter le dossier (`addRoot`) et
  rien ne change en cas de refus.
- **Raison** : réutilise les lectures existantes de la bibliothèque, sans
  nouvelle requête dans le cœur ; l'instance unique évite aussi deux
  `picazoo.exe` sur la même base.

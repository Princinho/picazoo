# Research: Images liées

Fonctionnalité 003 — décisions techniques. Contrairement à la 002, aucune
contrainte d'E/S disque ne domine ici : la 003 ne lit que des noms de
fichiers déjà en mémoire (index de la 001). La contrainte dominante est la
**qualité de la détection** : regrouper les vraies séries d'un événement
sans produire de faux groupes à partir des noms génériques donnés par les
appareils.

Les décisions reprennent la pile de la 001/002 (C++20, Qt 6.12, SQLite) et
**n'ajoutent aucune dépendance vcpkg**.

Aucun `NEEDS CLARIFICATION` ne reste ouvert : les 3 choix de produit (voir
spec.md, Clarifications) sont tranchés en amont de cette recherche.

---

## N1 — Détecter une racine commune : préfixe + suffixe de numérotation

- **Decision**: pour chaque nom de fichier (sans extension), séparer un
  **préfixe** (la racine) d'un **suffixe de numérotation** reconnu par une
  des formes suivantes, dans cet ordre de priorité :
  1. séparateur + chiffres en fin de nom (`vacances_bali_001` →
     `vacances_bali` + `_001`) ;
  2. numérotation entre parenthèses, avec ou sans espace avant
     (`photo (12)` → `photo` + ` (12)`) ;
  3. suffixe « copie » de l'Explorateur (`photo - copie`, `photo - copie
     (2)`) → `photo` + le suffixe ;
  4. chiffres collés en fin de nom sans séparateur, seulement si au moins 3
     chiffres (`IMG20240714` → `IMG` + `20240714` ; `img1` n'a pas de
     préfixe distinct, rejeté, voir N2).
  La comparaison des racines se fait **sans tenir compte de la casse**
  (`Vacances_Bali` ≡ `vacances_bali`). Deux éléments dont les racines
  normalisées sont identiques, de formats d'image ou de vidéo quelconques
  (001), rejoignent le même groupe candidat.
- **Rationale**: ces quatre formes couvrent la quasi-totalité des cas
  rencontrés en pratique : export d'appareil photo/téléphone (`_001`,
  `_002`), renommage manuel par l'Explorateur (numérotation entre
  parenthèses, suffixe « copie »), et séquences d'horodatage collées
  (`PXL_20240714_153000`). Prioriser le séparateur explicite avant le
  collage réduit les faux positifs (un numéro de modèle d'appareil en fin de
  nom sans séparateur, par exemple, ne casse pas une racine autrement
  stable).
- **Alternatives considered**: distance d'édition entre tous les noms
  (rejeté : coût quadratique sur de grandes bibliothèques, et capture des
  ressemblances fortuites entre noms sans rapport — deux photos de
  vacances différentes nommées toutes deux « photo » ne doivent pas être
  comparées lettre à lettre) ; expression régulière unique et figée (rejeté
  : moins lisible et moins testable qu'une petite fonction par forme
  reconnue, chacune testée indépendamment) ; inclure l'extension dans la
  comparaison (rejeté : `vacances_bali_001.jpg` et `vacances_bali_001.mp4`,
  edge case explicite de la spec, doivent déjà être regroupés par la racine
  seule, extension ignorée).

## N2 — Racines génériques et trop courtes : écartées par défaut

- **Decision**: une racine candidate (après N1) est jugée **générique** et
  ne forme pas de groupe par défaut si elle correspond à l'un de ces cas :
  - préfixe connu d'appareil/capture (liste fixe et courte, insensible à la
    casse : `IMG`, `DSC`, `DSCN`, `DCIM`, `PXL`, `VID`, `MVI`, `GOPR`,
    `Capture`, `Screenshot`, `Image`, `Photo`, `Video`) suivi d'un numéro ou
    d'un horodatage, sans autre mot distinctif ;
  - racine de moins de 3 caractères alphabétiques significatifs (hors
    chiffres et séparateurs) — ex. `a_1`, `01`, `img`.
  Un réglage (`settings.naming_include_generic_roots`, défaut `0`) permet à
  l'utilisateur d'inclure quand même ces racines dans la détection ; dans ce
  cas elles forment des groupes avec un niveau de confiance plus bas.
- **Rationale**: répond directement à FR-003 et à l'edge case « racine
  générique de l'appareil » de la spec. Sans cette règle, la quasi-totalité
  d'une bibliothèque non renommée (qui ne porte que des noms d'appareil)
  formerait un seul « groupe » géant et inutile par préfixe d'appareil, ce
  qui serait pire que l'absence de regroupement. La liste de préfixes est
  volontairement courte et quasi statique (noms de fabricants stables
  depuis des années) plutôt qu'une heuristique statistique, pour rester
  prévisible et testable.
- **Alternatives considered**: détection statistique de la généricité (une
  racine qui couvre plus de X % de la bibliothèque est générique) : plus
  robuste aux appareils non listés, mais moins prévisible et plus difficile
  à expliquer à l'utilisateur (SC-005 : comprendre sans explication
  pourquoi un groupe est proposé) ; laissé en remarque pour une itération
  future si la liste fixe se révèle insuffisante à l'usage.

## N3 — Un moteur léger, sur le thread d'indexation, sans fil dédié

- **Decision**: `NamingEngine` ne crée **aucun thread propre** (contrairement
  à `DuplicateEngine`, 002). Il s'abonne aux signaux déjà émis par
  `Library` (`scanFinished`, `itemsChanged`) et planifie son travail via
  `Library::post`, qui l'exécute sur le thread d'indexation déjà existant,
  entre deux tâches. Deux modes :
  1. **Repérage complet** : après le premier `scanFinished` d'une
     bibliothèque qui n'a jamais été repérée (ou sur demande explicite),
     relit tous les noms de fichiers indexés, calcule toutes les racines et
     reconstruit les groupes qui n'ont pas de décision utilisateur figée
     (fusion/scission/exclusion/renommage conservés).
  2. **Mise à jour incrémentale** : à chaque lot d'éléments touchés
     (`itemsChanged`/éléments créés par un scan partiel), ne recalcule que
     les racines des éléments concernés : si la racine normalisée d'un
     nouvel élément correspond à un groupe existant (ou à une racine
     précédemment exclue), il rejoint ce groupe (ou reste exclu) sans
     recalculer le reste de la bibliothèque.
- **Rationale**: le calcul (comparaison de chaînes, tri) est largement
  sous la seconde même à 100 000 éléments (pas de lecture de fichier, pas
  de décodage d'image), donc un fil dédié avec sa propre gestion de pause,
  de priorité et de batterie (comme la 002) ajouterait de la complexité
  sans bénéfice mesurable (principe III, mais sans le coût de la 002). Le
  mode incrémental répond à FR-016/SC-004 sans repasser sur toute la
  bibliothèque à chaque ajout.
- **Alternatives considered**: fil dédié comme `DuplicateEngine` (rejeté :
  complexité de synchronisation et de cycle de vie sans contrepartie, vu le
  coût de calcul négligeable) ; recalcul complet à chaque changement
  (rejeté : ne passerait pas SC-003 sur une grande bibliothèque qui reçoit
  des ajouts fréquents, même si chaque recalcul est rapide isolément,
  l'accumulation d'appels sur une surveillance active resterait un gâchis
  évitable) ; détection au moment de l'affichage de l'écran plutôt qu'en
  arrière-plan (rejeté : contredit FR-011, « Images liées » doit afficher un
  nombre de groupes avant même d'être ouvert, comme « Doublons »).

## N4 — Confiance : spécificité de la racine et régularité de la numérotation

- **Decision**: le niveau de confiance d'un groupe (FR-005), exprimé en
  mots, combine deux signaux disponibles sans lecture de fichier :
  - **« Série régulière »** : la racine est spécifique (non générique, N2)
    et les numéros détectés forment une suite quasi continue (peu de
    « trous ») — le cas `vacances_bali_001` à `_005`.
  - **« Même racine »** : la racine est spécifique mais la numérotation est
    irrégulière, absente, ou mélange plusieurs formes (N1) — toujours
    groupé, confiance plus prudente.
  - (Avec le réglage de N2 activé) **« Racine généralement utilisée par
    l'appareil »** : racine générique incluse explicitement, confiance la
    plus basse.
- **Rationale**: répond à FR-005 avec des signaux déjà calculés par N1/N2,
  sans ajouter de calcul coûteux (pas de date EXIF à comparer ici,
  contrairement à la 002 — la 003 ignore volontairement les métadonnées
  visuelles, qui sont le terrain de la 002). Les mots choisis restent
  compréhensibles sans explication (SC-005).
- **Alternatives considered**: réutiliser la date de prise de vue (déjà
  indexée par la 001) pour affiner la confiance (regrouper seulement si les
  dates sont proches) : rejeté pour une première version — risquerait de
  casser des séries dont les dates s'étalent sur plusieurs jours (voyage
  renommé après coup) ; resterait une amélioration possible si la
  confiance par nom seul se révèle insuffisante à l'usage.

## N5 — Indépendance totale vis-à-vis de la 002 (doublons)

- **Decision**: `core/naming/` ne dépend d'aucun type ni d'aucune table de
  `core/duplicates/`, et réciproquement. Les deux modules lisent la même
  table `item` de la 001 en lecture seule et écrivent chacun leurs propres
  tables. Un élément peut apparaître simultanément dans un groupe de
  doublons et un groupe d'images liées ; aucun code ne tente de faire
  correspondre les deux.
- **Rationale**: répond à l'Assumption de la spec et à FR-010 ; garde les
  deux fonctionnalités testables et compréhensibles séparément, et laisse
  un éventuel rapprochement futur (par exemple : suggérer qu'un doublon
  identifié par la 002 et un élément du même groupe de nommage de la 003
  sont liés) comme amélioration explicitement hors périmètre, non comme une
  dépendance structurelle imposée dès maintenant.
- **Alternatives considered**: une table partagée « groupe » générique
  réutilisée par les deux fonctionnalités (rejeté : les cycles de vie
  diffèrent trop — un groupe de doublons se dissout quand il ne reste
  qu'une copie, un groupe de nommage reste stable tant que la racine
  existe — un modèle commun aurait forcé des compromis artificiels dans les
  deux).

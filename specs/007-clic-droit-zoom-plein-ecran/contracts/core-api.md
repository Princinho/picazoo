# Contrat du cœur : zoom, corbeille, presse-papiers, fond d'écran, propriétés

Signatures indicatives (C++20, `namespace picazoo`) ; les noms définitifs
peuvent bouger à l'implémentation, les garanties non.

## `browse/ZoomMotion` — moteur pur (research Z1–Z5)

```cpp
namespace zoom {
enum class Source { Wheel, Key, Button, Pinch };

inline constexpr double kTauMs = 75.0;          // poursuite (Z2)
inline constexpr double kGestureGapMs = 300.0;  // fin d'un geste de molette (FR-013)
inline constexpr double kCeilingThreshold = 0.9; // seuil de la butée (FR-010)
inline constexpr double kCeiling = 1.0;          // butée (FR-010)

class ZoomMotion {
public:
    void setImageSize(QSizeF);       // conserve la taille affichée, réexprime un plafond en cours
    void setView(QSizeF view, double margin);
    void setImmediate(bool);         // animations Windows désactivées (FR-045)

    void reset();                                    // ajustement immédiat (changement de photo)
    void wheel(double angleDeltaY, QPointF at, qint64 nowMs);          // Z3, Z4
    void step(int direction, Source, bool autoRepeat, QPointF at, qint64 nowMs); // clavier, bouton
    void pinchBegin(qint64 nowMs);
    void pinchUpdate(double factorSinceBegin, QPointF centroid, qint64 nowMs); // direct, sans poursuite
    void pinchEnd();
    void setTarget(double scale, QPointF at);       // Ajuster, 100 %, 0 / 1, double-clic
    void panTo(QPointF offset);                      // glisser (borné par clampPan), sans élan (FR-047)
    void stop();                                     // fige la cible sur l'affichage (FR-042)

    bool advance(double dtMs);                       // vrai tant que moving()

    double scale() const;  QPointF offset() const;  double target() const;
    double fitScale() const;  bool fitMode() const;  bool moving() const;
    std::optional<double> ceiling() const;           // plafond du geste en cours (tests)
};
} // namespace zoom
```

Garanties (testées dans `tests/unit/tst_zoommotion.cpp`) :

1. `target()` est toujours dans `[fitScale(), min(ceiling, kMaxScale)]`.
2. `scale()` ne dépasse jamais `target()` dans le sens du mouvement (pas de
   rebond) et l'atteint **exactement** à l'arrêt.
3. Un geste de zoom avant commencé à une échelle affichée < 0,9 ne dépasse
   pas 1,0, quel que soit le nombre de crans ; des crans au plafond ne
   changent rien, y compris après l'arrêt du mouvement tant que le geste
   n'a pas expiré.
4. Un geste commencé à ≥ 0,9 dépasse 1,0 dès le premier cran (si 1,25 × l'échelle > 1).
5. Un cran isolé multiplie la cible par 1,25 exactement ; 10 crans à
   20 crans/s mènent plus loin que 10 crans à 4 crans/s.
6. Après le dernier cran, `moving()` devient faux en moins de 600 ms de
   `advance()` cumulés ; un pas isolé atteint 98 % de son chemin entre 250
   et 350 ms.
7. `stop()`, un cran en sens inverse et `reset()` arrêtent tout mouvement
   dans le même appel.
8. Le point de l'image sous l'ancre reste sous l'ancre à ±0,5 px pendant
   tout le mouvement de zoom avant (mêmes fonctions `ZoomMath` qu'aujourd'hui).
9. `setImmediate(true)` : chaque entrée place `scale()` sur la cible dans le
   même appel ; aucune accélération.
10. Le zoom arrière n'a jamais de plafond.

## `ZoomController` — façade QML (core/browse, `QObject`)

Enregistrée dans les deux exécutables (`qmlRegisterType`, module
`Picazoo.Zoom 1.0`). Propriétés : `scale`, `offsetX`, `offsetY`, `percent`,
`fitScale`, `fitMode`, `moving`, `immediate`, `imageWidth`, `imageHeight`,
`viewWidth`, `viewHeight`, `margin`. Méthodes : celles de `ZoomMotion`, le
temps étant pris d'un `QElapsedTimer` interne ; `advance(frameTimeSeconds)`
est appelée par une `FrameAnimation` QML active tant que `moving`.

## `fileops/FileTrash` — journal + corbeille d'une liste de chemins (research T2)

```cpp
enum class OperationSource { Duplicates, Library, Viewer };

struct LooseTrashEntry { std::optional<ItemId> item; QString path; qint64 bytes; qint64 mtime; };

class FileTrash {
public:
    explicit FileTrash(Database&);
    // Écrit l'opération et ses entrées « pending », puis envoie chaque fichier à la corbeille
    // (platform::moveToRecycleBin). Revérifie avant d'agir que le fichier existe et que taille et
    // date n'ont pas changé. Appel bloquant : à faire hors du fil de l'interface.
    OperationId trash(const QList<LooseTrashEntry>&, OperationSource);
    // Restaure ce qui est encore dans la corbeille ; écrit une opération « restore ».
    OperationId restore(OperationId);
};
```

Garanties : jamais d'effacement définitif ; journal écrit avant le premier
fichier ; un fichier modifié depuis l'ouverture du menu est laissé en place
(`skipped_changed`) ; disque sans corbeille → `skipped_no_recycle_bin`.

## `fileops/TrashService` — ajouts

```cpp
TrashPlan planItems(const QList<ItemId>& items) const; // entrées sans copie gardée (keeper = 0)
OperationId execute(const TrashPlan& confirmed, OperationSource source = OperationSource::Duplicates);
```

- `runTrash` délègue le passage à la corbeille à `FileTrash` ; pour une
  entrée sans copie gardée, la revérification ne compare pas d'empreinte.
- `recoverInterrupted` ignore les opérations créées depuis moins de 60 s.
- Inchangé pour les doublons (tests 002/006 inchangés et verts).

## `platform/win/ClipboardActions` (research M2)

```cpp
// Fichiers (CF_HDROP + Preferred DropEffect = copie) ; image décodée en plus si `image` est fourni.
bool copyFilesToClipboard(const QStringList& paths, const QImage& image = {});
bool copyTextToClipboard(const QString& text); // « Copier le chemin » : un chemin par ligne
```

## `platform/win/Wallpaper` (research W1)

```cpp
struct WallpaperSnapshot { QList<QPair<QString, QString>> monitors; int position; bool slideshow; bool restorable; };
bool wallpaperSupported(const QString& path);       // extension acceptée (JPEG, PNG, BMP, GIF, TIFF)
std::optional<WallpaperSnapshot> snapshotWallpaper();
bool setWallpaper(const QString& path);             // tous les écrans, disposition inchangée
bool restoreWallpaper(const WallpaperSnapshot&);    // seulement si restorable
```

## `platform/win/ShellActions` — ajout (research P1)

```cpp
bool showProperties(const QStringList& paths, WId owner); // fenêtre Propriétés de Windows (1 ou plusieurs fichiers)
```

## `platform/win/RecycleBin` — ajout

```cpp
// Mis en cache par racine de volume ; à appeler hors du fil de l'interface.
bool volumeHasRecycleBinCached(const QString& path);
```

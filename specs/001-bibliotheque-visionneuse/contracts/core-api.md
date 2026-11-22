# Contrat du cœur — 001

Interface entre le cœur (`src/core`, sans dépendance à Qt Quick) et
l'application (`src/app`). C'est la frontière testée par les tests
unitaires et d'intégration, et celle que réutiliseront 002 à 004. Les
signatures sont indicatives ; les noms et les garanties sont contractuels.

## `Library` — façade du cœur

```cpp
class Library : public QObject {
public:
    static std::unique_ptr<Library> open(const QString& dataDir);   // %LOCALAPPDATA%\Picazoo

    // Dossiers surveillés (FR-001, FR-002)
    QList<WatchedRoot> roots() const;
    RootId addRoot(const QString& path);
    void   removeRoot(RootId);                      // index seulement, jamais le disque
    void   excludePath(RootId, const QString& subPath);
    void   includePath(RootId, const QString& subPath);
    static QStringList defaultRoots();              // Images, Bureau, Documents

    // Analyse (FR-005, FR-007)
    void startScan();                               // rattrapage complet en arrière-plan
    void pauseScan();
    void resumeScan();
    ScanProgress progress() const;

    // Lecture (thread-safe, ne bloque jamais sur l'indexeur)
    QList<Folder> folders(FolderSort) const;
    QList<ItemId> items(FolderId, ItemSort) const;
    Item item(ItemId) const;

    // Vignettes (R6)
    void prioritize(const QList<ItemId>& visible, const QList<ItemId>& nearby);
    std::optional<QImage> thumbnail(ItemId, ThumbKind preferred) const;

    // Préférences
    QVariant setting(const QString& key) const;
    void     setSetting(const QString& key, const QVariant&);

signals:
    void foldersChanged();                          // regroupés, au plus toutes les 250 ms
    void itemsChanged(FolderId);
    void itemUpdated(ItemId);                       // métadonnées ou vignette prêtes
    void rootStateChanged(RootId, RootState);
    void progressChanged(ScanProgress);
    void storageError(QString message);
};
```

## `Decoder` — décodage d'un élément (R7, R8)

```cpp
struct DecodeRequest { QString path; Format format; QSize target; int orientation; };
struct DecodeResult  { QImage image; QSize fullSize; bool isPreview; };

class Decoder {
public:
    // Plus petit facteur de réduction qui reste >= target. Sans effet de bord.
    static std::optional<DecodeResult> decode(const DecodeRequest&);
    static std::optional<QImage> embeddedThumbnail(const QString& path, Format);
    static Metadata readMetadata(const QString& path, Format);
};
```

## Garanties transverses

1. **Lecture seule** : tout accès aux fichiers de l'utilisateur se fait en
   lecture partagée (`FILE_SHARE_READ | FILE_SHARE_WRITE |
   FILE_SHARE_DELETE`) ; aucune API d'écriture, de renommage ou de
   suppression n'est appelée sur un chemin situé sous une racine
   surveillée. Un test d'intégration le vérifie (instantané avant et après,
   SC-006).
2. **Identité stable** : un `ItemId` survit aux renommages et déplacements
   entre racines (règles de data-model.md).
3. **Pas d'attente sur l'interface** : les méthodes de lecture répondent à
   partir de l'état en mémoire ou d'une connexion de lecture SQLite ; elles
   n'attendent jamais l'indexeur.
4. **Reprise** : fermer l'application à n'importe quel moment de l'analyse
   ne corrompt pas `library.db` (transactions par lots, WAL) et la
   prochaine analyse reprend les éléments `pending`.

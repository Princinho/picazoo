#pragma once

// Types partagés du cœur de Picazoo (voir specs/001-bibliotheque-visionneuse/data-model.md).

#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <optional>

namespace picazoo {

using ItemId = qint64;
using FolderId = qint64;
using RootId = qint64;

enum class Kind { Image, Raw, Video };

enum class Format {
    Unknown,
    Jpeg, Png, Gif, Bmp, Tiff, Webp, Heic,
    Cr2, Cr3, Nef, Arw, Orf, Rw2, Raf, Dng,
    Mp4, Mov, Avi, Mkv,
};

enum class ThumbState { Pending, Provisional, Done, Unreadable, NoPreview };
enum class MetaState { Pending, Done, Failed };
enum class RootState { Available, Unavailable, Denied };
enum class ThumbKind { Provisional, Final };
enum class FolderSort { DateDesc, DateAsc, NameAsc };
enum class ItemSort { TakenAsc, NameAsc };

struct WatchedRoot {
    RootId id = 0;
    QString path;
    RootState state = RootState::Available;
    std::optional<qint64> lastScanAt;
    QStringList excluded;
};

struct Folder {
    FolderId id = 0;
    RootId rootId = 0;
    QString path;
    QString parentPath;
    QString name;
    int itemCount = 0;
    std::optional<qint64> date; // epoch ms, élément le plus ancien
};

struct Item {
    ItemId id = 0;
    FolderId folderId = 0;
    QString fileName;
    QString folderPath; // rempli par les lectures, pour reconstituer le chemin
    Kind kind = Kind::Image;
    Format format = Format::Unknown;
    qint64 sizeBytes = 0;
    qint64 mtime = 0;
    std::optional<qint64> takenAt;
    qint64 sortDate = 0;
    int width = 0;
    int height = 0;
    int orientation = 1;
    QString camera;
    std::optional<qint64> durationMs;
    MetaState metaState = MetaState::Pending;
    ThumbState thumbState = ThumbState::Pending;
    int version = 1;

    QString path() const { return folderPath.endsWith(u'\\') ? folderPath + fileName : folderPath + u'\\' + fileName; }
};

struct ScanProgress {
    enum class Phase { Idle, Discovering, Indexing, Thumbnails };
    Phase phase = Phase::Idle;
    qint64 done = 0;
    qint64 total = 0;
    bool paused = false;
};

// Conversions vers et depuis les valeurs texte stockées en base.
QString toDb(Kind);
QString toDb(ThumbState);
QString toDb(MetaState);
QString toDb(RootState);
QString toDb(ThumbKind);
Kind kindFromDb(const QString&);
ThumbState thumbStateFromDb(const QString&);
MetaState metaStateFromDb(const QString&);
RootState rootStateFromDb(const QString&);

} // namespace picazoo

Q_DECLARE_METATYPE(picazoo::ScanProgress)

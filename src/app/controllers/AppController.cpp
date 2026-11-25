#include "controllers/AppController.h"

#include "library/Library.h"
#include "library/Version.h"
#include "media/Decoder.h"
#include "media/Formats.h"
#include "platform/win/ClipboardActions.h"
#include "platform/win/RecycleBin.h"
#include "platform/win/Paths.h"
#include "platform/win/ShellActions.h"
#include "video/VideoProbe.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QHash>
#include <QSet>
#include <QGuiApplication>
#include <QPointer>
#include <QThreadPool>
#include <QWindow>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QProcess>
#include <QTimeZone>

#include <algorithm>

using namespace picazoo;

AppController::AppController(Library& library, QObject* parent) : QObject(parent), lib_(library)
{
    connect(&lib_, &Library::rootsChanged, this, &AppController::rootsChanged);
    connect(&lib_, &Library::rootStateChanged, this, &AppController::rootsChanged);
    connect(&lib_, &Library::progressChanged, this, &AppController::progressChanged);
    connect(&lib_, &Library::storageError, this, &AppController::storageError);
}

QVariantList AppController::roots() const
{
    QVariantList out;
    const auto counts = lib_.rootItemCounts();
    for (const auto& r : lib_.roots()) {
        QVariantMap m;
        m.insert(QStringLiteral("count"), counts.value(r.id, 0));
        m.insert(QStringLiteral("id"), r.id);
        m.insert(QStringLiteral("path"), r.path);
        QString state;
        switch (r.state) {
        case RootState::Available: state = QStringLiteral("available"); break;
        case RootState::Unavailable: state = QStringLiteral("unavailable"); break;
        case RootState::Denied: state = QStringLiteral("denied"); break;
        }
        m.insert(QStringLiteral("state"), state);
        m.insert(QStringLiteral("excluded"), r.excluded);
        out << m;
    }
    return out;
}

bool AppController::scanActive() const { return lib_.progress().phase != ScanProgress::Phase::Idle; }

bool AppController::scanPaused() const { return lib_.isPaused(); }

QString AppController::scanText() const
{
    const ScanProgress p = lib_.progress();
    const QLocale fr(QLocale::French);
    switch (p.phase) {
    case ScanProgress::Phase::Discovering:
        return QStringLiteral("Recherche des photos… %1 dossiers").arg(fr.toString(p.done));
    case ScanProgress::Phase::Indexing: return QStringLiteral("Analyse en cours…");
    case ScanProgress::Phase::Thumbnails:
        return QStringLiteral("Analyse : %1 / %2 éléments").arg(fr.toString(p.done), fr.toString(p.total));
    case ScanProgress::Phase::Idle: return {};
    }
    return {};
}

double AppController::scanProgress() const
{
    const ScanProgress p = lib_.progress();
    if (p.phase != ScanProgress::Phase::Thumbnails || p.total <= 0)
        return -1.0;
    return double(p.done) / double(p.total);
}

QString AppController::folderSort() const { return lib_.setting(QStringLiteral("folder_sort")).toString(); }

void AppController::setFolderSort(const QString& s)
{
    lib_.setSetting(QStringLiteral("folder_sort"), s);
    emit sortChanged();
}

QString AppController::itemSort() const { return lib_.setting(QStringLiteral("item_sort")).toString(); }

void AppController::setItemSort(const QString& s)
{
    lib_.setSetting(QStringLiteral("item_sort"), s);
    emit sortChanged();
}

QString AppController::version() const { return QString::fromLatin1(picazoo::version()); }

qint64 AppController::lastFolderId() const { return lib_.setting(QStringLiteral("last_folder_id")).toLongLong(); }

void AppController::setLastFolderId(qint64 id)
{
    lib_.setSetting(QStringLiteral("last_folder_id"), id);
    emit lastFolderChanged();
}

QString AppController::urlToPath(const QUrl& url)
{
    return platform::normalizePath(url.isLocalFile() ? url.toLocalFile() : url.toString());
}

void AppController::addRoot(const QUrl& folder) { lib_.addRoot(urlToPath(folder)); }

void AppController::addRootPath(const QString& folder) { lib_.addRoot(QDir::toNativeSeparators(folder)); }

namespace {

QString normalizedPath(const QString& p)
{
    QString n = QDir::toNativeSeparators(QDir::cleanPath(p));
    while (n.size() > 3 && n.endsWith(u'\\'))
        n.chop(1);
    return n;
}

bool isInside(const QString& path, const QString& folder)
{
    const QString f = normalizedPath(folder);
    return path.compare(f, Qt::CaseInsensitive) == 0 ||
           path.startsWith(f.endsWith(u'\\') ? f : f + u'\\', Qt::CaseInsensitive);
}

} // namespace

void AppController::showPath(const QString& path)
{
    const QFileInfo fi(path);
    const QString file = normalizedPath(fi.absoluteFilePath());
    const QString dir = normalizedPath(fi.absolutePath());

    for (const Folder& f : lib_.folders(FolderSort::NameAsc)) {
        if (normalizedPath(f.path).compare(dir, Qt::CaseInsensitive) != 0)
            continue;
        for (const ItemSummary& s : lib_.itemSummaries(f.id, ItemSort::NameAsc)) {
            if (s.fileName.compare(fi.fileName(), Qt::CaseInsensitive) == 0) {
                emit showItem(s.id);
                return;
            }
        }
        break;
    }
    for (const WatchedRoot& r : lib_.roots()) {
        if (!isInside(dir, r.path))
            continue;
        const bool excluded = std::any_of(r.excluded.cbegin(), r.excluded.cend(),
                                          [&](const QString& e) { return isInside(dir, e); });
        if (!excluded) {
            emit showMessage(QStringLiteral("Picazoo analyse encore ce dossier ; « %1 » apparaîtra dans la "
                                            "bibliothèque d'ici quelques instants.")
                                 .arg(fi.fileName()));
            return;
        }
    }
    Q_UNUSED(file);
    emit askAddFolder(dir);
}

void AppController::removeRoot(qint64 rootId) { lib_.removeRoot(rootId); }

void AppController::excludeFolder(qint64 rootId, const QUrl& folder) { lib_.excludePath(rootId, urlToPath(folder)); }

void AppController::includeFolder(qint64 rootId, const QString& path) { lib_.includePath(rootId, path); }

void AppController::rescan() { lib_.startScan(); }

void AppController::togglePause()
{
    if (lib_.isPaused())
        lib_.resumeScan();
    else
        lib_.pauseScan();
    emit progressChanged();
}

QString AppController::itemPath(qint64 itemId) const
{
    const auto it = lib_.item(itemId);
    return it ? it->path() : QString();
}

void AppController::revealInExplorer(qint64 itemId)
{
    const QString p = itemPath(itemId);
    if (!p.isEmpty())
        platform::revealInExplorer(p);
}

// --------------------------------------------------------------- 007 : menus contextuels

QStringList AppController::pathsOf(const QVariantList& ids) const
{
    QStringList out;
    for (const QVariant& v : ids) {
        const QString p = itemPath(v.toLongLong());
        if (!p.isEmpty())
            out << p;
    }
    return out;
}

void AppController::copyItems(const QVariantList& ids)
{
    const QStringList paths = pathsOf(ids);
    if (paths.isEmpty())
        return;
    if (paths.size() > 1) {
        platform::copyFilesToClipboard(paths);
        return;
    }
    const auto item = lib_.item(ids.first().toLongLong());
    if (!item || item->kind != Kind::Image) {
        platform::copyFilesToClipboard(paths);
        return;
    }
    // Le fichier tout de suite ; l'image décodée hors du fil de l'interface, puis les deux.
    platform::copyFilesToClipboard(paths);
    const DecodeRequest req{paths.first(), item->format, QSize(8000, 8000), item->orientation};
    QPointer<AppController> self(this);
    QThreadPool::globalInstance()->start([self, req] {
        auto r = Decoder::decode(req);
        if (!r || r->image.isNull())
            return;
        QImage img = r->image;
        QMetaObject::invokeMethod(qApp, [self, img, path = req.path] {
            if (self)
                platform::copyFilesToClipboard({path}, img);
        });
    });
}

void AppController::copyPaths(const QVariantList& ids)
{
    const QStringList paths = pathsOf(ids);
    if (!paths.isEmpty())
        platform::copyTextToClipboard(platform::pathsAsText(paths));
}

void AppController::copyFolderPath(const QString& path)
{
    if (!path.isEmpty())
        platform::copyTextToClipboard(QDir::toNativeSeparators(path));
}

void AppController::showProperties(const QVariantList& ids)
{
    const QStringList paths = pathsOf(ids);
    QWindow* w = QGuiApplication::focusWindow();
    platform::showProperties(paths, w ? quintptr(w->winId()) : 0);
}

void AppController::revealFolder(const QString& path)
{
    if (!path.isEmpty())
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

bool AppController::canSetWallpaper(qint64 itemId) const
{
    const auto item = lib_.item(itemId);
    return item && item->kind == Kind::Image && platform::wallpaperSupported(item->path());
}

void AppController::setWallpaper(qint64 itemId)
{
    if (!canSetWallpaper(itemId))
        return;
    const QString path = itemPath(itemId);
    QPointer<AppController> self(this);
    QThreadPool::globalInstance()->start([self, path] {
        const auto before = platform::snapshotWallpaper();
        const bool ok = platform::setWallpaper(path);
        QMetaObject::invokeMethod(qApp, [self, before, ok] {
            if (!self)
                return;
            self->wallpaperBefore_ = ok ? before : std::nullopt;
            emit self->wallpaperChanged(ok, ok && before && before->restorable);
        });
    });
}

void AppController::undoWallpaper()
{
    if (!wallpaperBefore_ || !wallpaperBefore_->restorable)
        return;
    const auto snapshot = *wallpaperBefore_;
    wallpaperBefore_.reset();
    QPointer<AppController> self(this);
    QThreadPool::globalInstance()->start([self, snapshot] {
        const bool ok = platform::restoreWallpaper(snapshot);
        QMetaObject::invokeMethod(qApp, [self, ok] {
            if (self)
                emit self->wallpaperRestored(ok);
        });
    });
}

int AppController::recycleState(const QVariantList& ids)
{
    const QStringList paths = pathsOf(ids);
    if (paths.isEmpty())
        return 0;
    // Une seule question par volume ; la réponse est gardée en cache par le cœur.
    static QHash<QString, int> known; // racine -> 0 / 1 ; absent : en cours
    static QSet<QString> pending;
    int state = 1;
    for (const QString& p : paths) {
        const QString root = p.left(3).toUpper();
        const auto it = known.constFind(root);
        if (it != known.constEnd()) {
            if (*it == 0)
                return 0;
            continue;
        }
        state = -1;
        if (pending.contains(root))
            continue;
        pending.insert(root);
        QPointer<AppController> self(this);
        QThreadPool::globalInstance()->start([self, p, root] {
            const bool has = platform::volumeHasRecycleBinCached(p);
            QMetaObject::invokeMethod(qApp, [self, root, has] {
                known.insert(root, has ? 1 : 0);
                pending.remove(root);
                if (self) {
                    ++self->recycleRevision_;
                    emit self->recycleInfoChanged();
                }
            });
        });
    }
    return state;
}

void AppController::openWithDefaultApp(qint64 itemId)
{
    const QString p = itemPath(itemId);
    if (!p.isEmpty())
        platform::openWithDefaultApp(p);
}

bool AppController::openInStandaloneViewer(qint64 itemId)
{
    const QString p = itemPath(itemId);
    if (p.isEmpty() || !QFileInfo::exists(p))
        return false;
    // Installée à côté de picazoo.exe (bin/), comme dans le dossier de compilation.
    const QString exe = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("picazoo-viewer.exe"));
    if (!QFileInfo::exists(exe))
        return false;
    return QProcess::startDetached(exe, {QDir::toNativeSeparators(p)});
}

QString AppController::formatDuration(qint64 ms) { return picazoo::formatDuration(ms); }

QString AppController::formatBytes(qint64 bytes)
{
    const QLocale fr(QLocale::French);
    if (bytes >= 1024 * 1024)
        return fr.toString(double(bytes) / (1024.0 * 1024.0), 'f', 1) + QStringLiteral(" Mo");
    return fr.toString(qMax<qint64>(1, bytes / 1024)) + QStringLiteral(" Ko");
}

QVariantMap AppController::describe(Library& lib, qint64 itemId)
{
    QVariantMap m;
    const auto it = lib.item(itemId);
    if (!it)
        return m;
    const QLocale fr(QLocale::French);
    m.insert(QStringLiteral("id"), it->id);
    m.insert(QStringLiteral("name"), it->fileName);
    m.insert(QStringLiteral("path"), it->path());
    m.insert(QStringLiteral("kind"), toDb(it->kind));
    m.insert(QStringLiteral("version"), it->version);
    m.insert(QStringLiteral("orientation"), it->orientation);
    m.insert(QStringLiteral("format"), formatName(it->format));
    m.insert(QStringLiteral("width"), it->width);
    m.insert(QStringLiteral("height"), it->height);
    m.insert(QStringLiteral("url"), QUrl::fromLocalFile(it->path()));
    m.insert(QStringLiteral("thumbState"), toDb(it->thumbState));
    if (it->takenAt) {
        m.insert(QStringLiteral("date"),
                 fr.toString(QDateTime::fromMSecsSinceEpoch(*it->takenAt, QTimeZone::UTC), QStringLiteral("d MMMM yyyy 'à' HH:mm")));
        m.insert(QStringLiteral("dateLabel"), QStringLiteral("Prise de vue"));
    } else {
        m.insert(QStringLiteral("date"), fr.toString(QDateTime::fromMSecsSinceEpoch(it->mtime).toLocalTime(),
                                                     QStringLiteral("d MMMM yyyy 'à' HH:mm")));
        m.insert(QStringLiteral("dateLabel"), QStringLiteral("Date du fichier"));
    }
    if (it->width > 0 && it->height > 0)
        m.insert(QStringLiteral("dimensions"), QStringLiteral("%1 × %2 pixels").arg(it->width).arg(it->height));
    m.insert(QStringLiteral("size"), formatBytes(it->sizeBytes));
    if (!it->camera.isEmpty())
        m.insert(QStringLiteral("camera"), it->camera);
    if (it->durationMs)
        m.insert(QStringLiteral("duration"), formatDuration(*it->durationMs));
    return m;
}

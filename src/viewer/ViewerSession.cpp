#include "ViewerSession.h"

#include "FileImageProvider.h"
#include "ViewerSettings.h"

#include "browse/ZoomMath.h"
#include "media/Decoder.h"
#include "media/Formats.h"
#include "media/Metadata.h"
#include "platform/win/ExplorerOrder.h"
#include "platform/win/FileAssociations.h"
#include "platform/win/ShellActions.h"
#include "platform/win/SystemPrefs.h"

#include <windows.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QProcess>
#include <QTimeZone>

#include <algorithm>

using namespace picazoo;

namespace {

struct Family {
    const char* id;
    const char* label;
    QStringList extensions;
};

const QList<Family>& families()
{
    static const QList<Family> f{
        {"jpeg", "JPEG", {QStringLiteral(".jpg"), QStringLiteral(".jpeg"), QStringLiteral(".jpe")}},
        {"png", "PNG", {QStringLiteral(".png")}},
        {"gif", "GIF", {QStringLiteral(".gif")}},
        {"bmp", "BMP", {QStringLiteral(".bmp")}},
        {"tiff", "TIFF", {QStringLiteral(".tif"), QStringLiteral(".tiff")}},
        {"webp", "WebP", {QStringLiteral(".webp")}},
        {"heic", "HEIC", {QStringLiteral(".heic"), QStringLiteral(".heif")}},
        {"raw", "RAW", {QStringLiteral(".cr2"), QStringLiteral(".cr3"), QStringLiteral(".nef"), QStringLiteral(".arw"),
                        QStringLiteral(".orf"), QStringLiteral(".rw2"), QStringLiteral(".raf"), QStringLiteral(".dng")}},
    };
    return f;
}

QString formatBytes(qint64 bytes)
{
    const QLocale fr(QLocale::French);
    if (bytes < 0)
        return {};
    if (bytes >= 1024 * 1024)
        return fr.toString(double(bytes) / (1024.0 * 1024.0), 'f', 1) + QStringLiteral(" Mo");
    return fr.toString(qMax<qint64>(1, bytes / 1024)) + QStringLiteral(" Ko");
}

// Millisecondes écoulées depuis la création du processus (temps d'ouverture réel).
qint64 msSinceProcessStart()
{
    FILETIME created, exited, kernel, user;
    if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user))
        return -1;
    FILETIME now;
    GetSystemTimeAsFileTime(&now);
    auto toU = [](const FILETIME& ft) { return (quint64(ft.dwHighDateTime) << 32) | ft.dwLowDateTime; };
    return qint64((toU(now) - toU(created)) / 10000ULL);
}

} // namespace

ViewerSession::ViewerSession(FileImageProvider& images, ViewerSettings& settings, QScreen* screen, QObject* parent)
    : QObject(parent), images_(images), settings_(settings), screen_(screen)
{
    infoPool_.setMaxThreadCount(1);
    animations_ = platform::animationsEnabled();
    transparency_ = platform::transparencyEnabled();
    connect(&browser_, &FolderBrowser::currentChanged, this, &ViewerSession::onCurrentChanged);
    connect(&browser_, &FolderBrowser::listChanged, this, [this] {
        stamps_.clear();
        for (const BrowseEntry& e : browser_.entries())
            stamps_.insert(e.path, e.modified);
        emit positionChanged();
        prefetch();
    });
    connect(&browser_, &FolderBrowser::wrapped, this, &ViewerSession::wrapped);
    connect(&browser_, &FolderBrowser::currentRemoved, this, [this] {
        emit currentRemoved();
        QTimer::singleShot(1500, this, [this] {
            if (browser_.missingCurrent()) {
                direction_ = 1;
                browser_.next();
            }
        });
    });
}

ViewerSession::~ViewerSession()
{
    infoPool_.clear();
    infoPool_.waitForDone();
}

void ViewerSession::open(const QString& path, quintptr foreground)
{
    sinceOpen_.restart();
    foreground_ = foreground;
    direction_ = 0;
    browser_.setOrderProvider([foreground](const QString& folder) {
        return platform::explorerViewOrder(folder, foreground);
    });
    const BrowseEntry e = entryForFile(path);
    images_.preload(e.path, e.modified); // avant même l'interface (research V2)
    // À la première ouverture, l'ordre de l'Explorateur et les voisines attendent que la
    // photo soit affichée : ils occuperaient l'Explorateur et le processeur pendant le
    // démarrage (research V2).
    browser_.open(e.path, !firstShown_);
}

void ViewerSession::openFromMessage(const QString& path, quintptr foreground)
{
    if (path.isEmpty()) {
        emit raiseRequested();
        return;
    }
    if (sinceOpen_.isValid() && sinceOpen_.elapsed() < 500)
        return; // sélection multiple ouverte dans l'Explorateur : on garde la première
    open(path, foreground);
    emit raiseRequested();
}

QString ViewerSession::currentName() const { return QFileInfo(browser_.currentPath()).fileName(); }

QString ViewerSession::kindOf(const QString& path) const
{
    const Format f = formatFromPath(path);
    if (f == Format::Unknown)
        return QStringLiteral("unsupported");
    if (picazoo::kindOf(f) == Kind::Video)
        return QStringLiteral("video");
    if (f == Format::Gif)
        return QStringLiteral("gif");
    return QStringLiteral("image");
}

QString ViewerSession::currentKind() const
{
    if (browser_.missingCurrent() || browser_.count() == 0)
        return QStringLiteral("none");
    return kindOf(browser_.currentPath());
}

QString ViewerSession::positionText() const
{
    if (!browser_.listed() || browser_.count() == 0 || browser_.currentIndex() < 0)
        return {};
    const QLocale fr(QLocale::French);
    return QStringLiteral("%1 sur %2").arg(fr.toString(browser_.currentIndex() + 1), fr.toString(browser_.count()));
}

QVariantList ViewerSession::neighbors() const
{
    QVariantList out;
    const int i = browser_.currentIndex();
    const int n = int(browser_.count());
    if (i < 0 || n <= 1 || !browser_.listed())
        return out;
    const auto& entries = browser_.entries();
    for (int k = std::max(0, i - 8); k <= std::min(n - 1, i + 8); ++k) {
        QVariantMap m;
        m.insert(QStringLiteral("index"), k);
        m.insert(QStringLiteral("path"), entries[k].path);
        m.insert(QStringLiteral("current"), k == i && !browser_.missingCurrent());
        m.insert(QStringLiteral("thumb"), source(QStringLiteral("thumb"), entries[k].path));
        out.push_back(m);
    }
    return out;
}

qint64 ViewerSession::stampOf(const QString& path) const { return stamps_.value(path, 0); }

QString ViewerSession::source(const QString& kind, const QString& path) const
{
    if (path.isEmpty())
        return {};
    return QStringLiteral("image://photo/") + FileImageProvider::makeId(kind, path, stampOf(path));
}

QSize ViewerSession::fullSize(const QString& path) const { return images_.fullSize(path); }

QString ViewerSession::decodeError(const QString& path) const { return images_.errorKind(path); }

void ViewerSession::next()
{
    direction_ = 1;
    browser_.next();
}

void ViewerSession::previous()
{
    direction_ = -1;
    browser_.previous();
}

void ViewerSession::first()
{
    direction_ = -1;
    browser_.first();
}

void ViewerSession::last()
{
    direction_ = 1;
    browser_.last();
}

void ViewerSession::goTo(int index)
{
    direction_ = index >= browser_.currentIndex() ? 1 : -1;
    browser_.goTo(index);
}

void ViewerSession::onCurrentChanged()
{
    emit currentChanged();
    emit positionChanged();
    prefetch();
    loadInfo();
}

void ViewerSession::prefetch()
{
    if (browser_.count() <= 1 || !firstShown_)
        return;
    const int dir = direction_ >= 0 ? 1 : -1;
    for (int d : {dir, -dir, 2 * dir, -2 * dir, 3 * dir}) {
        const QString p = browser_.pathAt(d);
        if (!p.isEmpty() && kindOf(p) == u"image")
            images_.preload(p, stampOf(p));
    }
}

void ViewerSession::loadInfo()
{
    const quint64 gen = ++infoGen_;
    const auto entry = browser_.currentEntry();
    if (!entry) {
        info_.clear();
        emit infoChanged();
        return;
    }
    const BrowseEntry e = *entry;
    infoPool_.start([this, gen, e] {
        QVariantMap m;
        const QLocale fr(QLocale::French);
        m.insert(QStringLiteral("name"), e.name);
        m.insert(QStringLiteral("path"), e.path);
        Metadata md;
        if (picazoo::kindOf(e.format) != Kind::Video && e.format != Format::Unknown)
            md = Decoder::readMetadata(e.path, e.format);
        const QString fmt = QStringLiteral("d MMMM yyyy 'à' HH:mm");
        if (md.takenAt) {
            m.insert(QStringLiteral("date"),
                     fr.toString(QDateTime::fromMSecsSinceEpoch(*md.takenAt, QTimeZone::UTC), fmt));
            m.insert(QStringLiteral("dateLabel"), QStringLiteral("Prise de vue"));
        } else if (e.modified > 0) {
            m.insert(QStringLiteral("date"), fr.toString(QDateTime::fromMSecsSinceEpoch(e.modified).toLocalTime(), fmt));
            m.insert(QStringLiteral("dateLabel"), QStringLiteral("Date du fichier"));
        }
        const QSize dims = md.size.isValid() ? orientedSize(md.size, md.orientation) : QSize();
        if (dims.isValid() && !dims.isEmpty())
            m.insert(QStringLiteral("dimensions"), QStringLiteral("%1 × %2 pixels").arg(dims.width()).arg(dims.height()));
        m.insert(QStringLiteral("size"), formatBytes(e.size));
        if (!md.camera.isEmpty())
            m.insert(QStringLiteral("camera"), md.camera);
        QMetaObject::invokeMethod(this, [this, gen, m, path = e.path] {
            if (gen != infoGen_)
                return;
            info_ = m;
            // Dimensions inconnues des métadonnées : celles du décodage (thread de l'interface).
            const QSize full = images_.fullSize(path);
            if (!info_.contains(QStringLiteral("dimensions")) && full.isValid() && !full.isEmpty())
                info_.insert(QStringLiteral("dimensions"),
                             QStringLiteral("%1 × %2 pixels").arg(full.width()).arg(full.height()));
            emit infoChanged();
        }, Qt::QueuedConnection);
    });
}

bool ViewerSession::opaque() const { return settings_.opaque(); }

void ViewerSession::setOpaque(bool v)
{
    if (v == opaque())
        return;
    settings_.setOpaque(v);
    emit opaqueChanged();
}

bool ViewerSession::stripVisible() const { return settings_.stripVisible(); }

void ViewerSession::setStripVisible(bool v)
{
    if (v == stripVisible())
        return;
    settings_.setStripVisible(v);
    emit stripVisibleChanged();
}

bool ViewerSession::registered() const { return platform::FileAssociations::isRegistered(); }

// --- Zoom

double ViewerSession::fitScale(double iw, double ih, double vw, double vh) const
{
    return zoom::fitScale({iw, ih}, {vw, vh});
}

double ViewerSession::clampScale(double s, double iw, double ih, double vw, double vh) const
{
    return zoom::clampScale(s, {iw, ih}, {vw, vh});
}

double ViewerSession::wheelFactor(double angleDeltaY) const { return zoom::wheelFactor(angleDeltaY); }

QPointF ViewerSession::zoomAround(double ox, double oy, double scale, double newScale, double ax, double ay) const
{
    return zoom::zoomAround({ox, oy}, scale, newScale, {ax, ay});
}

QPointF ViewerSession::clampOffset(double ox, double oy, double scale, double iw, double ih, double vw, double vh) const
{
    return zoom::clampOffset({ox, oy}, scale, {iw, ih}, {vw, vh});
}

QPointF ViewerSession::anchorOnImage(double ax, double ay, double ox, double oy, double scale, double iw, double ih) const
{
    return zoom::anchorOnImage({ax, ay}, {ox, oy}, scale, {iw, ih});
}

QPointF ViewerSession::zoomEndOffset(double ox, double oy, double scale, double newScale, double ax, double ay,
                                  double iw, double ih, double vw, double vh) const
{
    return zoom::zoomEndOffset({ox, oy}, scale, newScale, {ax, ay}, {iw, ih}, {vw, vh});
}

QPointF ViewerSession::zoomStepOffset(double sx, double sy, double startScale, double endScale, double ex, double ey,
                                   double current, double ax, double ay) const
{
    return zoom::zoomStepOffset({sx, sy}, startScale, endScale, {ex, ey}, current, {ax, ay});
}

QPointF ViewerSession::clampPan(double ox, double oy, double prevX, double prevY, double scale, double iw, double ih,
                             double vw, double vh) const
{
    return zoom::clampPan({ox, oy}, {prevX, prevY}, scale, {iw, ih}, {vw, vh});
}

// --- Actions

void ViewerSession::revealInExplorer()
{
    if (!browser_.currentPath().isEmpty())
        platform::revealInExplorer(browser_.currentPath());
}

void ViewerSession::openWithOther()
{
    if (!browser_.currentPath().isEmpty())
        platform::FileAssociations::openWithDialog(browser_.currentPath());
}

void ViewerSession::openWithDefault()
{
    if (!browser_.currentPath().isEmpty())
        platform::openWithDefaultApp(browser_.currentPath());
}

QString ViewerSession::viewerExe() const
{
    return QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
}

void ViewerSession::openInPicazoo()
{
    const QString app = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("picazoo.exe"));
    if (!QFileInfo::exists(app)) {
        emit message(QStringLiteral("Picazoo est introuvable à côté de la visionneuse."));
        return;
    }
    QStringList args;
    if (!browser_.currentPath().isEmpty())
        args << QStringLiteral("--show") << browser_.currentPath();
    AllowSetForegroundWindow(ASFW_ANY);
    if (QProcess::startDetached(app, args))
        emit quitRequested();
    else
        emit message(QStringLiteral("Picazoo n'a pas pu être lancé."));
}

void ViewerSession::firstImageShown()
{
    if (firstShown_)
        return;
    firstShown_ = true;
    browser_.startOrderQuery();
    prefetch();
    if (qEnvironmentVariableIsSet("PICAZOO_VIEWER_TIMING")) {
        const QString line = QStringLiteral("picazoo-viewer : première photo affichée %1 ms après le lancement\n")
                                 .arg(msSinceProcessStart());
        OutputDebugStringW(reinterpret_cast<const wchar_t*>(line.utf16()));
        fputs(line.toLocal8Bit().constData(), stderr);
    }
    QTimer::singleShot(1500, this, [this] {
        if (shouldPropose()) {
            proposalVisible_ = true;
            emit proposalVisibleChanged();
        }
    });
}

// --- Association

bool ViewerSession::shouldPropose() const
{
    if (qEnvironmentVariableIsSet("PICAZOO_SCREENSHOT") && !qEnvironmentVariableIsSet("PICAZOO_SHOW_PROPOSAL"))
        return false;
    const QString answer = settings_.assocAnswer();
    if (answer == u"never" || answer == u"accepted")
        return false;
    if (answer == u"later") {
        const QDateTime at = settings_.assocAskedAt();
        if (at.isValid() && at.addDays(7) > QDateTime::currentDateTimeUtc())
            return false;
    }
    return !platform::FileAssociations::isDefaultFor(QStringLiteral(".jpg"), viewerExe());
}

QVariantList ViewerSession::formatFamilies() const
{
    QVariantList out;
    for (const Family& f : families()) {
        QVariantMap m;
        m.insert(QStringLiteral("id"), QString::fromLatin1(f.id));
        m.insert(QStringLiteral("label"), QString::fromLatin1(f.label));
        m.insert(QStringLiteral("extensions"), f.extensions.join(u' '));
        out.push_back(m);
    }
    return out;
}

QStringList ViewerSession::extensionsFor(const QStringList& ids) const
{
    QStringList out;
    for (const Family& f : families()) {
        if (ids.contains(QString::fromLatin1(f.id)))
            out += f.extensions;
    }
    return out;
}

QVariantList ViewerSession::associationStatus() const
{
    QVariantList out;
    for (const auto& s : platform::FileAssociations::status(viewerExe())) {
        QVariantMap m;
        m.insert(QStringLiteral("extension"), s.extension);
        m.insert(QStringLiteral("picazoo"), s.isDefault);
        out.push_back(m);
    }
    return out;
}

bool ViewerSession::registerNow()
{
    QString error;
    const bool ok = platform::FileAssociations::registerApp(platform::RegistryScope::User, viewerExe(), &error);
    emit registeredChanged();
    if (!ok)
        emit message(error);
    return ok;
}

void ViewerSession::showProposal()
{
    proposalVisible_ = true;
    emit proposalVisibleChanged();
}

void ViewerSession::hideProposal()
{
    proposalVisible_ = false;
    emit proposalVisibleChanged();
}

void ViewerSession::answerProposal(const QString& answer, const QStringList& ids)
{
    settings_.setAssocAnswer(answer, ids);
    proposalVisible_ = false;
    emit proposalVisibleChanged();
    if (answer != u"accepted")
        return;
    if (!platform::FileAssociations::isRegistered() && !registerNow())
        return;
    platform::FileAssociations::openDefaultAppsSettings();
}

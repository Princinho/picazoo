#include "controllers/ViewerController.h"

#include "browse/ZoomMath.h"
#include "controllers/AppController.h"
#include "library/Library.h"
#include "models/GridRowModel.h"
#include "platform/win/SystemPrefs.h"
#include "providers/ViewerImageProvider.h"

#include <QLocale>

using namespace picazoo;

ViewerController::ViewerController(Library& library, GridRowModel& grid, ViewerImageProvider& images,
                                   QObject* parent)
    : QObject(parent), lib_(library), grid_(grid), images_(images),
      animations_(platform::animationsEnabled())
{
    connect(&grid_, &GridRowModel::structureChanged, this, [this] { nav_.setOrder(grid_.flatOrder()); });
    connect(&lib_, &Library::itemUpdated, this, [this](qint64 id) {
        if (id == current_)
            refresh();
    });
    nav_.setOrder(grid_.flatOrder());
}

QString ViewerController::position() const
{
    const int i = nav_.indexOf(current_);
    if (i < 0)
        return {};
    const QLocale fr(QLocale::French);
    return QStringLiteral("%1 / %2").arg(fr.toString(i + 1), fr.toString(nav_.size()));
}

bool ViewerController::hasPrevious() const { return nav_.previous(current_) != 0; }

bool ViewerController::hasNext() const { return nav_.next(current_) != 0; }

void ViewerController::setScreenSize(QSize s)
{
    if (s == screen_ || s.isEmpty())
        return;
    screen_ = s;
    images_.setScreenSize(s);
    emit screenSizeChanged();
}

void ViewerController::open(qint64 itemId)
{
    nav_.setOrder(grid_.flatOrder());
    setCurrent(itemId);
}

void ViewerController::next()
{
    if (const ItemId n = nav_.next(current_))
        setCurrent(n);
}

void ViewerController::previous()
{
    if (const ItemId p = nav_.previous(current_))
        setCurrent(p);
}

double ViewerController::fitScale(double iw, double ih, double vw, double vh) const
{
    return zoom::fitScale({iw, ih}, {vw, vh});
}

double ViewerController::clampScale(double s, double iw, double ih, double vw, double vh) const
{
    return zoom::clampScale(s, {iw, ih}, {vw, vh});
}

double ViewerController::wheelFactor(double angleDeltaY) const { return zoom::wheelFactor(angleDeltaY); }

QPointF ViewerController::clampOffset(double ox, double oy, double scale, double iw, double ih, double vw,
                                      double vh) const
{
    return zoom::clampOffset({ox, oy}, scale, {iw, ih}, {vw, vh});
}

QPointF ViewerController::anchorOnImage(double ax, double ay, double ox, double oy, double scale, double iw, double ih) const
{
    return zoom::anchorOnImage({ax, ay}, {ox, oy}, scale, {iw, ih});
}

QPointF ViewerController::zoomEndOffset(double ox, double oy, double scale, double newScale, double ax, double ay,
                                  double iw, double ih, double vw, double vh) const
{
    return zoom::zoomEndOffset({ox, oy}, scale, newScale, {ax, ay}, {iw, ih}, {vw, vh});
}

QPointF ViewerController::zoomStepOffset(double sx, double sy, double startScale, double endScale, double ex, double ey,
                                   double current, double ax, double ay) const
{
    return zoom::zoomStepOffset({sx, sy}, startScale, endScale, {ex, ey}, current, {ax, ay});
}

QPointF ViewerController::clampPan(double ox, double oy, double prevX, double prevY, double scale, double iw, double ih,
                             double vw, double vh) const
{
    return zoom::clampPan({ox, oy}, {prevX, prevY}, scale, {iw, ih}, {vw, vh});
}

void ViewerController::refresh()
{
    info_ = AppController::describe(lib_, current_);
    buildStrip();
    emit currentChanged();
}

void ViewerController::setCurrent(qint64 id)
{
    current_ = id;
    info_ = AppController::describe(lib_, id);
    buildStrip();
    emit currentChanged();
    prefetch();
}

QString ViewerController::folderLabel() const
{
    const qint64 folder = grid_.folderOfItem(current_);
    const QVariantList items = grid_.itemsOfFolder(folder);
    const int index = int(items.indexOf(QVariant(current_)));
    QString name;
    for (const auto& f : grid_.folders()) {
        if (f.id == folder) {
            name = f.name;
            break;
        }
    }
    if (index < 0 || name.isEmpty())
        return position();
    const QLocale fr(QLocale::French);
    return QStringLiteral("%1 · %2 sur %3").arg(name, fr.toString(index + 1), fr.toString(items.size()));
}

void ViewerController::buildStrip()
{
    strip_.clear();
    const int i = nav_.indexOf(current_);
    if (i < 0)
        return;
    for (int k = i - 5; k <= i + 5; ++k) {
        const ItemId id = nav_.at(k);
        if (!id)
            continue;
        const auto it = lib_.item(id);
        if (!it)
            continue;
        QVariantMap m;
        m.insert(QStringLiteral("id"), id);
        m.insert(QStringLiteral("version"), it->version);
        m.insert(QStringLiteral("current"), id == current_);
        const bool hasThumb = it->thumbState == ThumbState::Done || it->thumbState == ThumbState::Provisional;
        m.insert(QStringLiteral("thumb"), hasThumb ? QStringLiteral("image://thumb/%1/%2/%3")
                                                         .arg(id)
                                                         .arg(it->version)
                                                         .arg(toDb(it->thumbState))
                                                   : QString());
        m.insert(QStringLiteral("video"), it->kind == Kind::Video);
        strip_ << m;
    }
}

void ViewerController::prefetch()
{
    for (ItemId id : nav_.prefetch(current_, 2)) {
        const auto it = lib_.item(id);
        if (it && it->kind != Kind::Video)
            images_.preload(id, it->version, screen_);
    }
}

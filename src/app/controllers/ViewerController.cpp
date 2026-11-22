#include "controllers/ViewerController.h"

#include "controllers/AppController.h"
#include "library/Library.h"
#include "models/GridRowModel.h"
#include "providers/ViewerImageProvider.h"

#include <QLocale>

using namespace picazoo;

ViewerController::ViewerController(Library& library, GridRowModel& grid, ViewerImageProvider& images,
                                   QObject* parent)
    : QObject(parent), lib_(library), grid_(grid), images_(images)
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

#include "models/ItemSelection.h"

#include "models/GridRowModel.h"

#include <algorithm>

ItemSelection::ItemSelection(GridRowModel& grid, QObject* parent) : QObject(parent), grid_(grid)
{
    connect(&grid_, &GridRowModel::structureChanged, this, [this] {
        // Retire de la sélection les éléments qui ont disparu de la bibliothèque.
        bool changed = false;
        for (auto it = selected_.begin(); it != selected_.end();) {
            if (grid_.flatIndexOf(*it) < 0) {
                it = selected_.erase(it);
                changed = true;
            } else {
                ++it;
            }
        }
        if (current_ && grid_.flatIndexOf(current_) < 0) {
            current_ = 0;
            emit currentChanged();
        }
        if (changed)
            touch();
    });
}

void ItemSelection::touch()
{
    ++revision_;
    emit selectionChanged();
}

void ItemSelection::setCurrent(qint64 id)
{
    if (id == current_)
        return;
    current_ = id;
    anchor_ = id;
    selected_ = id ? QSet<qint64>{id} : QSet<qint64>{};
    emit currentChanged();
    touch();
}

void ItemSelection::click(qint64 id, int modifiers)
{
    if (modifiers == 1) { // Ctrl
        if (selected_.contains(id))
            selected_.remove(id);
        else
            selected_.insert(id);
        current_ = id;
        anchor_ = id;
        emit currentChanged();
        touch();
    } else if (modifiers == 2) { // Maj
        selectRange(anchor_ ? anchor_ : id, id, false);
        current_ = id;
        emit currentChanged();
    } else {
        current_ = 0; // force la notification
        setCurrent(id);
    }
}

void ItemSelection::selectRange(qint64 fromId, qint64 toId, bool add)
{
    int a = grid_.flatIndexOf(fromId);
    int b = grid_.flatIndexOf(toId);
    if (a < 0 || b < 0)
        return;
    if (a > b)
        std::swap(a, b);
    if (!add)
        selected_.clear();
    for (int i = a; i <= b; ++i)
        selected_.insert(grid_.itemAtFlat(i));
    touch();
}

void ItemSelection::selectFolder(qint64 folderId)
{
    selected_.clear();
    for (const QVariant& v : grid_.itemsOfFolder(folderId))
        selected_.insert(v.toLongLong());
    touch();
}

void ItemSelection::setSelection(const QVariantList& ids, bool add)
{
    if (!add)
        selected_.clear();
    for (const QVariant& v : ids)
        selected_.insert(v.toLongLong());
    if (!ids.isEmpty()) {
        current_ = ids.last().toLongLong();
        emit currentChanged();
    }
    touch();
}

void ItemSelection::clear()
{
    selected_.clear();
    touch();
}

qint64 ItemSelection::move(int dx, int dy, bool extend)
{
    const qint64 from = current_ ? current_ : grid_.itemAtFlat(0);
    if (!from)
        return 0;
    const qint64 to = current_ ? grid_.neighbor(from, dx, dy) : from;
    if (extend) {
        if (!anchor_)
            anchor_ = from;
        selectRange(anchor_, to, false);
        current_ = to;
        emit currentChanged();
    } else {
        current_ = 0;
        setCurrent(to);
    }
    return to;
}

QVariantList ItemSelection::selectedIds() const
{
    QVariantList out;
    for (qint64 id : selected_)
        out << id;
    return out;
}

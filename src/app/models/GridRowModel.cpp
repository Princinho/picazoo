#include "models/GridRowModel.h"

#include "library/Library.h"

#include <QDateTime>
#include <QLocale>
#include <QTimeZone>

#include <algorithm>

using namespace picazoo;

namespace {

QString formatDate(const std::optional<qint64>& ms)
{
    if (!ms)
        return {};
    const QDate d = QDateTime::fromMSecsSinceEpoch(*ms, QTimeZone::UTC).date();
    return QLocale(QLocale::French).toString(d, QStringLiteral("d MMMM yyyy"));
}

QString stateName(ThumbState s)
{
    switch (s) {
    case ThumbState::Pending: return QStringLiteral("pending");
    case ThumbState::Provisional: return QStringLiteral("provisional");
    case ThumbState::Done: return QStringLiteral("done");
    case ThumbState::Unreadable: return QStringLiteral("unreadable");
    case ThumbState::NoPreview: return QStringLiteral("no_preview");
    }
    return QStringLiteral("pending");
}

bool sameRow(const GridRow& a, const GridRow& b)
{
    return a.type == b.type && a.folder == b.folder && a.first == b.first && a.count == b.count;
}

} // namespace

GridRowModel::GridRowModel(Library& library, QObject* parent) : QAbstractListModel(parent), lib_(library)
{
    thumbSize_ = lib_.setting(QStringLiteral("thumb_size")).toInt();
    refreshTimer_.setSingleShot(true);
    refreshTimer_.setInterval(0);
    connect(&refreshTimer_, &QTimer::timeout, this, [this] { refresh(false); });
    connect(&lib_, &Library::foldersChanged, this, &GridRowModel::onFoldersChanged);
    connect(&lib_, &Library::itemsChanged, this, &GridRowModel::onItemsChanged);
    connect(&lib_, &Library::itemUpdated, this, &GridRowModel::onItemUpdated);
    reload();
}

FolderSort GridRowModel::folderSort() const
{
    const QString s = lib_.setting(QStringLiteral("folder_sort")).toString();
    return s == u"name_asc" ? FolderSort::NameAsc : s == u"date_asc" ? FolderSort::DateAsc : FolderSort::DateDesc;
}

ItemSort GridRowModel::itemSort() const
{
    return lib_.setting(QStringLiteral("item_sort")).toString() == u"name_asc" ? ItemSort::NameAsc
                                                                               : ItemSort::TakenAsc;
}

GridParams GridRowModel::params() const
{
    GridParams p;
    p.width = width_;
    p.thumbSize = thumbSize_;
    p.spacing = spacing();
    p.headerHeight = 60;
    return p;
}

int GridRowModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : int(rows_.size()); }

QHash<int, QByteArray> GridRowModel::roleNames() const
{
    return {
        {RowTypeRole, "rowType"},       {FolderIdRole, "folderId"},     {FolderNameRole, "folderName"},
        {FolderPathRole, "folderPath"}, {FolderDateRole, "folderDate"}, {FolderCountRole, "folderCount"},
        {RowItemsRole, "rowItems"},     {RowHeightRole, "rowHeight"},   {FirstIndexRole, "firstIndex"},
        {RowYRole, "rowY"},
    };
}

QVariantMap GridRowModel::cell(const ItemSummary& s) const
{
    QVariantMap m;
    m.insert(QStringLiteral("id"), s.id);
    m.insert(QStringLiteral("version"), s.version);
    m.insert(QStringLiteral("kind"), toDb(s.kind));
    m.insert(QStringLiteral("state"), stateName(s.thumbState));
    m.insert(QStringLiteral("duration"), s.durationMs ? QVariant(*s.durationMs) : QVariant());
    m.insert(QStringLiteral("name"), s.fileName);
    return m;
}

QVariant GridRowModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= rows_.size())
        return {};
    const GridRow& r = rows_[index.row()];
    const auto fi = folderIndex_.constFind(r.folder);
    const Folder* f = fi != folderIndex_.constEnd() ? &folders_[*fi] : nullptr;
    switch (role) {
    case RowTypeRole: return r.type == GridRow::Type::Header ? QStringLiteral("header") : QStringLiteral("items");
    case FolderIdRole: return r.folder;
    case FolderNameRole: return f ? f->name : QString();
    case FolderPathRole: return f ? f->path : QString();
    case FolderDateRole: return f ? formatDate(f->date) : QString();
    case FolderCountRole: return f ? f->itemCount : 0;
    case RowHeightRole: return r.height;
    case FirstIndexRole: return r.first;
    case RowYRole: return r.y;
    case RowItemsRole: {
        QVariantList list;
        if (r.type != GridRow::Type::Items)
            return list;
        const auto& items = items_.value(r.folder);
        for (int i = r.first; i < r.first + r.count && i < items.size(); ++i)
            list << cell(items[i]);
        return list;
    }
    default: return {};
    }
}

void GridRowModel::setViewWidth(double w)
{
    if (qFuzzyCompare(w, width_) || w <= 0)
        return;
    const int before = GridLayout::columnsFor(params());
    width_ = w;
    if (GridLayout::columnsFor(params()) != before) {
        refresh(true);
    }
    emit layoutParamsChanged();
}

void GridRowModel::setThumbSize(int s)
{
    s = std::clamp(s, 64, 320);
    if (s == thumbSize_)
        return;
    thumbSize_ = s;
    lib_.setSetting(QStringLiteral("thumb_size"), s);
    refresh(true);
    emit layoutParamsChanged();
}

void GridRowModel::reload()
{
    beginResetModel();
    folders_ = lib_.folders(folderSort());
    folderIndex_.clear();
    for (qsizetype i = 0; i < folders_.size(); ++i)
        folderIndex_.insert(folders_[i].id, i);
    items_ = lib_.allItemSummaries(itemSort());
    QList<QPair<FolderId, int>> blocks;
    for (const auto& f : folders_)
        blocks.append({f.id, int(items_.value(f.id).size())});
    layout_.build(blocks, params());
    rows_ = layout_.rows();
    rebuildFlat();
    dirtyFolders_.clear();
    foldersDirty_ = false;
    endResetModel();
    emit countsChanged();
    emit structureChanged();
}

void GridRowModel::onFoldersChanged()
{
    foldersDirty_ = true;
    scheduleRefresh();
}

void GridRowModel::onItemsChanged(qint64 folderId)
{
    dirtyFolders_.insert(folderId);
    scheduleRefresh();
}

void GridRowModel::scheduleRefresh()
{
    if (!refreshTimer_.isActive())
        refreshTimer_.start();
}

void GridRowModel::onItemUpdated(qint64 itemId)
{
    const auto w = where_.constFind(itemId);
    if (w == where_.constEnd())
        return;
    if (dirtyFolders_.contains(w->first))
        return; // le dossier entier sera relu
    const auto item = lib_.item(itemId);
    if (!item)
        return;
    auto it = items_.find(w->first);
    if (it == items_.end() || w->second >= it->size())
        return;
    ItemSummary& s = (*it)[w->second];
    if (s.id != itemId)
        return;
    s.thumbState = item->thumbState;
    s.version = item->version;
    s.durationMs = item->durationMs;
    const int row = layout_.rowOfItem(w->first, w->second);
    if (row >= 0 && row < rows_.size())
        emit dataChanged(index(row), index(row), {RowItemsRole});
}

void GridRowModel::refresh(bool force)
{
    const QSet<FolderId> dirty = std::exchange(dirtyFolders_, {});
    const bool foldersDirty = std::exchange(foldersDirty_, false);

    QList<Folder> newFolders = foldersDirty || force ? lib_.folders(folderSort()) : folders_;
    QHash<FolderId, QList<ItemSummary>> newItems = items_;
    QSet<FolderId> changed = dirty;
    if (foldersDirty || force) {
        QSet<FolderId> present;
        for (const auto& f : newFolders) {
            present.insert(f.id);
            if (!newItems.contains(f.id))
                changed.insert(f.id);
            else if (folderIndex_.contains(f.id) && folders_[folderIndex_[f.id]].itemCount != f.itemCount)
                changed.insert(f.id);
        }
        for (auto it = newItems.begin(); it != newItems.end();) {
            if (!present.contains(it.key()))
                it = newItems.erase(it);
            else
                ++it;
        }
    }
    for (FolderId f : changed)
        newItems.insert(f, lib_.itemSummaries(f, itemSort()));

    QList<QPair<FolderId, int>> blocks;
    for (const auto& f : newFolders)
        blocks.append({f.id, int(newItems.value(f.id).size())});
    GridLayout newLayout;
    newLayout.build(blocks, params());
    const QList<GridRow>& newRows = newLayout.rows();

    const bool columnsChanged = newLayout.columns() != layout_.columns();
    const QList<Folder> oldFolders = folders_;

    // Les données des lignes conservées sont lues depuis les nouvelles structures.
    folders_ = newFolders;
    folderIndex_.clear();
    for (qsizetype i = 0; i < folders_.size(); ++i)
        folderIndex_.insert(folders_[i].id, i);
    items_ = std::move(newItems);

    QSet<FolderId> oldSet, newSet;
    for (const auto& f : oldFolders)
        oldSet.insert(f.id);
    for (const auto& f : folders_)
        newSet.insert(f.id);

    // Ordre relatif des dossiers communs inchangé ? Sinon, réinitialisation.
    QList<FolderId> oldCommon, newCommon;
    for (const auto& f : oldFolders)
        if (newSet.contains(f.id))
            oldCommon << f.id;
    for (const auto& f : folders_)
        if (oldSet.contains(f.id))
            newCommon << f.id;

    if (columnsChanged || oldCommon != newCommon || rows_.size() != layout_.rows().size()) {
        beginResetModel();
        layout_ = newLayout;
        rows_ = layout_.rows();
        rebuildFlat();
        endResetModel();
        emit countsChanged();
        emit structureChanged();
        return;
    }

    const GridLayout oldLayout = layout_;
    int pos = 0;
    qsizetype i = 0, j = 0;
    while (i < oldFolders.size() || j < folders_.size()) {
        const FolderId of = i < oldFolders.size() ? oldFolders[i].id : -1;
        const FolderId nf = j < folders_.size() ? folders_[j].id : -1;
        if (of == nf) {
            const auto [ob, oc] = oldLayout.block(of);
            const auto [nb, nc] = newLayout.block(nf);
            Q_UNUSED(ob);
            const int common = std::min(oc, nc);
            bool rowsDiffer = changed.contains(nf);
            for (int k = 0; k < common; ++k) {
                if (!sameRow(rows_[pos + k], newRows[nb + k]))
                    rowsDiffer = true;
                rows_[pos + k] = newRows[nb + k];
            }
            if (rowsDiffer && common > 0)
                emit dataChanged(index(pos), index(pos + common - 1));
            if (nc > oc) {
                beginInsertRows({}, pos + oc, pos + nc - 1);
                for (int k = oc; k < nc; ++k)
                    rows_.insert(pos + k, newRows[nb + k]);
                endInsertRows();
            } else if (nc < oc) {
                beginRemoveRows({}, pos + nc, pos + oc - 1);
                rows_.remove(pos + nc, oc - nc);
                endRemoveRows();
            }
            pos += nc;
            ++i;
            ++j;
        } else if (nf != -1 && !oldSet.contains(nf)) {
            const auto [nb, nc] = newLayout.block(nf);
            beginInsertRows({}, pos, pos + nc - 1);
            for (int k = 0; k < nc; ++k)
                rows_.insert(pos + k, newRows[nb + k]);
            endInsertRows();
            pos += nc;
            ++j;
        } else {
            const auto [ob, oc] = oldLayout.block(of);
            Q_UNUSED(ob);
            beginRemoveRows({}, pos, pos + oc - 1);
            rows_.remove(pos, oc);
            endRemoveRows();
            ++i;
        }
    }
    layout_ = newLayout;
    rows_ = layout_.rows();
    rebuildFlat();
    emit countsChanged();
    emit structureChanged();
}

void GridRowModel::rebuildFlat()
{
    flat_.clear();
    where_.clear();
    for (const auto& f : folders_) {
        const auto& items = items_.value(f.id);
        for (int k = 0; k < items.size(); ++k) {
            flat_ << items[k].id;
            where_.insert(items[k].id, {f.id, k});
        }
    }
}

double GridRowModel::yOfFolder(qint64 folderId) const { return layout_.yOfFolder(folderId); }

int GridRowModel::headerRowOfFolder(qint64 folderId) const { return layout_.headerRow(folderId); }

int GridRowModel::rowOfItem(qint64 itemId) const
{
    const auto w = where_.constFind(itemId);
    return w == where_.constEnd() ? -1 : layout_.rowOfItem(w->first, w->second);
}

double GridRowModel::yOfItem(qint64 itemId) const
{
    const int r = rowOfItem(itemId);
    return r < 0 ? -1.0 : layout_.rows()[r].y;
}

void GridRowModel::reportRows(int firstRow, int lastRow)
{
    const int n = int(layout_.rows().size());
    if (n == 0)
        return;
    firstRow = std::clamp(firstRow, 0, n - 1);
    lastRow = std::clamp(lastRow, firstRow, n - 1);
    auto collect = [this, n](int a, int b) {
        QList<ItemId> ids;
        a = std::max(0, a);
        b = std::min(n - 1, b);
        for (int r = a; r <= b; ++r) {
            const GridRow& row = layout_.rows()[r];
            if (row.type != GridRow::Type::Items)
                continue;
            const auto& items = items_.value(row.folder);
            for (int k = row.first; k < row.first + row.count && k < items.size(); ++k)
                ids << items[k].id;
        }
        return ids;
    };
    const int span = lastRow - firstRow + 1;
    const QList<ItemId> visible = collect(firstRow, lastRow);
    QList<ItemId> nearby = collect(lastRow + 1, lastRow + 2 * span);
    nearby += collect(firstRow - 2 * span, firstRow - 1);
    lib_.prioritize(visible, nearby);
}

int GridRowModel::flatIndexOf(qint64 itemId) const { return int(flat_.indexOf(itemId)); }

qint64 GridRowModel::itemAtFlat(int flatIndex) const
{
    return flatIndex >= 0 && flatIndex < flat_.size() ? flat_[flatIndex] : 0;
}

qint64 GridRowModel::neighbor(qint64 itemId, int dx, int dy) const
{
    const auto w = where_.constFind(itemId);
    if (w == where_.constEnd())
        return flat_.isEmpty() ? 0 : flat_.first();
    if (dy == 0) {
        const int i = flatIndexOf(itemId) + dx;
        return itemAtFlat(std::clamp(i, 0, int(flat_.size()) - 1));
    }
    // Déplacement vertical : même colonne dans la rangée voisine (y compris du dossier voisin).
    const int cols = layout_.columns();
    const int row = layout_.rowOfItem(w->first, w->second);
    const int col = w->second % cols;
    int r = row + (dy > 0 ? 1 : -1);
    while (r >= 0 && r < layout_.rows().size() && layout_.rows()[r].type != GridRow::Type::Items)
        r += dy > 0 ? 1 : -1;
    if (r < 0 || r >= layout_.rows().size())
        return itemId;
    const GridRow& target = layout_.rows()[r];
    const auto& items = items_.value(target.folder);
    const int k = target.first + std::min(col, target.count - 1);
    return k < items.size() ? items[k].id : itemId;
}

qint64 GridRowModel::folderOfItem(qint64 itemId) const { return where_.value(itemId, {0, 0}).first; }

QVariantList GridRowModel::itemsOfFolder(qint64 folderId) const
{
    QVariantList out;
    for (const auto& s : items_.value(folderId))
        out << s.id;
    return out;
}

qint64 GridRowModel::firstItemOfFolder(qint64 folderId) const
{
    const auto& items = items_.value(folderId);
    return items.isEmpty() ? 0 : items.first().id;
}

QVariantList GridRowModel::itemsInRect(double x0, double y0, double x1, double y1) const
{
    QVariantList out;
    const int n = int(layout_.rows().size());
    if (n == 0)
        return out;
    const double left = std::min(x0, x1), right = std::max(x0, x1);
    const double top = std::min(y0, y1), bottom = std::max(y0, y1);
    const GridParams& p = layout_.params();
    const double cell = p.thumbSize + p.spacing;
    for (int r = layout_.rowAtY(top); r >= 0 && r < n; ++r) {
        const GridRow& row = layout_.rows()[r];
        if (row.y > bottom)
            break;
        if (row.type != GridRow::Type::Items || row.y + p.thumbSize < top)
            continue;
        const auto& items = items_.value(row.folder);
        for (int k = 0; k < row.count; ++k) {
            const double cx = p.margin + k * cell;
            if (cx + p.thumbSize >= left && cx <= right && row.first + k < items.size())
                out << items[row.first + k].id;
        }
    }
    return out;
}

QString GridRowModel::kindOf(qint64 itemId) const
{
    const auto w = where_.constFind(itemId);
    if (w == where_.constEnd())
        return {};
    const auto& items = items_.value(w->first);
    return w->second < items.size() ? toDb(items[w->second].kind) : QString();
}

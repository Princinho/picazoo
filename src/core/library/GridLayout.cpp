#include "library/GridLayout.h"

#include <algorithm>
#include <cmath>

namespace picazoo {

int GridLayout::columnsFor(const GridParams& p)
{
    const double usable = p.width - 2.0 * p.margin + p.spacing;
    return std::max(1, int(std::floor(usable / double(p.thumbSize + p.spacing))));
}

void GridLayout::build(const QList<QPair<FolderId, int>>& blocks, const GridParams& params)
{
    params_ = params;
    columns_ = columnsFor(params);
    rows_.clear();
    blocks_.clear();
    const double cell = params.thumbSize + params.spacing;
    double y = 0;
    for (const auto& [folder, count] : blocks) {
        const int firstRow = int(rows_.size());
        rows_.push_back(GridRow{GridRow::Type::Header, folder, 0, count, y, double(params.headerHeight)});
        y += params.headerHeight;
        for (int i = 0; i < count; i += columns_) {
            rows_.push_back(GridRow{GridRow::Type::Items, folder, i, std::min(columns_, count - i), y, cell});
            y += cell;
        }
        blocks_.insert(folder, {firstRow, int(rows_.size()) - firstRow});
    }
}

double GridLayout::totalHeight() const
{
    return rows_.isEmpty() ? 0.0 : rows_.back().y + rows_.back().height;
}

int GridLayout::rowAtY(double y) const
{
    if (rows_.isEmpty())
        return -1;
    auto it = std::upper_bound(rows_.begin(), rows_.end(), y, [](double v, const GridRow& r) { return v < r.y; });
    const int index = int(it - rows_.begin()) - 1;
    return std::clamp(index, 0, int(rows_.size()) - 1);
}

int GridLayout::headerRow(FolderId folder) const
{
    const auto it = blocks_.constFind(folder);
    return it == blocks_.constEnd() ? -1 : it->first;
}

double GridLayout::yOfFolder(FolderId folder) const
{
    const int r = headerRow(folder);
    return r < 0 ? -1.0 : rows_[r].y;
}

int GridLayout::rowOfItem(FolderId folder, int indexInFolder) const
{
    const int r = headerRow(folder);
    if (r < 0 || indexInFolder < 0)
        return -1;
    const int row = r + 1 + indexInFolder / columns_;
    if (row >= int(rows_.size()))
        return -1;
    const GridRow& g = rows_[row];
    const bool inside = g.type == GridRow::Type::Items && g.folder == folder && indexInFolder >= g.first &&
                        indexInFolder < g.first + g.count;
    return inside ? row : -1;
}

QPair<int, int> GridLayout::block(FolderId folder) const { return blocks_.value(folder, {-1, 0}); }

} // namespace picazoo

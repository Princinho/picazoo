#include "library/Navigation.h"

namespace picazoo {

void Navigation::setOrder(const QList<ItemId>& flat)
{
    order_ = flat;
    index_.clear();
    index_.reserve(flat.size());
    for (int i = 0; i < flat.size(); ++i)
        index_.insert(flat[i], i);
}

ItemId Navigation::next(ItemId id) const
{
    const int i = indexOf(id);
    return i < 0 ? 0 : at(i + 1);
}

ItemId Navigation::previous(ItemId id) const
{
    const int i = indexOf(id);
    return i < 0 ? 0 : at(i - 1);
}

QList<ItemId> Navigation::prefetch(ItemId id, int radius) const
{
    QList<ItemId> out;
    const int i = indexOf(id);
    if (i < 0)
        return out;
    for (int d = 1; d <= radius; ++d) {
        if (const ItemId n = at(i + d))
            out << n;
        if (const ItemId p = at(i - d))
            out << p;
    }
    return out;
}

} // namespace picazoo

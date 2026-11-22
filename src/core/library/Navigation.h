#pragma once

// Ordre de navigation de la visionneuse (US2) : précédent / suivant à travers les
// dossiers, préchargement ±1 et ±2.

#include "library/Types.h"

#include <QHash>
#include <QList>

namespace picazoo {

class Navigation {
public:
    void setOrder(const QList<ItemId>& flat);
    int size() const { return int(order_.size()); }
    int indexOf(ItemId id) const { return index_.value(id, -1); }
    ItemId at(int index) const { return index >= 0 && index < order_.size() ? order_[index] : 0; }

    // Élément suivant / précédent ; 0 aux bornes de la bibliothèque.
    ItemId next(ItemId id) const;
    ItemId previous(ItemId id) const;
    // Éléments à précharger : +1, -1, +2, -2 (dans cet ordre de priorité).
    QList<ItemId> prefetch(ItemId id, int radius = 2) const;

private:
    QList<ItemId> order_;
    QHash<ItemId, int> index_;
};

} // namespace picazoo

#include "browse/BrowseOrder.h"

#include <QHash>
#include <QSet>

namespace picazoo {

QList<BrowseEntry> mergeOrder(const QStringList& viewOrderNames, const QList<BrowseEntry>& listing)
{
    QHash<QString, qsizetype> byName;
    byName.reserve(listing.size());
    for (qsizetype i = 0; i < listing.size(); ++i)
        byName.insert(listing[i].name.toCaseFolded(), i);

    QList<BrowseEntry> out;
    out.reserve(listing.size());
    QList<bool> used(listing.size(), false);
    for (const QString& name : viewOrderNames) {
        const auto it = byName.constFind(name.toCaseFolded());
        if (it == byName.constEnd() || used[*it])
            continue;
        used[*it] = true;
        out.push_back(listing[*it]);
    }
    for (qsizetype i = 0; i < listing.size(); ++i) {
        if (!used[i])
            out.push_back(listing[i]); // déjà en ordre naturel
    }
    return out;
}

} // namespace picazoo

#include "duplicates/KeeperRule.h"

#include <algorithm>

namespace picazoo {

int KeeperRule::compare(const CopyFacts& a, const CopyFacts& b, KeeperReason* decisive)
{
    auto decide = [&](KeeperReason r, bool aWins) {
        if (decisive)
            *decisive = r;
        return aWins ? -1 : 1;
    };
    if (a.isRaw != b.isRaw)
        return decide(KeeperReason::Raw, a.isRaw);
    if (a.pixels != b.pixels)
        return decide(KeeperReason::Resolution, a.pixels > b.pixels);
    if (a.sizeBytes != b.sizeBytes)
        return decide(KeeperReason::LessCompressed, a.sizeBytes > b.sizeBytes);
    if (a.hasTakenAt != b.hasTakenAt)
        return decide(KeeperReason::HasExif, a.hasTakenAt);
    if (a.date != b.date)
        return decide(KeeperReason::Oldest, a.date < b.date);
    if (a.path.size() != b.path.size())
        return decide(KeeperReason::Path, a.path.size() < b.path.size());
    const int byName = a.path.compare(b.path, Qt::CaseInsensitive);
    if (byName != 0)
        return decide(KeeperReason::Path, byName < 0);
    if (decisive)
        *decisive = KeeperReason::Path;
    return a.id == b.id ? 0 : (a.id < b.id ? -1 : 1);
}

void KeeperRule::sort(QList<CopyFacts>& copies)
{
    std::stable_sort(copies.begin(), copies.end(),
                     [](const CopyFacts& a, const CopyFacts& b) { return compare(a, b) < 0; });
}

KeeperReason KeeperRule::reason(const QList<CopyFacts>& sortedCopies)
{
    if (sortedCopies.size() < 2)
        return KeeperReason::Path;
    KeeperReason r = KeeperReason::Path;
    compare(sortedCopies[0], sortedCopies[1], &r);
    return r;
}

} // namespace picazoo

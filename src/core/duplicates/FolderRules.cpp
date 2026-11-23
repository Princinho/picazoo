#include "duplicates/FolderRules.h"

#include <algorithm>

namespace picazoo {

QString FolderRules::normalized(const QString& path)
{
    QString p = path;
    p.replace(u'/', u'\\');
    while (p.size() > 3 && p.endsWith(u'\\'))
        p.chop(1);
    return p;
}

bool FolderRules::covers(const QString& rulePath, const QString& folderPath)
{
    const QString r = normalized(rulePath);
    const QString f = normalized(folderPath);
    if (r.isEmpty())
        return false;
    if (f.compare(r, Qt::CaseInsensitive) == 0)
        return true;
    const QString prefix = r.endsWith(u'\\') ? r : r + u'\\';
    return f.startsWith(prefix, Qt::CaseInsensitive);
}

FolderRules::FolderRules(QList<FolderRule> rules) : rules_(std::move(rules))
{
    std::stable_sort(rules_.begin(), rules_.end(), [](const FolderRule& a, const FolderRule& b) {
        return normalized(a.path).size() > normalized(b.path).size();
    });
}

const FolderRule* FolderRules::match(const QString& folderPath) const
{
    for (const FolderRule& r : rules_)
        if (covers(r.path, folderPath))
            return &r;
    return nullptr;
}

} // namespace picazoo

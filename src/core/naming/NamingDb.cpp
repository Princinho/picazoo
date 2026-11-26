#include "naming/NamingDb.h"

#include "index/Database.h"
#include "naming/NamingDetector.h"

#include <QDateTime>

namespace picazoo {

NamingRules NamingRules::load(Database& db)
{
    NamingRules rules;
    auto q = db.prepare("SELECT root_key, kind, target_root_key FROM naming_decision");
    while (q.step()) {
        if (q.text(1) == u"rejected")
            rules.rejected.insert(q.text(0));
        else
            rules.merged.insert(q.text(0), q.text(2));
    }
    return rules;
}

QString NamingRules::resolve(const QString& key) const
{
    QString current = key;
    QSet<QString> seen{current};
    for (auto it = merged.constFind(current); it != merged.constEnd(); it = merged.constFind(current)) {
        if (seen.contains(*it))
            break; // des fusions croisées ne doivent pas boucler
        current = *it;
        seen.insert(current);
    }
    return current;
}

void deleteNamingGroup(Database& db, NamingGroupId id)
{
    auto q = db.prepare("DELETE FROM naming_group WHERE id = ?"); // les membres suivent (cascade)
    q.bind(1, id);
    q.run();
}

bool refreshNamingGroup(Database& db, NamingGroupId id)
{
    auto group = db.prepare("SELECT member_count, confidence FROM naming_group WHERE id = ?");
    group.bind(1, id);
    if (!group.step())
        return false;
    const int oldCount = group.int32(0);
    const QString oldConfidence = group.text(1);

    QList<std::optional<DetectedRoot>> roots;
    auto q = db.prepare("SELECT i.file_name FROM naming_member m JOIN item i ON i.id = m.item_id WHERE m.group_id = ?");
    q.bind(1, id);
    while (q.step())
        roots << detectRoot(baseNameOf(q.text(0)));
    if (roots.size() < 2) {
        deleteNamingGroup(db, id);
        return true;
    }
    const QString confidence = toDb(confidenceFor(roots));
    if (roots.size() == oldCount && confidence == oldConfidence)
        return false;
    auto update = db.prepare("UPDATE naming_group SET member_count = ?, confidence = ?, updated_at = ? WHERE id = ?");
    update.bind(1, qint64(roots.size())).bind(2, confidence);
    update.bind(3, QDateTime::currentMSecsSinceEpoch()).bind(4, id);
    update.run();
    return true;
}

} // namespace picazoo

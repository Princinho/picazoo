#include "library/Settings.h"

#include "index/Database.h"

#include <QStringList>

#include <algorithm>

namespace picazoo {

namespace {

const QHash<QString, QVariant>& defaults()
{
    static const QHash<QString, QVariant> d{
        {QStringLiteral("thumb_size"), 160},
        {QStringLiteral("folder_sort"), QStringLiteral("date_desc")},
        {QStringLiteral("item_sort"), QStringLiteral("taken_asc")},
        {QStringLiteral("folder_view"), QStringLiteral("by_year")},
        {QStringLiteral("last_folder_id"), QString()},
        {QStringLiteral("onboarding_done"), 0},
        // Doublons (002)
        {QStringLiteral("duplicates_enabled"), 1},
        {QStringLiteral("duplicates_paused"), 0},
        {QStringLiteral("duplicates_level_filter"), QStringLiteral("all")},
    };
    return d;
}

const QHash<QString, QStringList>& choices()
{
    static const QHash<QString, QStringList> c{
        {QStringLiteral("folder_sort"), {QStringLiteral("date_desc"), QStringLiteral("date_asc"), QStringLiteral("name_asc")}},
        {QStringLiteral("item_sort"), {QStringLiteral("taken_asc"), QStringLiteral("name_asc")}},
        {QStringLiteral("folder_view"), {QStringLiteral("by_year"), QStringLiteral("tree")}},
        {QStringLiteral("duplicates_level_filter"),
         {QStringLiteral("all"), QStringLiteral("identical"), QStringLiteral("same_image"), QStringLiteral("similar")}},
    };
    return c;
}

} // namespace

Settings::Settings(Database& db) : db_(db)
{
    std::lock_guard dbLock(db_.mutex());
    auto q = db_.prepare("SELECT key, value FROM settings");
    while (q.step())
        cache_.insert(q.text(0), q.text(1));
}

QVariant Settings::defaultValue(const QString& key) { return defaults().value(key); }

QVariant Settings::sanitize(const QString& key, const QVariant& value)
{
    if (key == u"thumb_size") {
        bool ok = false;
        const int v = value.toInt(&ok);
        return ok ? std::clamp(v, 64, 320) : defaultValue(key);
    }
    if (key == u"onboarding_done" || key == u"duplicates_enabled" || key == u"duplicates_paused")
        return value.toInt() != 0 ? 1 : 0;
    const auto it = choices().constFind(key);
    if (it != choices().constEnd())
        return it->contains(value.toString()) ? value : defaultValue(key);
    return value;
}

QVariant Settings::value(const QString& key) const
{
    std::lock_guard lock(mutex_);
    const auto it = cache_.constFind(key);
    if (it == cache_.constEnd())
        return defaultValue(key);
    return sanitize(key, *it);
}

void Settings::setValue(const QString& key, const QVariant& value)
{
    const QString v = sanitize(key, value).toString();
    {
        std::lock_guard lock(mutex_);
        cache_.insert(key, v);
    }
    std::lock_guard dbLock(db_.mutex());
    auto q = db_.prepare("INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)");
    q.bind(1, key).bind(2, v);
    q.run();
}

} // namespace picazoo

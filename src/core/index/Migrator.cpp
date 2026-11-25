#include "index/Migrator.h"

#include "index/Database.h"

#include <QFile>

inline void initPicazooSqlResources() { Q_INIT_RESOURCE(picazoo_core_sql); }

namespace picazoo {

namespace {

int currentVersion(Database& db)
{
    auto exists = db.prepare("SELECT 1 FROM sqlite_master WHERE type='table' AND name='schema_version'");
    if (!exists.step())
        return 0;
    auto v = db.prepare("SELECT version FROM schema_version LIMIT 1");
    return v.step() ? v.int32(0) : 0;
}

// Une migration par version : 1 = schéma initial (001), 2 = doublons (002)…
QString migrationName(int version)
{
    switch (version) {
    case 1: return QStringLiteral(":/sql/001_initial.sql");
    case 2: return QStringLiteral(":/sql/002_doublons.sql");
    case 3: return QStringLiteral(":/sql/003_doublons_lot.sql");
    case 4: return QStringLiteral(":/sql/004_origine_operations.sql");
    default: return {};
    }
}

QByteArray loadSql(int version)
{
    const QString name = migrationName(version);
    QFile f(name);
    if (!f.open(QIODevice::ReadOnly))
        throw DbError("Migration introuvable : " + name.toStdString());
    return f.readAll();
}

} // namespace

int migrateLibrary(Database& db)
{
    initPicazooSqlResources();
    std::lock_guard lock(db.mutex());
    int version = currentVersion(db);
    if (version > kLibrarySchemaVersion)
        throw DbError("La bibliothèque a été créée par une version plus récente de Picazoo.");
    // Migrations non destructives, dans l'ordre ; chacune met à jour schema_version.
    while (version < kLibrarySchemaVersion) {
        Transaction tx(db);
        db.exec(loadSql(version + 1).constData());
        tx.commit();
        ++version;
    }
    return version;
}

} // namespace picazoo

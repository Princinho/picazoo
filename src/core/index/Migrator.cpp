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

QByteArray loadSql(int version)
{
    const QString name = QStringLiteral(":/sql/%1_initial.sql").arg(version, 3, 10, QLatin1Char('0'));
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
    if (version == 0) {
        Transaction tx(db);
        db.exec(loadSql(1).constData());
        tx.commit();
        version = 1;
    }
    return version;
}

} // namespace picazoo

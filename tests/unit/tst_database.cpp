// T024 — migration, contraintes et préférences.

#include "index/Database.h"
#include "index/Migrator.h"
#include "index/ThumbStore.h"
#include "library/Settings.h"
#include "support/TempTree.h"

#include <QtTest>

using namespace picazoo;

class TstDatabase : public QObject {
    Q_OBJECT

private slots:
    void migratesEmptyDatabase()
    {
        test::TempTree t;
        auto db = Database::open(t.file("library.db"));
        QCOMPARE(migrateLibrary(*db), kLibrarySchemaVersion);
        QCOMPARE(migrateLibrary(*db), kLibrarySchemaVersion); // idempotent
        for (const char* table : {"watched_root", "excluded_path", "folder", "item", "settings", "item_fingerprint",
                                  "dup_group", "dup_member", "dup_choice", "dup_decision", "file_operation",
                                  "file_operation_entry", "naming_group", "naming_member", "naming_decision",
                                  "naming_exclusion"}) {
            auto q = db->prepare("SELECT 1 FROM sqlite_master WHERE type='table' AND name=?");
            q.bind(1, QString::fromLatin1(table));
            QVERIFY2(q.step(), table);
        }
    }

    // 007 (T030) : migration 3 -> 4, origine des opérations du journal.
    void migratesOperationSource()
    {
        test::TempTree t;
        const QString path = t.file("library.db");
        {
            auto probe = Database::open(t.file("probe.db"));
            migrateLibrary(*probe); // charge les ressources SQL de la bibliothèque statique
            auto db = Database::open(path);
            std::lock_guard lock(db->mutex());
            for (const char* name : {":/sql/001_initial.sql", ":/sql/002_doublons.sql", ":/sql/003_doublons_lot.sql"}) {
                QFile f(QString::fromLatin1(name));
                QVERIFY(f.open(QIODevice::ReadOnly));
                Transaction tx(*db);
                db->exec(f.readAll().constData());
                tx.commit();
            }
            db->exec("INSERT INTO file_operation (kind, created_at, state, file_count, bytes)"
                     " VALUES ('trash', 0, 'done', 1, 10)");
        }
        auto db = Database::open(path);
        QCOMPARE(migrateLibrary(*db), kLibrarySchemaVersion);
        std::lock_guard lock(db->mutex());
        auto q = db->prepare("SELECT source FROM file_operation");
        QVERIFY(q.step());
        QCOMPARE(q.text(0), QStringLiteral("duplicates"));
        db->exec("INSERT INTO file_operation (kind, created_at, state, source) VALUES ('trash', 0, 'done', 'library')");
        db->exec("INSERT INTO file_operation (kind, created_at, state, source) VALUES ('trash', 0, 'done', 'viewer')");
        QVERIFY_THROWS_EXCEPTION(DbError, db->exec("INSERT INTO file_operation (kind, created_at, state, source)"
                                                   " VALUES ('trash', 0, 'done', 'ailleurs')"));
    }

    void enforcesUniqueAndCascade()
    {
        test::TempTree t;
        auto db = Database::open(t.file("library.db"));
        migrateLibrary(*db);
        db->exec("INSERT INTO watched_root (path, added_at) VALUES ('C:\\Photos', 0)");
        QVERIFY_THROWS_EXCEPTION(DbError, db->exec("INSERT INTO watched_root (path, added_at) VALUES ('c:\\photos', 0)"));
        db->exec("INSERT INTO excluded_path (root_id, path) VALUES (1, 'C:\\Photos\\Tmp')");
        db->exec("INSERT INTO folder (root_id, path, parent_path, name) VALUES (1, 'C:\\Photos\\A', 'C:\\Photos', 'A')");
        db->exec("INSERT INTO item (folder_id, file_name, kind, format, size_bytes, mtime, sort_date, last_seen_at)"
                 " VALUES (1, 'a.jpg', 'image', 'jpeg', 10, 0, 0, 0)");
        QVERIFY_THROWS_EXCEPTION(DbError, db->exec("INSERT INTO item (folder_id, file_name, kind, format, size_bytes,"
                                                   " mtime, sort_date, last_seen_at, thumb_state)"
                                                   " VALUES (1, 'b.jpg', 'image', 'jpeg', 1, 0, 0, 0, 'bogus')"));
        auto def = db->prepare("SELECT orientation, meta_state, thumb_state, version FROM item");
        QVERIFY(def.step());
        QCOMPARE(def.int32(0), 1);
        QCOMPARE(def.text(1), QStringLiteral("pending"));
        QCOMPARE(def.text(2), QStringLiteral("pending"));
        QCOMPARE(def.int32(3), 1);
        db->exec("DELETE FROM watched_root WHERE id=1");
        for (const char* table : {"excluded_path", "folder", "item"}) {
            auto q = db->prepare((QByteArray("SELECT COUNT(*) FROM ") + table).constData());
            QVERIFY(q.step());
            QCOMPARE(q.int64(0), 0);
        }
    }

    void settingsDefaultsAndPersistence()
    {
        test::TempTree t;
        {
            auto db = Database::open(t.file("library.db"));
            migrateLibrary(*db);
            Settings s(*db);
            QCOMPARE(s.value("thumb_size").toInt(), 160);
            QCOMPARE(s.value("folder_sort").toString(), QStringLiteral("date_desc"));
            QCOMPARE(s.value("item_sort").toString(), QStringLiteral("taken_asc"));
            QCOMPARE(s.value("folder_view").toString(), QStringLiteral("by_year"));
            QCOMPARE(s.value("onboarding_done").toInt(), 0);
            s.setValue("thumb_size", 999);
            QCOMPARE(s.value("thumb_size").toInt(), 320);
            s.setValue("folder_view", "tree");
            s.setValue("folder_sort", "nimporte");
            QCOMPARE(s.value("folder_sort").toString(), QStringLiteral("date_desc"));
        }
        auto db = Database::open(t.file("library.db"));
        Settings s(*db);
        QCOMPARE(s.value("thumb_size").toInt(), 320);
        QCOMPARE(s.value("folder_view").toString(), QStringLiteral("tree"));
    }

    void thumbStoreVersioning()
    {
        test::TempTree t;
        ThumbStore store(t.file("thumbs.db"));
        store.put({42, ThumbKind::Final, 3, QSize(10, 8), QByteArray("jpeg")});
        QVERIFY(store.get(42, ThumbKind::Final, 3).has_value());
        QVERIFY(!store.get(42, ThumbKind::Final, 4).has_value());
        QVERIFY(!store.get(42, ThumbKind::Provisional, 3).has_value());
        store.remove({42});
        QCOMPARE(store.count(), 0);
    }
};

QTEST_GUILESS_MAIN(TstDatabase)
#include "tst_database.moc"

// 007 (T031) — mise à la corbeille d'une visionneuse autonome (FileTrash) : journal dans
// library.db (créée au besoin), corbeille réelle sur dossier temporaire, fichier modifié laissé
// en place, restauration ; écriture pendant qu'une autre connexion tient la base.

#include "fileops/FileTrash.h"
#include "index/Database.h"
#include "index/Migrator.h"
#include "platform/win/Paths.h"
#include "platform/win/RecycleBin.h"
#include "support/FixtureFactory.h"
#include "support/RecycleBinCleanup.h"
#include "support/TempTree.h"

#include <windows.h>

#include <QFile>
#include <QFileInfo>
#include <QtTest>

using namespace picazoo;
using namespace picazoo::test;

class TstFileTrash : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        TempTree probe;
        if (!platform::volumeHasRecycleBin(probe.path()))
            QSKIP("Le disque des fichiers temporaires n'a pas de corbeille.");
    }

    void trashThenRestore()
    {
        TempTree data, photos;
        const QString path = photos.file("vacances.jpg");
        FixtureFactory::writeImage(path, FixtureFactory::scene(QSize(640, 480), 2));
        const QByteArray original = [&] {
            QFile f(path);
            return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
        }();

        auto db = Database::open(data.file("library.db"));
        QCOMPARE(migrateLibrary(*db), kLibrarySchemaVersion); // base neuve : la visionneuse la crée
        // Une autre connexion (l'application ouverte) tient la base pendant l'opération.
        auto other = Database::open(data.file("library.db"));
        other->exec("INSERT INTO settings (key, value) VALUES ('test', '1')");

        const auto entry = FileTrash::describe(path);
        QVERIFY(entry);
        QVERIFY(!entry->item);
        FileTrash trash(*db);
        const OperationId op = trash.trash({*entry}, OperationSource::Viewer);
        QVERIFY(op);
        QVERIFY(!QFileInfo::exists(path));
        int inBin = 0;
        for (const auto& r : platform::recycledEntries(path))
            inBin += platform::samePath(r.originalPath, path);
        QCOMPARE(inBin, 1); // dans la corbeille, pas effacé

        {
            std::lock_guard lock(db->mutex());
            auto q = db->prepare("SELECT o.state, o.source, o.file_count, e.result, e.item_id IS NULL,"
                                 " e.keeper_path IS NULL FROM file_operation o JOIN file_operation_entry e"
                                 " ON e.operation_id = o.id WHERE o.id = ?");
            q.bind(1, op);
            QVERIFY(q.step());
            QCOMPARE(q.text(0), QStringLiteral("done"));
            QCOMPARE(q.text(1), QStringLiteral("viewer"));
            QCOMPARE(q.int32(2), 1);
            QCOMPARE(q.text(3), QStringLiteral("trashed"));
            QCOMPARE(q.int32(4), 1);
            QCOMPARE(q.int32(5), 1);
        }
        other->exec("UPDATE settings SET value = '2' WHERE key = 'test'");

        int restored = 0;
        const OperationId back = trash.restore(op, &restored);
        QVERIFY(back);
        QCOMPARE(restored, 1);
        QVERIFY(QFileInfo::exists(path));
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), original);
        {
            std::lock_guard lock(db->mutex());
            auto q = db->prepare("SELECT state FROM file_operation WHERE id = ?");
            q.bind(1, op);
            QVERIFY(q.step());
            QCOMPARE(q.text(0), QStringLiteral("undone"));
            auto r = db->prepare("SELECT kind, undoes_id, source FROM file_operation WHERE id = ?");
            r.bind(1, back);
            QVERIFY(r.step());
            QCOMPARE(r.text(0), QStringLiteral("restore"));
            QCOMPARE(r.int64(1), op);
            QCOMPARE(r.text(2), QStringLiteral("viewer"));
        }
        QCOMPARE(trash.restore(op), OperationId(0)); // plus rien à restaurer
        purgeRecycledUnder(photos.path());
    }

    void changedFileIsLeftInPlace()
    {
        TempTree data, photos;
        const QString path = photos.file("modifiee.jpg");
        FixtureFactory::writeImage(path, FixtureFactory::scene(QSize(320, 240), 3));
        auto db = Database::open(data.file("library.db"));
        migrateLibrary(*db);
        const auto entry = FileTrash::describe(path, ItemId(42));
        QVERIFY(entry);
        {
            QFile f(path);
            QVERIFY(f.open(QIODevice::Append));
            f.write("modifié entre le menu et la confirmation");
        }
        FileTrash trash(*db);
        const OperationId op = trash.trash({*entry}, OperationSource::Viewer);
        QVERIFY(QFileInfo::exists(path));
        std::lock_guard lock(db->mutex());
        auto q = db->prepare("SELECT o.state, e.result, e.item_id FROM file_operation o JOIN file_operation_entry e"
                             " ON e.operation_id = o.id WHERE o.id = ?");
        q.bind(1, op);
        QVERIFY(q.step());
        QCOMPARE(q.text(0), QStringLiteral("failed"));
        QCOMPARE(q.text(1), QStringLiteral("skipped_changed"));
        QCOMPARE(q.int64(2), 42);
        purgeRecycledUnder(photos.path());
    }

    // Un chemin court (« EGNAKO~1 ») est noté sous sa forme longue, celle que la corbeille retient :
    // sans cela, la restauration ne retrouverait pas le fichier.
    void shortPathIsStoredLong()
    {
        TempTree photos;
        const QString path = photos.file("une photo assez longue.jpg");
        FixtureFactory::writeImage(path, FixtureFactory::scene(QSize(64, 48), 1));
        wchar_t buffer[MAX_PATH * 2];
        const DWORD n = GetShortPathNameW(platform::toWin32(path).c_str(), buffer, MAX_PATH * 2);
        QVERIFY(n > 0);
        const QString shortPath = QString::fromWCharArray(buffer, int(n));
        const auto entry = FileTrash::describe(shortPath);
        QVERIFY(entry);
        QVERIFY2(platform::samePath(entry->path, path), qPrintable(entry->path));
    }

    void missingFileIsNotDescribed()
    {
        TempTree photos;
        QVERIFY(!FileTrash::describe(photos.file("absente.jpg")));
        QVERIFY(!FileTrash::describe(photos.path())); // un dossier n'est pas un fichier
    }
};

QTEST_MAIN(TstFileTrash)
#include "tst_filetrash.moc"

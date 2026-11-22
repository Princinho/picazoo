// T027 — découverte : dossiers et nombres attendus, fichiers ignorés, jonction non
// suivie, racine imbriquée parcourue une seule fois, exclusion ; invariant SC-006.

#include "index/LibraryDb.h"
#include "library/Library.h"
#include "support/FixtureFactory.h"
#include "support/FsSnapshot.h"
#include "support/LibraryFixture.h"
#include "support/TempTree.h"

#include <QtTest>

using namespace picazoo;
using namespace picazoo::test;

class TstDiscovery : public QObject {
    Q_OBJECT

private slots:
    void discoversExpectedFolders()
    {
        TempTree photos;
        const QString r = photos.path();
        FixtureFactory::writeJpeg(photos.file("A/a1.jpg"));
        FixtureFactory::writeJpeg(photos.file("A/a2.JPG"));
        FixtureFactory::writeBytes(photos.file("A/Thumbs.db"), "x");
        FixtureFactory::writeBytes(photos.file("A/desktop.ini"), "x");
        FixtureFactory::writeBytes(photos.file("A/notes.txt"), "x");
        FixtureFactory::setHidden(FixtureFactory::writeJpeg(photos.file("A/cachee.jpg")));
        FixtureFactory::writeJpeg(photos.file("B/b1.jpg"));
        FixtureFactory::writePng(photos.file("B/b2.png"));
        FixtureFactory::writeJpeg(photos.file("C/c1.jpg"));
        FixtureFactory::writeBytes(photos.file("C/clip.mp4"), QByteArray(1024, '\0'));
        photos.mkdir("Vide");
        FixtureFactory::writeJpeg(photos.file("Cache/h1.jpg"));
        FixtureFactory::setHidden(photos.file("Cache"));
        FixtureFactory::writeJpeg(photos.file("Imbrique/n1.jpg"));
        FixtureFactory::writeJpeg(photos.file("Exclu/x1.jpg"));
        QVERIFY(FixtureFactory::makeJunction(photos.file("Boucle"), r));

        const FsSnapshot before = FsSnapshot::take(r);

        LibraryFixture fx;
        const RootId outer = fx.lib->addRoot(r);
        fx.lib->addRoot(photos.file("Imbrique"));
        QVERIFY(fx.idle());
        fx.lib->excludePath(outer, photos.file("Exclu"));
        QVERIFY(fx.idle());

        const QMap<QString, int> counts = fx.countsByFolder();
        QCOMPARE(counts.value("A"), 2);
        QCOMPARE(counts.value("B"), 2);
        QCOMPARE(counts.value("C"), 2);
        QCOMPARE(counts.value("Imbrique"), 1);
        QVERIFY(!counts.contains("Vide"));
        QVERIFY(!counts.contains("Cache"));
        QVERIFY(!counts.contains("Exclu"));
        QVERIFY(!counts.contains("Boucle"));
        QCOMPARE(counts.size(), 4);
        QCOMPARE(fx.lib->itemCount(), 7); // aucune image en double

        // Vignettes : photos prêtes ; la vidéo attend son lecteur (US3).
        for (const auto& f : fx.lib->folders(FolderSort::NameAsc)) {
            for (const auto& s : fx.lib->itemSummaries(f.id, ItemSort::NameAsc)) {
                if (s.kind == Kind::Video)
                    QCOMPARE(int(s.thumbState), int(ThumbState::Pending));
                else
                    QCOMPARE(int(s.thumbState), int(ThumbState::Done));
            }
        }

        fx.close();
        PICAZOO_VERIFY_UNTOUCHED(before, r);
    }

    void metadataAndSorting()
    {
        TempTree photos;
        JpegSpec late;
        late.takenAt = QDateTime(QDate(2021, 5, 2), QTime(10, 0));
        late.orientation = 6;
        late.size = QSize(80, 40);
        JpegSpec early;
        early.takenAt = QDateTime(QDate(2015, 1, 1), QTime(9, 0));
        FixtureFactory::writeJpeg(photos.file("Album/a.jpg"), late);
        FixtureFactory::writeJpeg(photos.file("Album/b.jpg"), early);
        const FsSnapshot before = FsSnapshot::take(photos.path());

        LibraryFixture fx;
        fx.lib->addRoot(photos.path());
        QVERIFY(fx.idle());
        const auto folders = fx.lib->folders(FolderSort::DateDesc);
        QCOMPARE(folders.size(), 1);
        QCOMPARE(QDateTime::fromMSecsSinceEpoch(*folders[0].date, QTimeZone::UTC).date().year(), 2015);
        const auto items = fx.lib->itemSummaries(folders[0].id, ItemSort::TakenAsc);
        QCOMPARE(items.size(), 2);
        QCOMPARE(items[0].fileName, QStringLiteral("b.jpg")); // le plus ancien d'abord
        const auto a = fx.lib->item(items[1].id);
        QVERIFY(a);
        QCOMPARE(a->orientation, 6);
        QCOMPARE(QSize(a->width, a->height), QSize(40, 80)); // dimensions après orientation
        const auto thumb = fx.lib->thumbnail(a->id, a->version);
        QVERIFY(thumb);
        QVERIFY(thumb->height() > thumb->width());
        fx.close();
        PICAZOO_VERIFY_UNTOUCHED(before, photos.path());
    }

    void unreadableFileDoesNotBlock()
    {
        TempTree photos;
        FixtureFactory::writeBytes(photos.file("D/casse.jpg"), QByteArray(200, 'z'));
        FixtureFactory::writeJpeg(photos.file("D/bonne.jpg"));
        LibraryFixture fx;
        fx.lib->addRoot(photos.path());
        QVERIFY(fx.idle());
        const auto folders = fx.lib->folders(FolderSort::NameAsc);
        QCOMPARE(folders.size(), 1);
        for (const auto& s : fx.lib->itemSummaries(folders[0].id, ItemSort::NameAsc)) {
            if (s.fileName == u"casse.jpg")
                QCOMPARE(int(s.thumbState), int(ThumbState::Unreadable));
            else
                QCOMPARE(int(s.thumbState), int(ThumbState::Done));
        }
    }

    void realSamples()
    {
        const QDir fixtures(QStringLiteral(PICAZOO_FIXTURES_DIR));
        const QStringList samples = fixtures.entryList({"sample.dng", "sample.cr2", "sample.nef", "sample.heic"}, QDir::Files);
        if (samples.isEmpty())
            QSKIP("Échantillons RAW/HEIC absents de tests/fixtures (voir README).");
        TempTree photos;
        photos.mkdir("R");
        for (const QString& s : samples)
            QVERIFY(QFile::copy(fixtures.filePath(s), photos.file("R/" + s)));
        LibraryFixture fx;
        fx.lib->addRoot(photos.path());
        QVERIFY(fx.idle(60000));
        const auto folders = fx.lib->folders(FolderSort::NameAsc);
        QCOMPARE(folders.size(), 1);
        for (const auto& s : fx.lib->itemSummaries(folders[0].id, ItemSort::NameAsc)) {
            QVERIFY2(s.thumbState == ThumbState::Done, qPrintable(s.fileName));
            const auto it = fx.lib->item(s.id);
            QVERIFY2(it && it->width > 0 && it->height > 0, qPrintable(s.fileName));
        }
    }
};

QTEST_MAIN(TstDiscovery)
#include "tst_discovery.moc"

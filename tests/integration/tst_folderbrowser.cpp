// T017/T018 (005) — parcours d'un dossier : ouverture immédiate, listage, ordre de
// l'Explorateur, boucle, suivi des ajouts et suppressions, lecture seule (SC-007).

#include "browse/FolderBrowser.h"
#include "support/FixtureFactory.h"
#include "support/FsSnapshot.h"
#include "support/TempTree.h"

#include <QSignalSpy>
#include <QtTest>

using namespace picazoo;
using namespace picazoo::test;

namespace {

QStringList names(const FolderBrowser& b)
{
    QStringList out;
    for (const auto& e : b.entries())
        out << e.name;
    return out;
}

void writePhotos(const TempTree& t, int count)
{
    for (int i = 1; i <= count; ++i)
        FixtureFactory::writeJpeg(t.file(QStringLiteral("photo%1.jpg").arg(i)));
}

} // namespace

class TstFolderBrowser : public QObject {
    Q_OBJECT

private slots:
    void opensImmediatelyThenLists()
    {
        TempTree t;
        writePhotos(t, 10);
        FixtureFactory::writeBytes(t.file(QStringLiteral("notes.txt")), "x");
        FixtureFactory::writeBytes(t.file(QStringLiteral("clip.mp4")), "x");
        const FsSnapshot before = FsSnapshot::take(t.path());

        FolderBrowser b;
        b.open(t.file(QStringLiteral("photo2.jpg")));
        // Avant le listage : la photo demandée seule, affichable tout de suite.
        QCOMPARE(b.count(), 1);
        QCOMPARE(b.currentPath(), t.file(QStringLiteral("photo2.jpg")));
        QVERIFY(b.waitForListing(5000));
        QCOMPARE(b.count(), 10);
        QCOMPARE(b.currentIndex(), 1);
        QCOMPARE(b.orderSource(), FolderBrowser::OrderSource::Natural);
        QVERIFY(b.next());
        QCOMPARE(b.currentPath(), t.file(QStringLiteral("photo3.jpg"))); // et pas photo10
        PICAZOO_VERIFY_UNTOUCHED(before, t.path());
    }

    // 007 (T045) : la photo mise à la corbeille quitte le parcours tout de suite ; la suivante
    // (sinon la précédente) prend sa place, sans annonce « supprimée ou déplacée ».
    void removePathShowsNeighbour()
    {
        TempTree t;
        writePhotos(t, 3);
        FolderBrowser b;
        QSignalSpy removed(&b, &FolderBrowser::currentRemoved);
        b.open(t.file(QStringLiteral("photo2.jpg")));
        QVERIFY(b.waitForListing(5000));
        QVERIFY(b.removePath(t.file(QStringLiteral("photo2.jpg"))));
        QCOMPARE(b.count(), 2);
        QCOMPARE(b.currentPath(), t.file(QStringLiteral("photo3.jpg")));
        QVERIFY(b.removePath(t.file(QStringLiteral("photo3.jpg"))));
        QCOMPARE(b.currentPath(), t.file(QStringLiteral("photo1.jpg"))); // plus de suivante : la précédente
        QVERIFY(!b.removePath(t.file(QStringLiteral("photo1.jpg"))));
        QCOMPARE(b.count(), 0);
        QCOMPARE(removed.count(), 0);
    }

    void wrapsAround()
    {
        TempTree t;
        writePhotos(t, 3);
        FolderBrowser b;
        QSignalSpy wrapped(&b, &FolderBrowser::wrapped);
        b.open(t.file(QStringLiteral("photo3.jpg")));
        QVERIFY(b.waitForListing(5000));
        QVERIFY(b.next());
        QCOMPARE(b.currentPath(), t.file(QStringLiteral("photo1.jpg")));
        QCOMPARE(wrapped.count(), 1);
        QCOMPARE(wrapped.first().first().toBool(), true);
        QVERIFY(b.previous());
        QCOMPARE(b.currentPath(), t.file(QStringLiteral("photo3.jpg")));
        b.first();
        QCOMPARE(b.currentIndex(), 0);
        b.last();
        QCOMPARE(b.currentIndex(), 2);
        QCOMPARE(b.pathAt(1), t.file(QStringLiteral("photo1.jpg")));
    }

    void followsExplorerOrderWithoutMovingThePhoto()
    {
        TempTree t;
        writePhotos(t, 4);
        FolderBrowser b;
        b.setOrderProvider([](const QString&) -> std::optional<QStringList> {
            return QStringList{"photo4.jpg", "photo3.jpg", "photo2.jpg", "photo1.jpg"};
        });
        b.open(t.file(QStringLiteral("photo3.jpg")), /*deferOrder=*/true);
        QVERIFY(b.waitForListing(5000));
        QCOMPARE(b.orderSource(), FolderBrowser::OrderSource::Natural);
        b.startOrderQuery();
        QVERIFY(b.waitForListing(5000, true));
        QCOMPARE(b.orderSource(), FolderBrowser::OrderSource::Explorer);
        QCOMPARE(names(b), (QStringList{"photo4.jpg", "photo3.jpg", "photo2.jpg", "photo1.jpg"}));
        QCOMPARE(b.currentPath(), t.file(QStringLiteral("photo3.jpg")));
        QVERIFY(b.next());
        QCOMPARE(b.currentPath(), t.file(QStringLiteral("photo2.jpg")));
    }

    void followsChangesOnDisk()
    {
        TempTree t;
        writePhotos(t, 3);
        FolderBrowser b;
        b.setWatchDebounceMs(50);
        QSignalSpy removed(&b, &FolderBrowser::currentRemoved);
        b.open(t.file(QStringLiteral("photo2.jpg")));
        QVERIFY(b.waitForListing(5000));

        FixtureFactory::writeJpeg(t.file(QStringLiteral("photo10.jpg")));
        QTRY_COMPARE_WITH_TIMEOUT(b.count(), 4, 5000);
        QCOMPARE(names(b).last(), QStringLiteral("photo10.jpg"));

        QVERIFY(QFile::remove(t.file(QStringLiteral("photo2.jpg"))));
        QTRY_COMPARE_WITH_TIMEOUT(removed.count(), 1, 5000);
        QVERIFY(b.missingCurrent());
        QVERIFY(!b.currentEntry());
        QVERIFY(b.next()); // la suivante prend sa place
        QCOMPARE(b.currentPath(), t.file(QStringLiteral("photo3.jpg")));
    }

    void unsupportedFileStaysBrowsable()
    {
        TempTree t;
        writePhotos(t, 2);
        FixtureFactory::writeBytes(t.file(QStringLiteral("dessin.psd")), "x");
        FolderBrowser b;
        b.open(t.file(QStringLiteral("dessin.psd")));
        QVERIFY(b.waitForListing(5000));
        QCOMPARE(names(b), (QStringList{"dessin.psd", "photo1.jpg", "photo2.jpg"}));
        QCOMPARE(b.currentIndex(), 0);
    }
};

QTEST_GUILESS_MAIN(TstFolderBrowser)
#include "tst_folderbrowser.moc"

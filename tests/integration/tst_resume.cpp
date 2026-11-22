// T029 — reprise : interrompre l'analyse puis rouvrir ; aucune corruption, les
// éléments en attente sont repris, pas de nouvelle découverte complète.

#include "jobs/ThumbnailPipeline.h"
#include "library/Library.h"
#include "support/FixtureFactory.h"
#include "support/FsSnapshot.h"
#include "support/LibraryFixture.h"
#include "support/TempTree.h"

#include <QtTest>

using namespace picazoo;
using namespace picazoo::test;

class TstResume : public QObject {
    Q_OBJECT

private:
    static int countState(Library& lib, ThumbState state)
    {
        int n = 0;
        for (const auto& f : lib.folders(FolderSort::NameAsc))
            for (const auto& s : lib.itemSummaries(f.id, ItemSort::NameAsc))
                n += s.thumbState == state ? 1 : 0;
        return n;
    }

private slots:
    void resumesPendingThumbnails()
    {
        TempTree photos;
        JpegSpec spec;
        spec.size = QSize(1600, 1200);
        for (int i = 0; i < 40; ++i)
            FixtureFactory::writeJpeg(photos.file(QStringLiteral("P/%1.jpg").arg(i, 3, 10, QLatin1Char('0'))), spec);
        const FsSnapshot before = FsSnapshot::take(photos.path());

        LibraryFixture fx;
        fx.lib->pauseScan(); // les vignettes attendent ; la découverte se met aussi en pause…
        fx.lib->resumeScan();
        fx.lib->pipeline().pause(); // …on ne bloque que les vignettes
        fx.lib->addRoot(photos.path());
        QVERIFY(fx.idle(30000, false));
        QCOMPARE(fx.lib->itemCount(), 40);
        const auto scannedAt = fx.lib->roots().first().lastScanAt;
        QVERIFY(scannedAt.has_value());
        QVERIFY(countState(*fx.lib, ThumbState::Pending) > 0);

        QSet<ItemId> idsBefore;
        for (const auto& f : fx.lib->folders(FolderSort::NameAsc))
            for (ItemId id : fx.lib->items(f.id, ItemSort::NameAsc))
                idsBefore.insert(id);

        fx.reopen(); // fermeture en pleine analyse
        // Affichable immédiatement, depuis l'index, avant tout parcours du disque.
        QCOMPARE(fx.lib->itemCount(), 40);
        QCOMPARE(fx.lib->folders(FolderSort::NameAsc).size(), 1);
        QVERIFY(fx.idle(60000));
        QCOMPARE(countState(*fx.lib, ThumbState::Done), 40);
        // Le rattrapage en arrière-plan n'a recréé aucun élément.
        QSet<ItemId> idsAfter;
        for (const auto& f : fx.lib->folders(FolderSort::NameAsc))
            for (ItemId id : fx.lib->items(f.id, ItemSort::NameAsc))
                idsAfter.insert(id);
        QCOMPARE(idsAfter, idsBefore);
        QVERIFY(*fx.lib->roots().first().lastScanAt >= *scannedAt);
        fx.close();
        PICAZOO_VERIFY_UNTOUCHED(before, photos.path());
    }

    void resumesInterruptedDiscovery()
    {
        TempTree photos;
        for (int d = 0; d < 20; ++d)
            for (int i = 0; i < 5; ++i)
                FixtureFactory::writeJpeg(photos.file(QStringLiteral("D%1/%2.jpg").arg(d).arg(i)));
        LibraryFixture fx;
        fx.lib->addRoot(photos.path());
        fx.reopen(); // la découverte n'a pas pu se terminer
        QVERIFY(fx.idle(60000));
        QCOMPARE(fx.lib->itemCount(), 100);
        QVERIFY(fx.lib->roots().first().lastScanAt.has_value());
    }
};

QTEST_MAIN(TstResume)
#include "tst_resume.moc"

// T064 — rattrapage, application fermée : règles 1 à 5 de data-model.md.

#include "library/Library.h"
#include "support/FixtureFactory.h"
#include "support/FsSnapshot.h"
#include "support/LibraryFixture.h"
#include "support/TempTree.h"

#include <QtTest>

using namespace picazoo;
using namespace picazoo::test;

namespace {

std::optional<Item> findByName(Library& lib, const QString& name)
{
    for (const auto& f : lib.folders(FolderSort::NameAsc))
        for (const auto& s : lib.itemSummaries(f.id, ItemSort::NameAsc))
            if (s.fileName == name)
                return lib.item(s.id);
    return std::nullopt;
}

} // namespace

class TstReconcile : public QObject {
    Q_OBJECT

private slots:
    void rulesWhileClosed()
    {
        TempTree photos;
        JpegSpec a, b, c, d, e;
        a.top = Qt::yellow; b.top = Qt::cyan; c.top = Qt::magenta; d.top = Qt::darkGreen; e.top = Qt::gray;
        FixtureFactory::writeJpeg(photos.file("R1/A/inchangee.jpg"), a);
        FixtureFactory::writeJpeg(photos.file("R1/A/modifiee.jpg"), b);
        FixtureFactory::writeJpeg(photos.file("R1/A/renommee.jpg"), c);
        FixtureFactory::writeJpeg(photos.file("R1/A/deplacee.jpg"), d);
        FixtureFactory::writeJpeg(photos.file("R1/A/supprimee.jpg"), e);
        photos.mkdir("R2/B");

        LibraryFixture fx;
        fx.lib->addRoot(photos.file("R1"));
        fx.lib->addRoot(photos.file("R2"));
        QVERIFY(fx.idle());
        const auto unchanged = findByName(*fx.lib, "inchangee.jpg");
        const auto modified = findByName(*fx.lib, "modifiee.jpg");
        const auto renamed = findByName(*fx.lib, "renommee.jpg");
        const auto moved = findByName(*fx.lib, "deplacee.jpg");
        QVERIFY(unchanged && modified && renamed && moved);
        fx.close();

        // Changements faits pendant que Picazoo est fermé.
        JpegSpec bigger = b;
        bigger.size = QSize(128, 96);
        FixtureFactory::writeJpeg(photos.file("R1/A/modifiee.jpg"), bigger);
        QVERIFY(QFile::rename(photos.file("R1/A/renommee.jpg"), photos.file("R1/A/nouveau nom.jpg")));
        QVERIFY(QFile::rename(photos.file("R1/A/deplacee.jpg"), photos.file("R2/B/deplacee.jpg")));
        QVERIFY(QFile::remove(photos.file("R1/A/supprimee.jpg")));
        const FsSnapshot before = FsSnapshot::take(photos.path());

        fx.open();
        QVERIFY(fx.idle());
        // Règle 1 : inchangé
        const auto u2 = findByName(*fx.lib, "inchangee.jpg");
        QVERIFY(u2);
        QCOMPARE(u2->id, unchanged->id);
        QCOMPARE(u2->version, unchanged->version);
        // Règle 2 : même chemin, contenu modifié -> version + 1, vignette refaite
        const auto m2 = findByName(*fx.lib, "modifiee.jpg");
        QVERIFY(m2);
        QCOMPARE(m2->id, modified->id);
        QCOMPARE(m2->version, modified->version + 1);
        QCOMPARE(int(m2->thumbState), int(ThumbState::Done));
        QCOMPARE(QSize(m2->width, m2->height), QSize(128, 96));
        // Règle 3 : renommage et déplacement entre racines -> même identifiant
        const auto r2 = findByName(*fx.lib, "nouveau nom.jpg");
        QVERIFY(r2);
        QCOMPARE(r2->id, renamed->id);
        QVERIFY(!findByName(*fx.lib, "renommee.jpg"));
        const auto mv2 = findByName(*fx.lib, "deplacee.jpg");
        QVERIFY(mv2);
        QCOMPARE(mv2->id, moved->id);
        QVERIFY(mv2->folderPath.endsWith(QStringLiteral("R2\\B"), Qt::CaseInsensitive));
        QVERIFY(fx.lib->thumbnail(mv2->id, mv2->version).has_value()); // vignette conservée
        // Règle 4 : supprimé
        QVERIFY(!findByName(*fx.lib, "supprimee.jpg"));
        QCOMPARE(fx.lib->itemCount(), 4);
        fx.close();
        PICAZOO_VERIFY_UNTOUCHED(before, photos.path());
    }

    void unavailableRootKeepsItems()
    {
        TempTree photos;
        FixtureFactory::writeJpeg(photos.file("Cle/a.jpg"));
        FixtureFactory::writeJpeg(photos.file("Cle/b.jpg"));
        LibraryFixture fx;
        fx.lib->addRoot(photos.file("Cle"));
        QVERIFY(fx.idle());
        QCOMPARE(fx.lib->itemCount(), 2);
        fx.close();

        // « Débrancher la clé » : le dossier racine disparaît.
        QVERIFY(QDir(photos.file("Cle")).rename(photos.file("Cle"), photos.file("Cle-ailleurs")));
        fx.open();
        QVERIFY(fx.idle());
        QCOMPARE(fx.lib->itemCount(), 2); // règle 5 : rien n'est supprimé
        QCOMPARE(int(fx.lib->roots().first().state), int(RootState::Unavailable));

        // « Rebrancher la clé »
        QVERIFY(QDir(photos.path()).rename("Cle-ailleurs", "Cle"));
        fx.lib->checkRoots();
        QVERIFY(fx.idle());
        QCOMPARE(int(fx.lib->roots().first().state), int(RootState::Available));
        QCOMPARE(fx.lib->itemCount(), 2);
    }
};

QTEST_MAIN(TstReconcile)
#include "tst_reconcile.moc"

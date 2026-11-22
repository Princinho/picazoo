// T004/T008 (005) — listage d'un dossier pour la visionneuse autonome : images
// seulement, ordre naturel, fichiers cachés, sous-dossiers, vidéos et textes écartés.

#include "browse/FolderListing.h"
#include "support/FixtureFactory.h"
#include "support/FsSnapshot.h"
#include "support/TempTree.h"

#include <QtTest>

using namespace picazoo;
using namespace picazoo::test;

class TstFolderListing : public QObject {
    Q_OBJECT

private slots:
    void naturalOrder()
    {
        QVERIFY(naturalLess(QStringLiteral("photo2.jpg"), QStringLiteral("photo10.jpg")));
        QVERIFY(naturalLess(QStringLiteral("IMG_0009.JPG"), QStringLiteral("img_0010.jpg")));
        QVERIFY(!naturalLess(QStringLiteral("b.jpg"), QStringLiteral("A.jpg")));
    }

    void listsImagesOnly()
    {
        TempTree t;
        for (int i : {10, 2, 1})
            FixtureFactory::writeJpeg(t.file(QStringLiteral("photo%1.jpg").arg(i)));
        FixtureFactory::writePng(t.file(QStringLiteral("icone.png")));
        FixtureFactory::writeBytes(t.file(QStringLiteral("brut.CR2")), "pas un vrai raw");
        FixtureFactory::writeBytes(t.file(QStringLiteral("clip.mp4")), "video");
        FixtureFactory::writeBytes(t.file(QStringLiteral("notes.txt")), "texte");
        FixtureFactory::writeBytes(t.file(QStringLiteral("Thumbs.db")), "cache");
        FixtureFactory::setHidden(FixtureFactory::writeJpeg(t.file(QStringLiteral("cachee.jpg"))));
        t.mkdir(QStringLiteral("sous.jpg")); // un dossier au nom d'image
        FixtureFactory::writeJpeg(t.file(QStringLiteral("sous.jpg/dedans.jpg")));
        const FsSnapshot before = FsSnapshot::take(t.path());

        bool ok = false;
        const QList<BrowseEntry> list = listFolderImages(t.path(), &ok);
        QVERIFY(ok);
        QStringList names;
        for (const BrowseEntry& e : list)
            names << e.name;
        QCOMPARE(names, (QStringList{QStringLiteral("brut.CR2"), QStringLiteral("icone.png"),
                                     QStringLiteral("photo1.jpg"), QStringLiteral("photo2.jpg"),
                                     QStringLiteral("photo10.jpg")}));
        QCOMPARE(list.at(2).path, t.file(QStringLiteral("photo1.jpg")));
        QVERIFY(list.at(2).size > 0);
        QVERIFY(list.at(2).modified > 0);
        PICAZOO_VERIFY_UNTOUCHED(before, t.path());
    }

    void emptyAndMissingFolders()
    {
        TempTree t;
        bool ok = false;
        QVERIFY(listFolderImages(t.path(), &ok).isEmpty());
        QVERIFY(ok);
        QVERIFY(listFolderImages(t.file(QStringLiteral("absent")), &ok).isEmpty());
        QVERIFY(!ok);
    }

    void browsableFormats()
    {
        QVERIFY(isBrowsableImage(QStringLiteral("a.heic")));
        QVERIFY(isBrowsableImage(QStringLiteral("a.NEF")));
        QVERIFY(!isBrowsableImage(QStringLiteral("a.mov")));
        QVERIFY(!isBrowsableImage(QStringLiteral("a.psd")));
    }
};

QTEST_GUILESS_MAIN(TstFolderListing)
#include "tst_folderlisting.moc"

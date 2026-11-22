// 002 — moteur de détection des doublons sur une vraie bibliothèque de test (US1, US3, US4,
// US5) : copies identiques, liens physiques, vidéos, quasi-doublons, rafales, analyse
// incrémentale, reprise sans relecture, pause, décisions retenues. Aucun fichier touché.

#include "duplicates/DuplicateEngine.h"
#include "duplicates/DuplicateStore.h"
#include "library/Library.h"
#include "platform/win/ThrottledReader.h"
#include "support/FixtureFactory.h"
#include "support/FsSnapshot.h"
#include "support/LibraryFixture.h"
#include "support/TempTree.h"

#include <windows.h>

#include <QFile>
#include <QRandomGenerator>
#include <QTransform>
#include <QtTest>

#include <memory>

using namespace picazoo;
using namespace picazoo::test;

namespace {

DuplicateEngine::Options testOptions()
{
    DuplicateEngine::Options o;
    o.respectBattery = false; // le portable de développement peut être sur batterie
    o.autoStart = false;
    return o;
}

QByteArray randomBytes(int n, quint32 seed)
{
    QByteArray b(n, Qt::Uninitialized);
    QRandomGenerator rng(seed);
    for (int i = 0; i < n; ++i)
        b[i] = char(rng.bounded(256));
    return b;
}

QSet<QString> namesIn(Library& lib, const DuplicateGroup& g)
{
    QSet<QString> out;
    for (const auto& c : g.copies)
        if (auto it = lib.item(c.item))
            out.insert(it->fileName);
    return out;
}

struct Rig {
    LibraryFixture fx;
    std::unique_ptr<DuplicateEngine> engine;

    explicit Rig(const QString& photos)
    {
        fx.lib->addRoot(photos);
        start();
    }
    void start() { engine = std::make_unique<DuplicateEngine>(*fx.lib, testOptions()); }
    bool idle(int ms = 60000) { return fx.idle(ms) && engine->waitForIdle(ms); }
    void reopen()
    {
        engine.reset();
        fx.reopen();
        start();
    }
    QList<DuplicateGroup> groups(LevelFilter f = LevelFilter::All)
    {
        QList<DuplicateGroup> out;
        for (GroupId id : engine->store().groups(f))
            out << *engine->store().group(id);
        return out;
    }
};

} // namespace

class TstDuplicatesEngine : public QObject {
    Q_OBJECT

private slots:
    void identicalCopiesAreGrouped()
    {
        TempTree photos;
        const QByteArray a = FixtureFactory::jpegOf(FixtureFactory::scene(QSize(1024, 768), 1));
        FixtureFactory::writeBytes(photos.file("P/a.jpg"), a);
        FixtureFactory::writeBytes(photos.file("Q/a_copie.jpg"), a);
        FixtureFactory::writeBytes(photos.file("R/autre_nom.jpg"), a);
        for (int i = 0; i < 10; ++i)
            FixtureFactory::writeImage(photos.file(QStringLiteral("S/%1.jpg").arg(i)),
                                       FixtureFactory::scene(QSize(1024, 768), 50 + quint32(i)));
        const FsSnapshot before = FsSnapshot::take(photos.path());

        Rig rig(photos.path());
        QVERIFY(rig.idle());
        const auto groups = rig.groups();
        QCOMPARE(groups.size(), 1);
        QCOMPARE(groups[0].level, DupLevel::Identical);
        QCOMPARE(groups[0].copies.size(), 3);
        QCOMPARE(namesIn(*rig.fx.lib, groups[0]), (QSet<QString>{"a.jpg", "a_copie.jpg", "autre_nom.jpg"}));
        QCOMPARE(groups[0].reclaimableBytes, 2 * a.size());
        QCOMPARE(rig.engine->store().summary().groups, 1);
        int kept = 0;
        for (const auto& c : groups[0].copies)
            kept += c.keep;
        QCOMPARE(kept, 1);
        QVERIFY(groups[0].copies.first().isCenter);
        rig.engine.reset();
        rig.fx.close();
        PICAZOO_VERIFY_UNTOUCHED(before, photos.path());
    }

    void sameSizeDifferentContentIsNotIdentical()
    {
        TempTree photos;
        // Même taille, même début et même fin (même empreinte rapide), contenu différent au milieu.
        QByteArray x = FixtureFactory::jpegOf(FixtureFactory::scene(QSize(640, 480), 2)) + QByteArray(400 * 1024, '\0');
        QByteArray y = x;
        y[x.size() - 200 * 1024] = 1;
        FixtureFactory::writeBytes(photos.file("x.jpg"), x);
        FixtureFactory::writeBytes(photos.file("y.jpg"), y);
        Rig rig(photos.path());
        QVERIFY(rig.idle());
        QCOMPARE(rig.groups(LevelFilter::Identical).size(), 0);
    }

    void hardLinksAreOneFile()
    {
        TempTree photos;
        const QString a = FixtureFactory::writeImage(photos.file("a.jpg"), FixtureFactory::scene(QSize(640, 480), 3));
        QVERIFY(CreateHardLinkW(reinterpret_cast<const wchar_t*>(photos.file("lien.jpg").utf16()),
                                reinterpret_cast<const wchar_t*>(a.utf16()), nullptr));
        Rig rig(photos.path());
        QVERIFY(rig.idle());
        QCOMPARE(rig.groups().size(), 0);
    }

    void identicalVideosAreGrouped()
    {
        TempTree photos;
        const QByteArray v = randomBytes(300 * 1024, 4);
        FixtureFactory::writeBytes(photos.file("V/film.mp4"), v);
        FixtureFactory::writeBytes(photos.file("W/film (1).mp4"), v);
        FixtureFactory::writeBytes(photos.file("W/autre.mp4"), randomBytes(300 * 1024, 5));
        Rig rig(photos.path());
        QVERIFY(rig.idle());
        const auto groups = rig.groups();
        QCOMPARE(groups.size(), 1);
        QCOMPARE(groups[0].level, DupLevel::Identical);
        QCOMPARE(namesIn(*rig.fx.lib, groups[0]), (QSet<QString>{"film.mp4", "film (1).mp4"}));
    }

    void nearDuplicatesAndBursts()
    {
        TempTree photos;
        const QImage original = FixtureFactory::scene(QSize(1600, 1200), 9);
        JpegSpec exif;
        exif.takenAt = QDateTime(QDate(2024, 7, 14), QTime(10, 0, 0));
        FixtureFactory::writeImage(photos.file("A/original.jpg"), original, exif, 95);
        FixtureFactory::writeImage(photos.file("B/reduite.jpg"), original.scaled(800, 600, Qt::IgnoreAspectRatio, Qt::SmoothTransformation), exif, 90);
        FixtureFactory::writeImage(photos.file("B/recompressee.jpg"), original, {}, 55);
        FixtureFactory::writeImagePng(photos.file("C/convertie.png"), original.scaled(1200, 900, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        FixtureFactory::writeImage(photos.file("C/tournee.jpg"), original.transformed(QTransform().rotate(90)), {}, 90);
        QImage light = original;
        for (int y = 0; y < light.height(); ++y) {
            QRgb* line = reinterpret_cast<QRgb*>(light.scanLine(y));
            for (int x = 0; x < light.width(); ++x)
                line[x] = qRgb(std::min(255, qRed(line[x]) + 25), std::min(255, qGreen(line[x]) + 25),
                               std::min(255, qBlue(line[x]) + 25));
        }
        FixtureFactory::writeImage(photos.file("C/eclaircie.jpg"), light, {}, 90);
        // Rafale : même scène, sujet qui bouge, une seconde d'écart.
        for (int i = 0; i < 4; ++i) {
            JpegSpec burst;
            burst.takenAt = QDateTime(QDate(2024, 7, 15), QTime(9, 0, i));
            FixtureFactory::writeImage(photos.file(QStringLiteral("D/rafale_%1.jpg").arg(i)),
                                       FixtureFactory::scene(QSize(1600, 1200), 77, i * 3), burst);
        }
        for (int i = 0; i < 8; ++i)
            FixtureFactory::writeImage(photos.file(QStringLiteral("E/%1.jpg").arg(i)),
                                       FixtureFactory::scene(QSize(1600, 1200), 300 + quint32(i)));
        QImage flat(800, 600, QImage::Format_RGB32);
        flat.fill(QColor(10, 10, 10));
        FixtureFactory::writeImage(photos.file("F/noire1.jpg"), flat, {}, 90);
        FixtureFactory::writeImage(photos.file("F/noire2.jpg"), flat, {}, 70);
        const FsSnapshot before = FsSnapshot::take(photos.path());

        Rig rig(photos.path());
        QVERIFY(rig.idle(120000));
        const auto groups = rig.groups();
        for (const auto& g : groups) {
            QStringList names;
            for (const auto& c : g.copies)
                names << QStringLiteral("%1[%2 %3 r%4]").arg(rig.fx.lib->item(c.item)->fileName, levelLabel(c.level))
                             .arg(c.similarity.value_or(1), 0, 'f', 3).arg(c.rotation);
            qInfo() << "Groupe" << g.id << names.join(u", ");
        }
        QCOMPARE(groups.size(), 1);
        const DuplicateGroup& g = groups[0];
        QCOMPARE(namesIn(*rig.fx.lib, g), (QSet<QString>{"original.jpg", "reduite.jpg", "recompressee.jpg",
                                                         "convertie.png", "tournee.jpg", "eclaircie.jpg"}));
        QCOMPARE(rig.fx.lib->item(g.center)->fileName, QStringLiteral("original.jpg"));
        // L'original l'emporte sur la copie recompressée de même résolution : moins compressé.
        QCOMPARE(g.reason, KeeperReason::LessCompressed);
        for (const auto& c : g.copies) {
            const QString name = rig.fx.lib->item(c.item)->fileName;
            if (name == u"eclaircie.jpg")
                QCOMPARE(c.level, DupLevel::Similar);
            else if (!c.isCenter)
                QVERIFY2(c.level == DupLevel::SameImage, qPrintable(name));
            if (name == u"tournee.jpg")
                QCOMPARE(c.rotation, 270);
        }
        QCOMPARE(g.level, DupLevel::Similar);
        QCOMPARE(rig.groups(LevelFilter::SameImage).size(), 0);
        QCOMPARE(rig.groups(LevelFilter::Similar).size(), 1);
        rig.engine.reset();
        rig.fx.close();
        PICAZOO_VERIFY_UNTOUCHED(before, photos.path());
    }

    void incrementalCopyJoinsGroup()
    {
        TempTree photos;
        const QByteArray a = FixtureFactory::jpegOf(FixtureFactory::scene(QSize(800, 600), 31));
        FixtureFactory::writeBytes(photos.file("a.jpg"), a);
        FixtureFactory::writeBytes(photos.file("b.jpg"), a);
        Rig rig(photos.path());
        QVERIFY(rig.idle());
        QCOMPARE(rig.groups().size(), 1);
        const qint64 filesBefore = platform::readCounters().files.load();

        FixtureFactory::writeBytes(photos.file("Nouveau/c.jpg"), a);
        rig.fx.lib->startScan();
        QVERIFY(rig.idle());
        const auto groups = rig.groups();
        QCOMPARE(groups.size(), 1);
        QCOMPARE(groups[0].copies.size(), 3);
        // Seule la nouvelle copie a été lue (plus, au plus, ses deux voisines déjà connues : 0).
        QVERIFY2(platform::readCounters().files.load() - filesBefore <= 1,
                 qPrintable(QString::number(platform::readCounters().files.load() - filesBefore)));
    }

    void resumeDoesNotReread()
    {
        TempTree photos;
        for (int i = 0; i < 6; ++i) {
            const QByteArray a = FixtureFactory::jpegOf(FixtureFactory::scene(QSize(640, 480), 40 + quint32(i)));
            FixtureFactory::writeBytes(photos.file(QStringLiteral("X/%1.jpg").arg(i)), a);
            FixtureFactory::writeBytes(photos.file(QStringLiteral("Y/%1.jpg").arg(i)), a);
        }
        Rig rig(photos.path());
        QVERIFY(rig.idle());
        QCOMPARE(rig.groups().size(), 6);
        const qint64 filesBefore = platform::readCounters().files.load();
        rig.reopen();
        QVERIFY(rig.idle());
        QCOMPARE(rig.groups().size(), 6);
        QCOMPARE(platform::readCounters().files.load(), filesBefore); // SC-007
    }

    void pauseStopsReading()
    {
        TempTree photos;
        const QByteArray a = FixtureFactory::jpegOf(FixtureFactory::scene(QSize(640, 480), 60));
        FixtureFactory::writeBytes(photos.file("a.jpg"), a);
        FixtureFactory::writeBytes(photos.file("b.jpg"), a);
        LibraryFixture fx;
        DuplicateEngine engine(*fx.lib, testOptions());
        engine.pause();
        const qint64 filesBefore = platform::readCounters().files.load();
        fx.lib->addRoot(photos.path());
        QVERIFY(fx.idle());
        QTest::qWait(1500);
        QCOMPARE(platform::readCounters().files.load(), filesBefore);
        QCOMPARE(engine.store().summary().groups, 0);
        QCOMPARE(engine.progress().reason, QStringLiteral("user_paused"));
        engine.resume();
        QVERIFY(engine.waitForIdle(60000));
        QCOMPARE(engine.store().summary().groups, 1);
    }

    void decisionsArePersistent()
    {
        TempTree photos;
        const QByteArray a = FixtureFactory::jpegOf(FixtureFactory::scene(QSize(640, 480), 70));
        const QByteArray b = FixtureFactory::jpegOf(FixtureFactory::scene(QSize(640, 480), 71));
        FixtureFactory::writeBytes(photos.file("a1.jpg"), a);
        FixtureFactory::writeBytes(photos.file("a2.jpg"), a);
        FixtureFactory::writeBytes(photos.file("b1.jpg"), b);
        FixtureFactory::writeBytes(photos.file("b2.jpg"), b);
        FixtureFactory::writeBytes(photos.file("b3.jpg"), b);
        Rig rig(photos.path());
        QVERIFY(rig.idle());
        QCOMPARE(rig.groups().size(), 2);

        // « Ce ne sont pas des doublons » sur le groupe A.
        GroupId groupA = 0, groupB = 0;
        for (const auto& g : rig.groups())
            (namesIn(*rig.fx.lib, g).contains("a1.jpg") ? groupA : groupB) = g.id;
        rig.engine->store().markNotDuplicates(groupA);
        QVERIFY(rig.idle());
        QCOMPARE(rig.groups().size(), 1);

        // Copie gardée choisie par l'utilisateur ; refus d'écarter la dernière copie gardée.
        auto gb = *rig.engine->store().group(groupB);
        ItemId other = 0;
        for (const auto& c : gb.copies)
            if (!c.isCenter)
                other = c.item;
        QVERIFY(rig.engine->store().setKeep(groupB, other, true));
        QVERIFY(rig.engine->store().setKeep(groupB, gb.center, false));
        gb = *rig.engine->store().group(groupB);
        int kept = 0;
        for (const auto& c : gb.copies)
            kept += c.keep;
        QCOMPARE(kept, 1);
        QVERIFY(!rig.engine->store().setKeep(groupB, other, false)); // FR-017

        rig.reopen();
        rig.engine->rescanAll();
        QVERIFY(rig.idle());
        QCOMPARE(rig.groups().size(), 1); // A ne revient pas
        gb = *rig.engine->store().group(rig.groups().first().id);
        for (const auto& c : gb.copies)
            QCOMPARE(c.keep, c.item == other);

        // « Garder toutes » masque le groupe B ; annuler les décisions fait revenir A.
        rig.engine->store().keepAll(gb.id);
        QVERIFY(rig.idle());
        QCOMPARE(rig.groups().size(), 0);
        QList<ItemId> all;
        for (const auto& f : rig.fx.lib->folders(FolderSort::NameAsc))
            all += rig.fx.lib->items(f.id, ItemSort::NameAsc);
        rig.engine->store().undoDecisions(all);
        QVERIFY(rig.idle());
        QCOMPARE(rig.groups().size(), 2);
    }

    void disabledEngineDoesNothing()
    {
        TempTree photos;
        const QByteArray a = FixtureFactory::jpegOf(FixtureFactory::scene(QSize(640, 480), 80));
        FixtureFactory::writeBytes(photos.file("a.jpg"), a);
        FixtureFactory::writeBytes(photos.file("b.jpg"), a);
        LibraryFixture fx;
        DuplicateEngine engine(*fx.lib, testOptions());
        engine.setEnabled(false);
        fx.lib->addRoot(photos.path());
        QVERIFY(fx.idle());
        QTest::qWait(1000);
        QCOMPARE(engine.store().summary().groups, 0);
        QCOMPARE(engine.progress().reason, QStringLiteral("disabled"));
    }
};

QTEST_MAIN(TstDuplicatesEngine)
#include "tst_duplicates_engine.moc"

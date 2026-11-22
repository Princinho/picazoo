// T058 — format de durée ; VideoProbe sur les échantillons MP4 et MKV ; vidéo dans la bibliothèque.

#include "library/Library.h"
#include "support/FsSnapshot.h"
#include "support/LibraryFixture.h"
#include "support/TempTree.h"
#include "video/VideoProbe.h"

#include <QtTest>

using namespace picazoo;
using namespace picazoo::test;

class TstVideo : public QObject {
    Q_OBJECT

private:
    static QString sample(const QString& name)
    {
        const QString p = QDir(QStringLiteral(PICAZOO_FIXTURES_DIR)).filePath(name);
        return QFile::exists(p) ? QDir::toNativeSeparators(p) : QString();
    }

private slots:
    void durationFormat()
    {
        QCOMPARE(formatDuration(0), QStringLiteral("0:00"));
        QCOMPARE(formatDuration(59'000), QStringLiteral("0:59"));
        QCOMPARE(formatDuration(61'000), QStringLiteral("1:01"));
        QCOMPARE(formatDuration(3'600'000), QStringLiteral("1:00:00"));
        QCOMPARE(formatDuration(3'723'400), QStringLiteral("1:02:03"));
    }

    void probeSamples_data()
    {
        QTest::addColumn<QString>("file");
        QTest::newRow("mp4") << "sample.mp4";
        QTest::newRow("mkv") << "sample.mkv";
    }

    void probeSamples()
    {
        QFETCH(QString, file);
        const QString path = sample(file);
        if (path.isEmpty())
            QSKIP("Échantillon vidéo absent de tests/fixtures (voir README).");
        VideoProbe probe;
        const auto info = probe.probe(path, 10000);
        QVERIFY(info.has_value());
        QVERIFY(!info->frame.isNull());
        QVERIFY(info->durationMs.has_value());
        QVERIFY(*info->durationMs > 500);
    }

    void unreadableVideo()
    {
        TempTree t;
        QFile f(t.file("faux.mp4"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArray(4096, 'x'));
        f.close();
        VideoProbe probe;
        QVERIFY(!probe.probe(t.file("faux.mp4"), 3000).has_value());
    }

    void videoInLibrary()
    {
        const QString path = sample("sample.mp4");
        if (path.isEmpty())
            QSKIP("Échantillon vidéo absent de tests/fixtures (voir README).");
        TempTree photos;
        photos.mkdir("V");
        QVERIFY(QFile::copy(path, photos.file("V/clip.mp4")));
        const FsSnapshot before = FsSnapshot::take(photos.path());
        LibraryFixture fx;
        fx.lib->enableVideoThumbnails();
        fx.lib->addRoot(photos.path());
        QVERIFY(fx.idle(60000));
        const auto folders = fx.lib->folders(FolderSort::NameAsc);
        QCOMPARE(folders.size(), 1);
        const auto items = fx.lib->itemSummaries(folders[0].id, ItemSort::NameAsc);
        QCOMPARE(items.size(), 1);
        QCOMPARE(int(items[0].thumbState), int(ThumbState::Done));
        QVERIFY(items[0].durationMs.has_value());
        QVERIFY(fx.lib->thumbnail(items[0].id, items[0].version).has_value());
        fx.close();
        PICAZOO_VERIFY_UNTOUCHED(before, photos.path());
    }
};

QTEST_MAIN(TstVideo)
#include "tst_video.moc"

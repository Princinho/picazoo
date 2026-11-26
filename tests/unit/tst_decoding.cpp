// T025 — formats, facteurs de réduction, orientation, EXIF.

#include "jobs/ThumbnailPipeline.h" // kThumbSize
#include "media/Decoder.h"
#include "media/Formats.h"
#include "media/JpegDecoder.h"
#include "media/Metadata.h"
#include "support/FixtureFactory.h"
#include "support/TempTree.h"

#include <QDir>
#include <QtTest>

using namespace picazoo;
using test::FixtureFactory;
using test::JpegSpec;

class TstDecoding : public QObject {
    Q_OBJECT

private slots:
    void formatTable_data()
    {
        QTest::addColumn<QString>("path");
        QTest::addColumn<int>("format");
        QTest::addColumn<int>("kind");
        QTest::newRow("jpg") << "C:\\a\\b.JPG" << int(Format::Jpeg) << int(Kind::Image);
        QTest::newRow("jpe") << "x.jpe" << int(Format::Jpeg) << int(Kind::Image);
        QTest::newRow("tif") << "x.Tif" << int(Format::Tiff) << int(Kind::Image);
        QTest::newRow("heif") << "x.heif" << int(Format::Heic) << int(Kind::Image);
        QTest::newRow("cr3") << "x.CR3" << int(Format::Cr3) << int(Kind::Raw);
        QTest::newRow("dng") << "x.dng" << int(Format::Dng) << int(Kind::Raw);
        QTest::newRow("mkv") << "x.mkv" << int(Format::Mkv) << int(Kind::Video);
        QTest::newRow("txt") << "x.txt" << int(Format::Unknown) << int(Kind::Image);
        QTest::newRow("no-ext") << "C:\\dir.jpg\\file" << int(Format::Unknown) << int(Kind::Image);
    }

    void formatTable()
    {
        QFETCH(QString, path);
        QFETCH(int, format);
        QFETCH(int, kind);
        const Format f = formatFromPath(path);
        QCOMPARE(int(f), format);
        QCOMPARE(int(kindOf(f)), kind);
        if (f != Format::Unknown)
            QCOMPARE(int(formatFromName(formatName(f))), int(f));
    }

    void scaleFactor()
    {
        QCOMPARE(JpegDecoder::scaleDenominator(QSize(6000, 4000), QSize(1920, 1080)), 2);
        QCOMPARE(JpegDecoder::scaleDenominator(QSize(6000, 4000), QSize(320, 320)), 8);
        QCOMPARE(JpegDecoder::scaleDenominator(QSize(6000, 4000), QSize(700, 700)), 8);
        QCOMPARE(JpegDecoder::scaleDenominator(QSize(6000, 4000), QSize(800, 800)), 4);
        QCOMPARE(JpegDecoder::scaleDenominator(QSize(1000, 800), QSize(2000, 2000)), 1);
        QCOMPARE(JpegDecoder::scaleDenominator(QSize(1000, 800), QSize()), 1);
    }

    void orientations_data()
    {
        QTest::addColumn<int>("orientation");
        QTest::addColumn<bool>("transposed");
        QTest::addColumn<QString>("redSide"); // où se trouve la moitié rouge (haut des pixels stockés)
        QTest::newRow("1") << 1 << false << "top";
        QTest::newRow("2") << 2 << false << "top";
        QTest::newRow("3") << 3 << false << "bottom";
        QTest::newRow("4") << 4 << false << "bottom";
        QTest::newRow("5") << 5 << true << "left";
        QTest::newRow("6") << 6 << true << "right";
        QTest::newRow("7") << 7 << true << "right";
        QTest::newRow("8") << 8 << true << "left";
    }

    void orientations()
    {
        QFETCH(int, orientation);
        QFETCH(bool, transposed);
        QFETCH(QString, redSide);
        test::TempTree t;
        JpegSpec spec;
        spec.size = QSize(64, 32);
        spec.orientation = orientation;
        const QString path = FixtureFactory::writeJpeg(t.file("o.jpg"), spec);

        const Metadata m = readJpegMetadata(path);
        QVERIFY(m.ok);
        QCOMPARE(m.orientation, orientation);
        QCOMPARE(m.size, QSize(64, 32));

        DecodeStatus status{};
        auto r = Decoder::decode({path, Format::Jpeg, {}, orientation}, &status);
        QVERIFY(r);
        QCOMPARE(int(status), int(DecodeStatus::Ok));
        const QSize expected = transposed ? QSize(32, 64) : QSize(64, 32);
        QCOMPARE(r->image.size(), expected);
        QCOMPARE(r->fullSize, expected);

        const QImage& img = r->image;
        auto isRed = [&](int x, int y) { return qRed(img.pixel(x, y)) > 200 && qBlue(img.pixel(x, y)) < 60; };
        const int w = img.width(), h = img.height();
        if (redSide == u"top")
            QVERIFY(isRed(w / 2, 2) && !isRed(w / 2, h - 3));
        else if (redSide == u"bottom")
            QVERIFY(isRed(w / 2, h - 3) && !isRed(w / 2, 2));
        else if (redSide == u"left")
            QVERIFY(isRed(2, h / 2) && !isRed(w - 3, h / 2));
        else
            QVERIFY(isRed(w - 3, h / 2) && !isRed(2, h / 2));
    }

    void exifFields()
    {
        test::TempTree t;
        JpegSpec spec;
        spec.takenAt = QDateTime(QDate(2019, 7, 14), QTime(18, 30, 5));
        spec.make = "Canon";
        spec.model = "Canon EOS 80D";
        spec.embeddedThumb = true;
        const QString path = FixtureFactory::writeJpeg(t.file("e.jpg"), spec);
        const Metadata m = readJpegMetadata(path);
        QVERIFY(m.ok);
        QVERIFY(m.takenAt.has_value());
        QCOMPARE(QDateTime::fromMSecsSinceEpoch(*m.takenAt, QTimeZone::UTC).toString("yyyy-MM-dd HH:mm:ss"),
                 QStringLiteral("2019-07-14 18:30:05"));
        QCOMPARE(m.camera, QStringLiteral("Canon EOS 80D"));
        QVERIFY(!m.embeddedThumb.isEmpty());
        auto thumb = Decoder::embeddedThumbnail(path, Format::Jpeg);
        QVERIFY(thumb);
        QCOMPARE(thumb->size(), QSize(16, 12));
    }

    void exifDateParsing()
    {
        QVERIFY(!parseExifDate("0000:00:00 00:00:00").has_value());
        QVERIFY(!parseExifDate("").has_value());
        QVERIFY(parseExifDate("2024:02:29 23:59:59").has_value());
    }

    void scaledDecodeAndEncode()
    {
        test::TempTree t;
        JpegSpec spec;
        spec.size = QSize(1600, 1200);
        const QString path = FixtureFactory::writeJpeg(t.file("big.jpg"), spec);
        auto r = Decoder::decode({path, Format::Jpeg, QSize(320, 320), 1});
        QVERIFY(r);
        QCOMPARE(r->image.size(), QSize(400, 300)); // 1/4, encore >= 320×240
        QCOMPARE(r->fullSize, QSize(1600, 1200));
        const QByteArray enc = JpegDecoder::encode(fitInto(r->image, QSize(320, 320)));
        auto back = JpegDecoder::decode(enc);
        QVERIFY(back);
        QCOMPARE(back->size(), QSize(320, 240));
    }

    // 008 (US1) — la miniature intégrée d'un HEIC devient la vignette finale quand elle
    // suffit, sans décoder la pleine résolution (research H1).
    void heicThumbnailBecomesFinal()
    {
        const QDir fixtures(QStringLiteral(PICAZOO_FIXTURES_DIR));
        if (!fixtures.exists(QStringLiteral("sample.heic")))
            QSKIP("sample.heic absent de tests/fixtures (voir README).");
        const QString path = fixtures.filePath(QStringLiteral("sample.heic"));

        auto embedded = Decoder::embeddedThumbnail(path, Format::Heic);
        QVERIFY(embedded);
        QVERIFY(!embedded->isNull());
        const bool goodEnough = std::max(embedded->width(), embedded->height()) >= kThumbSize - 20;
        QVERIFY2(goodEnough, "sample.heic devrait avoir une miniature intégrée d'au moins 300 px "
                             "(typique d'une photo iPhone) pour que ce test soit représentatif");

        // La miniature se décode nettement plus vite que la pleine résolution : un
        // indice indirect que `embeddedThumbnail` ne matérialise jamais l'image entière.
        QElapsedTimer t;
        t.start();
        auto thumb = Decoder::embeddedThumbnail(path, Format::Heic);
        const qint64 thumbMs = t.elapsed();
        QVERIFY(thumb);

        t.restart();
        auto full = Decoder::decode({path, Format::Heic, {}, 1});
        const qint64 fullMs = t.elapsed();
        QVERIFY(full);
        QVERIFY2(thumbMs <= fullMs,
                 qPrintable(QStringLiteral("miniature (%1 ms) pas plus rapide que la pleine résolution (%2 ms)")
                                .arg(thumbMs)
                                .arg(fullMs)));
    }

    void truncatedAndPng()
    {
        test::TempTree t;
        const QString bad = FixtureFactory::writeTruncatedJpeg(t.file("bad.jpg"));
        DecodeStatus status{};
        auto r = Decoder::decode({bad, Format::Jpeg, QSize(64, 64), 1}, &status);
        // Un JPEG tronqué peut être partiellement décodable ; il ne doit jamais faire planter.
        Q_UNUSED(r);
        const QString png = FixtureFactory::writePng(t.file("p.png"), QSize(40, 20));
        auto p = Decoder::decode({png, Format::Png, {}, 1}, &status);
        QVERIFY(p);
        QCOMPARE(p->image.size(), QSize(40, 20));
        QCOMPARE(Decoder::readMetadata(png, Format::Png).size, QSize(40, 20));
        const QString garbage = FixtureFactory::writeBytes(t.file("g.jpg"), QByteArray(100, 'x'));
        QVERIFY(!Decoder::decode({garbage, Format::Jpeg, {}, 1}, &status));
        QCOMPARE(int(status), int(DecodeStatus::Unreadable));
    }
};

QTEST_MAIN(TstDecoding)
#include "tst_decoding.moc"

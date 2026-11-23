// 002 — logique pure de la détection des doublons : régulateur d'E/S, règle de la copie à
// garder, regroupement en étoile, empreinte visuelle, vérification des paires.

#include "duplicates/Grouper.h"
#include "duplicates/KeeperRule.h"
#include "duplicates/PairVerifier.h"
#include "duplicates/VisualFingerprint.h"
#include "platform/win/IoGovernor.h"
#include "support/FixtureFactory.h"

#include <QPainter>
#include <QTransform>
#include <QtTest>

using namespace picazoo;
using namespace picazoo::test;

namespace {

QImage reencode(const QImage& img, int quality)
{
    QImage out;
    out.loadFromData(FixtureFactory::jpegOf(img, {}, quality), "JPEG");
    return out;
}

QImage brighter(const QImage& img, int delta)
{
    QImage out = img.convertToFormat(QImage::Format_RGB32);
    for (int y = 0; y < out.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(out.scanLine(y));
        for (int x = 0; x < out.width(); ++x)
            line[x] = qRgb(std::min(255, qRed(line[x]) + delta), std::min(255, qGreen(line[x]) + delta),
                           std::min(255, qBlue(line[x]) + delta));
    }
    return out;
}

QImage thumb(const QImage& img) { return img.scaled(320, 320, Qt::KeepAspectRatio, Qt::SmoothTransformation); }

CopyFacts facts(ItemId id, qint64 pixels, qint64 size, const QString& path, bool raw = false, bool exif = true,
                qint64 date = 1000)
{
    CopyFacts f;
    f.id = id;
    f.pixels = pixels;
    f.sizeBytes = size;
    f.path = path;
    f.isRaw = raw;
    f.hasTakenAt = exif;
    f.date = date;
    return f;
}

} // namespace

class TstDuplicatesCore : public QObject {
    Q_OBJECT

private slots:
    // ----------------------------------------------------------- IoGovernor (D5)
    void governorLimitsThroughput()
    {
        qint64 now = 0;
        platform::IoGovernor::Config cfg;
        cfg.initialBytesPerSec = 10.0 * 1024 * 1024;
        platform::IoGovernor g(cfg, [&] { return now; });
        qint64 waited = 0;
        for (int i = 0; i < 100; ++i) { // 100 Mo
            const qint64 w = g.reserve(1024 * 1024);
            waited += w;
            now += w + 1000; // la lecture prend 1 ms
            g.recordRead(1024 * 1024, 1000);
        }
        // À 10 Mo/s (+25 %/s au calme, plafond 32 Mo/s), 100 Mo demandent plusieurs secondes.
        QVERIFY2(now > 3'000'000, qPrintable(QString::number(now)));
    }

    void governorBacksOffOnCongestion()
    {
        qint64 now = 0;
        platform::IoGovernor g({}, [&] { return now; });
        for (int i = 0; i < 20; ++i) {
            now += 100000;
            g.recordRead(1024 * 1024, 2000); // calme : 2 ms par Mo
        }
        const double before = g.bytesPerSecond();
        now += 1000000;
        g.recordRead(1024 * 1024, 20000); // 10× plus lent : une autre application lit
        QVERIFY(g.bytesPerSecond() <= before / 2 + 1);
        for (int i = 0; i < 10; ++i) {
            now += 100000;
            g.recordRead(1024 * 1024, 30000);
        }
        QVERIFY(g.bytesPerSecond() >= platform::IoGovernor::Config{}.minBytesPerSec);
        QVERIFY(g.bytesPerSecond() < before);
    }

    void governorUnlimited()
    {
        platform::IoGovernor g;
        g.setUnlimited(true);
        QCOMPARE(g.reserve(512 * 1024 * 1024), 0);
    }

    // ----------------------------------------------------------- KeeperRule (FR-008)
    void keeperPrefersRawThenResolution()
    {
        KeeperReason r;
        QVERIFY(KeeperRule::compare(facts(1, 100, 10, "C:\\a.cr2", true), facts(2, 400, 50, "C:\\a.jpg"), &r) < 0);
        QCOMPARE(r, KeeperReason::Raw);
        QVERIFY(KeeperRule::compare(facts(1, 100, 10, "C:\\a.jpg"), facts(2, 400, 5, "C:\\b.jpg"), &r) > 0);
        QCOMPARE(r, KeeperReason::Resolution);
        QVERIFY(KeeperRule::compare(facts(1, 400, 90, "C:\\a.jpg"), facts(2, 400, 50, "C:\\b.jpg"), &r) < 0);
        QCOMPARE(r, KeeperReason::LessCompressed);
        QVERIFY(KeeperRule::compare(facts(1, 400, 50, "C:\\a.jpg", false, false), facts(2, 400, 50, "C:\\b.jpg"), &r) > 0);
        QCOMPARE(r, KeeperReason::HasExif);
        QVERIFY(KeeperRule::compare(facts(1, 400, 50, "C:\\a.jpg", false, true, 5), facts(2, 400, 50, "C:\\b.jpg", false, true, 9), &r) < 0);
        QCOMPARE(r, KeeperReason::Oldest);
        QVERIFY(KeeperRule::compare(facts(1, 400, 50, "C:\\long\\a.jpg"), facts(2, 400, 50, "C:\\a.jpg"), &r) > 0);
        QCOMPARE(r, KeeperReason::Path);
    }

    // ----------------------------------------------------------- Grouper (FR-007)
    void grouperStarHasNoChains()
    {
        QHash<ItemId, CopyFacts> f;
        f.insert(1, facts(1, 1000, 100, "C:\\1.jpg"));
        f.insert(2, facts(2, 500, 50, "C:\\2.jpg"));
        f.insert(3, facts(3, 250, 25, "C:\\3.jpg"));
        // 1–2 et 2–3 se ressemblent, mais pas 1–3 : 3 ne doit pas rejoindre le groupe de 1.
        const QList<VisualEdge> edges{{1, 2, DupLevel::SameImage, 0.97, 0}, {2, 3, DupLevel::SameImage, 0.97, 0}};
        const auto groups = Grouper::build(f, {}, edges);
        QCOMPARE(groups.size(), 1);
        QCOMPARE(groups[0].center, 1);
        QCOMPARE(groups[0].members.size(), 2);
        QCOMPARE(groups[0].reason, KeeperReason::Resolution);
    }

    void grouperMergesIdenticalSetsAndHonoursDecisions()
    {
        QHash<ItemId, CopyFacts> f;
        for (ItemId id : {1, 2, 3, 4})
            f.insert(id, facts(id, 1000, 100, QStringLiteral("C:\\%1.jpg").arg(id), false, true, id));
        f.insert(5, facts(5, 250, 30, "C:\\small.jpg"));
        const QList<QList<ItemId>> exact{{1, 2, 3}};
        const QList<VisualEdge> edges{{3, 5, DupLevel::Similar, 0.9, 90}};
        auto groups = Grouper::build(f, exact, edges);
        QCOMPARE(groups.size(), 1);
        QCOMPARE(groups[0].center, 1); // la plus ancienne des identiques
        QCOMPARE(groups[0].members.size(), 4);
        QCOMPARE(groups[0].level, DupLevel::Similar);
        QCOMPARE(groups[0].members.last().item, 5);
        QCOMPARE(groups[0].members.last().rotation, 90);

        // « Ce ne sont pas des doublons » entre 1 et 2 : 2 sort du groupe de 1.
        groups = Grouper::build(f, exact, edges, [](ItemId a, ItemId b) { return (a == 1 && b == 2) || (a == 2 && b == 1); });
        QSet<ItemId> inFirst;
        for (const auto& m : groups[0].members)
            inFirst.insert(m.item);
        QVERIFY(!inFirst.contains(2) || groups[0].center == 2);
    }

    void grouperKeepsRawOverJpeg()
    {
        // Clarification du 2026-11-22 : RAW + JPEG de la même image regroupés, RAW gardé.
        QHash<ItemId, CopyFacts> f;
        f.insert(1, facts(1, 24'000'000, 9'000'000, "C:/IMG_0042.JPG"));
        f.insert(2, facts(2, 2'000'000, 25'000'000, "C:/IMG_0042.CR2", true));
        const auto groups = Grouper::build(f, {}, {{1, 2, DupLevel::SameImage, 0.98, 0}});
        QCOMPARE(groups.size(), 1);
        QCOMPARE(groups[0].center, 2);
        QCOMPARE(groups[0].reason, KeeperReason::Raw);
        QCOMPARE(groups[0].level, DupLevel::SameImage);
    }

    // ----------------------------------------------------------- Empreinte visuelle (D2)
    void phashSurvivesResizeRecompressionAndRotation()
    {
        const QImage original = FixtureFactory::scene(QSize(1600, 1200), 7);
        const VisualPrint a = VisualFingerprint::compute(thumb(original));
        QVERIFY(a.ok);
        QVERIFY(a.detail >= VisualFingerprint::kLowDetail);

        const VisualPrint half = VisualFingerprint::compute(thumb(original.scaled(800, 600, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));
        QVERIFY(VisualFingerprint::bestDistance(a.phash[0], half.phash) <= 4);

        const VisualPrint q = VisualFingerprint::compute(thumb(reencode(original, 50)));
        QVERIFY(VisualFingerprint::bestDistance(a.phash[0], q.phash) <= 6);

        const VisualPrint rot = VisualFingerprint::compute(thumb(original.transformed(QTransform().rotate(90))));
        int rotation = -1;
        QVERIFY(VisualFingerprint::bestDistance(a.phash[0], rot.phash, &rotation) <= 4);
        QCOMPARE(rotation, 270); // tourner la copie de 270° (horaire) la ramène sur l'original
    }

    void phashSeparatesDistinctScenes()
    {
        int close = 0;
        QList<VisualPrint> prints;
        for (quint32 s = 100; s < 130; ++s)
            prints << VisualFingerprint::compute(thumb(FixtureFactory::scene(QSize(800, 600), s)));
        for (int i = 0; i < prints.size(); ++i)
            for (int j = i + 1; j < prints.size(); ++j)
                close += VisualFingerprint::bestDistance(prints[i].phash[0], prints[j].phash) <= VisualFingerprint::kMaxDistance;
        QVERIFY2(close <= 2, qPrintable(QString::number(close)));
    }

    void detailFlagsFlatImages()
    {
        QImage flat(400, 300, QImage::Format_RGB32);
        flat.fill(QColor(20, 30, 200));
        QVERIFY(VisualFingerprint::compute(flat).detail < VisualFingerprint::kLowDetail);
        QVERIFY(VisualFingerprint::compute(FixtureFactory::twoTone(QSize(400, 300), Qt::red, Qt::blue)).detail <
                VisualFingerprint::kLowDetail);
    }

    // ----------------------------------------------------------- Vérification (D4)
    void verifierQualifiesPairs()
    {
        const QImage original = FixtureFactory::scene(QSize(1600, 1200), 11);
        const VisualSample a = PairVerifier::sample(thumb(original));
        const VisualSample same = PairVerifier::sample(thumb(reencode(original.scaled(800, 600), 60)));
        PairVerdict v = PairVerifier::verify(a, same, 0, std::nullopt, std::nullopt);
        QVERIFY2(v.match && v.level == DupLevel::SameImage, qPrintable(QString::number(v.ssim)));

        const VisualSample light = PairVerifier::sample(thumb(brighter(original, 25)));
        v = PairVerifier::verify(a, light, 0, std::nullopt, std::nullopt);
        QVERIFY2(v.match && v.level == DupLevel::Similar, qPrintable(QString::number(v.ssim)));

        const VisualSample other = PairVerifier::sample(thumb(FixtureFactory::scene(QSize(1600, 1200), 12)));
        QVERIFY(!PairVerifier::verify(a, other, 0, std::nullopt, std::nullopt).match);

        // Deux prises de vue distinctes (dates EXIF différentes) ne sont jamais regroupées.
        QVERIFY(!PairVerifier::verify(a, same, 0, qint64(1000), qint64(5000)).match);
        QVERIFY(PairVerifier::verify(a, same, 0, qint64(1000), qint64(1000)).match);
    }

    void verifierHandlesRotation()
    {
        const QImage original = FixtureFactory::scene(QSize(1200, 800), 21);
        const VisualSample a = PairVerifier::sample(thumb(original));
        const VisualSample rot = PairVerifier::sample(thumb(original.transformed(QTransform().rotate(90))));
        const PairVerdict v = PairVerifier::verify(a, rot, 270, std::nullopt, std::nullopt);
        QVERIFY2(v.match && v.level == DupLevel::SameImage, qPrintable(QString::number(v.ssim)));
    }
};

QTEST_MAIN(TstDuplicatesCore)
#include "tst_duplicates_core.moc"

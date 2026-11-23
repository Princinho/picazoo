// 002 — exactitude de la détection (SC-001, SC-002) sur un jeu généré : des originaux et leurs
// copies (réduites, recompressées, converties, tournées, éclaircies, contrastées), des photos
// toutes distinctes, des rafales et des images unies. Le test échoue si un seuil n'est pas tenu.

#include "duplicates/DuplicateEngine.h"
#include "duplicates/DuplicateStore.h"
#include "library/Library.h"
#include "support/FixtureFactory.h"
#include "support/LibraryFixture.h"
#include "support/TempTree.h"

#include <QTransform>
#include <QtTest>

using namespace picazoo;
using namespace picazoo::test;

namespace {

QImage adjust(const QImage& img, double contrast, int brightness)
{
    QImage out = img.convertToFormat(QImage::Format_RGB32);
    for (int y = 0; y < out.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(out.scanLine(y));
        for (int x = 0; x < out.width(); ++x) {
            auto f = [&](int v) { return std::clamp(int((v - 128) * contrast + 128 + brightness), 0, 255); };
            line[x] = qRgb(f(qRed(line[x])), f(qGreen(line[x])), f(qBlue(line[x])));
        }
    }
    return out;
}

// Famille d'un fichier : préfixe avant le premier « _ » (o3_reduite.jpg -> o3).
QString family(const QString& fileName) { return fileName.section(u'_', 0, 0); }

} // namespace

class TstDuplicateAccuracy : public QObject {
    Q_OBJECT

private slots:
    void meetsSc001AndSc002()
    {
        TempTree photos;
        const QSize size(1200, 900);
        constexpr int kOriginals = 10;
        int variants = 0;
        for (int k = 0; k < kOriginals; ++k) {
            const QImage o = FixtureFactory::scene(size, 1000 + quint32(k));
            JpegSpec exif;
            exif.takenAt = QDateTime(QDate(2023, 5, 1 + k), QTime(12, 0, 0));
            const QString p = QStringLiteral("O%1/o%1_").arg(k);
            FixtureFactory::writeImage(photos.file(p + "original.jpg"), o, exif, 95);
            FixtureFactory::writeImage(photos.file(p + "moitie.jpg"), o.scaled(size / 2, Qt::IgnoreAspectRatio, Qt::SmoothTransformation), exif, 90);
            FixtureFactory::writeImage(photos.file(p + "quart.jpg"), o.scaled(size / 4, Qt::IgnoreAspectRatio, Qt::SmoothTransformation), {}, 85);
            FixtureFactory::writeImage(photos.file(p + "q60.jpg"), o, {}, 60);
            FixtureFactory::writeImagePng(photos.file(p + "conv.png"), o.scaled(size * 3 / 4, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
            FixtureFactory::writeImage(photos.file(p + "rot90.jpg"), o.transformed(QTransform().rotate(90)), {}, 90);
            FixtureFactory::writeImage(photos.file(p + "rot180.jpg"), o.transformed(QTransform().rotate(180)), {}, 90);
            FixtureFactory::writeImage(photos.file(p + "eclaircie.jpg"), adjust(o, 1.0, 25), {}, 90);
            FixtureFactory::writeImage(photos.file(p + "contraste.jpg"), adjust(o, 1.1, 0), {}, 90);
            variants += 8;
        }
        int distinct = 0;
        for (int i = 0; i < 40; ++i, ++distinct)
            FixtureFactory::writeImage(photos.file(QStringLiteral("D/d%1_.jpg").arg(i)), FixtureFactory::scene(size, 5000 + quint32(i)));
        for (int b = 0; b < 5; ++b)
            for (int s = 0; s < 4; ++s, ++distinct) {
                JpegSpec shot;
                shot.takenAt = QDateTime(QDate(2023, 8, 1 + b), QTime(18, 30, s));
                FixtureFactory::writeImage(photos.file(QStringLiteral("R/b%1s%2_.jpg").arg(b).arg(s)),
                                           FixtureFactory::scene(size, 7000 + quint32(b), s * 2), shot);
            }
        for (int f = 0; f < 5; ++f, ++distinct) {
            QImage flat(size, QImage::Format_RGB32);
            flat.fill(QColor::fromHsv(f * 60, 80, 120));
            FixtureFactory::writeImage(photos.file(QStringLiteral("U/u%1_.jpg").arg(f)), flat, {}, 70 + f * 5);
        }

        LibraryFixture fx;
        DuplicateEngine::Options opt;
        opt.respectBattery = false;
        opt.autoStart = false;
        DuplicateEngine engine(*fx.lib, opt);
        fx.lib->addRoot(photos.path());
        QVERIFY(fx.idle(300000));
        QVERIFY(engine.waitForIdle(300000));

        int found = 0, wrongStrong = 0, wrongSimilar = 0;
        QSet<QString> familiesSeen;
        for (GroupId gid : engine.store().groups()) {
            const auto g = *engine.store().group(gid);
            const QString centerFamily = family(fx.lib->item(g.center)->fileName);
            for (const auto& c : g.copies) {
                const QString name = fx.lib->item(c.item)->fileName;
                if (c.isCenter)
                    continue;
                if (family(name) == centerFamily && centerFamily.startsWith(u'o')) {
                    // Retrouvée seulement si elle est dans le groupe de son original.
                    found += fx.lib->item(g.center)->fileName.endsWith(u"_original.jpg") ? 1 : 0;
                } else if (c.level == DupLevel::Similar) {
                    ++wrongSimilar;
                    qWarning() << "Faux « Très ressemblante » :" << name << "avec" << fx.lib->item(g.center)->fileName;
                } else {
                    ++wrongStrong;
                    qWarning() << "Faux doublon :" << name << "avec" << fx.lib->item(g.center)->fileName;
                }
            }
        }
        // Copies manquées : où sont-elles ?
        for (const auto& f : fx.lib->folders(FolderSort::NameAsc)) {
            for (const auto& s : fx.lib->itemSummaries(f.id, ItemSort::NameAsc)) {
                if (!s.fileName.startsWith(u'o') || s.fileName.contains(u"original"))
                    continue;
                const auto gid = engine.store().groupOf(s.id);
                const QString center = gid ? fx.lib->item(engine.store().group(*gid)->center)->fileName : QString();
                if (!center.endsWith(u"_original.jpg") || family(center) != family(s.fileName))
                    qInfo() << "Manquée :" << s.fileName << "groupe :" << (gid ? center : QStringLiteral("aucun"));
            }
        }
        const double recall = double(found) / variants;
        qInfo() << "Copies retrouvées :" << found << "/" << variants << "; faux forts :" << wrongStrong
                << "; faux « Très ressemblante » :" << wrongSimilar << "/" << distinct;
        QVERIFY2(recall >= 0.95, qPrintable(QStringLiteral("SC-001 : %1 %").arg(recall * 100, 0, 'f', 1)));
        QCOMPARE(wrongStrong, 0); // SC-002
        QVERIFY2(double(wrongSimilar) / distinct < 0.01, "SC-002 : trop de « Très ressemblante » à tort");
    }
};

QTEST_MAIN(TstDuplicateAccuracy)
#include "tst_duplicate_accuracy.moc"

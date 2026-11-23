// picazoo-testdata — dossier de photos de test pour la détection des doublons (002).
//
//   picazoo-testdata --duplicates <dossier> [--library <dossier de données Picazoo>]
//
// Crée dans <dossier> un original et ses copies (identiques renommées, réduite, recompressée,
// PNG, tournée, éclaircie), une rafale de 5 photos, des images unies et 20 photos distinctes.
// Avec --library, prépare aussi une bibliothèque Picazoo analysée (PICAZOO_DATA_DIR) pour
// ouvrir directement l'écran « Doublons ». Ne touche à rien en dehors de ces deux dossiers.

#include "duplicates/DuplicateEngine.h"
#include "library/Library.h"
#include "support/FixtureFactory.h"

#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QTextStream>
#include <QTransform>

#include <cstdio>

using namespace picazoo;
using namespace picazoo::test;

namespace {

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

void generate(const QString& root)
{
    auto file = [&](const QString& rel) { return QDir::toNativeSeparators(root + u'/' + rel); };
    const QSize size(2400, 1600);
    const QImage original = FixtureFactory::scene(size, 2026);
    JpegSpec exif;
    exif.takenAt = QDateTime(QDate(2025, 8, 3), QTime(17, 42, 10));
    const QByteArray jpeg = FixtureFactory::jpegOf(original, exif, 95);
    FixtureFactory::writeBytes(file("Vacances/plage.jpg"), jpeg);
    FixtureFactory::writeBytes(file("Sauvegarde 2025/plage.jpg"), jpeg);
    FixtureFactory::writeBytes(file("Telephone/IMG_20250803_174210.jpg"), jpeg);
    FixtureFactory::writeImage(file("Envoyees/plage_petite.jpg"), original.scaled(size / 3, Qt::IgnoreAspectRatio, Qt::SmoothTransformation), exif, 85);
    FixtureFactory::writeImage(file("Envoyees/plage_compressee.jpg"), original, {}, 50);
    FixtureFactory::writeImagePng(file("Exports/plage.png"), original.scaled(size / 2, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    FixtureFactory::writeImage(file("Exports/plage_tournee.jpg"), original.transformed(QTransform().rotate(90)), {}, 90);
    FixtureFactory::writeImage(file("Retouches/plage_eclaircie.jpg"), brighter(original, 25), {}, 90);

    const QImage other = FixtureFactory::scene(size, 77);
    const QByteArray otherJpeg = FixtureFactory::jpegOf(other, {}, 92);
    FixtureFactory::writeBytes(file("Vacances/montagne.jpg"), otherJpeg);
    FixtureFactory::writeBytes(file("Vacances/montagne (1).jpg"), otherJpeg);

    for (int i = 0; i < 5; ++i) {
        JpegSpec shot;
        shot.takenAt = QDateTime(QDate(2025, 8, 4), QTime(9, 15, i));
        FixtureFactory::writeImage(file(QStringLiteral("Rafale/IMG_%1.jpg").arg(100 + i)),
                                   FixtureFactory::scene(size, 300, i * 3), shot);
    }
    for (int i = 0; i < 3; ++i) {
        QImage flat(1600, 1200, QImage::Format_RGB32);
        flat.fill(QColor(8, 8, 10));
        FixtureFactory::writeImage(file(QStringLiteral("Unies/noire_%1.jpg").arg(i)), flat, {}, 60 + i * 15);
    }
    for (int i = 0; i < 20; ++i)
        FixtureFactory::writeImage(file(QStringLiteral("Divers/photo_%1.jpg").arg(i, 2, 10, QLatin1Char('0'))),
                                   FixtureFactory::scene(QSize(1600, 1200), 5000 + quint32(i)));
}

} // namespace

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    QTextStream out(stdout);
    const QStringList args = app.arguments();
    const qsizetype d = args.indexOf(QStringLiteral("--duplicates"));
    if (d < 0 || d + 1 >= args.size()) {
        out << "Usage : picazoo-testdata --duplicates <dossier> [--library <dossier de données>]\n";
        return 2;
    }
    const QString root = QDir::cleanPath(args[d + 1]);
    if (QDir(root).exists() && !QDir(root).isEmpty()) {
        out << "Le dossier " << QDir::toNativeSeparators(root) << " n'est pas vide : choisissez un dossier neuf.\n";
        return 1;
    }
    generate(root);
    out << "Photos de test créées dans " << QDir::toNativeSeparators(root) << "\n";

    const qsizetype l = args.indexOf(QStringLiteral("--library"));
    if (l >= 0 && l + 1 < args.size()) {
        auto lib = Library::open(QDir::toNativeSeparators(args[l + 1]));
        lib->setSetting(QStringLiteral("onboarding_done"), 1);
        lib->addRoot(QDir::toNativeSeparators(root));
        DuplicateEngine::Options opt;
        opt.respectBattery = false;
        DuplicateEngine engine(*lib, opt);
        lib->waitForIdle(600000);
        engine.waitForIdle(600000);
        out << "Bibliothèque prête : " << engine.store().summary().groups << " groupes de doublons.\n";
    }
    return 0;
}

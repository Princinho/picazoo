// Génère les icônes de Picazoo à partir des dessins SVG de assets/icons :
//   - picazoo-small.svg pour 16 à 32 px, picazoo.svg au-delà ;
//   - un PNG par taille dans assets/icons/png/ ;
//   - assets/icons/picazoo.ico (images PNG, 16 à 256 px).
//
// Usage : make-icons <dossier assets/icons>

#include <QByteArray>
#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QSvgRenderer>
#include <QTextStream>

#include <cstdio>

namespace {

QImage render(QSvgRenderer& svg, int size)
{
    QImage img(size, size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    svg.render(&p, QRectF(0, 0, size, size));
    return img;
}

void put16(QByteArray& b, quint16 v)
{
    b.append(char(v & 0xFF));
    b.append(char(v >> 8));
}

void put32(QByteArray& b, quint32 v)
{
    for (int i = 0; i < 4; ++i)
        b.append(char((v >> (8 * i)) & 0xFF));
}

} // namespace

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    QTextStream out(stdout);
    if (argc < 2) {
        out << "Usage : make-icons <dossier assets/icons>\n";
        return 1;
    }
    const QDir dir(QString::fromLocal8Bit(argv[1]));
    QSvgRenderer full(dir.filePath("picazoo.svg"));
    QSvgRenderer small(dir.filePath("picazoo-small.svg"));
    if (!full.isValid() || !small.isValid()) {
        out << "Dessins SVG introuvables ou invalides dans " << dir.absolutePath() << "\n";
        return 1;
    }
    dir.mkpath("png");

    const QList<int> sizes{16, 20, 24, 32, 40, 48, 64, 128, 256};
    QList<QByteArray> pngs;
    for (int s : sizes) {
        const QImage img = render(s <= 32 ? small : full, s);
        QByteArray png;
        QBuffer buf(&png);
        buf.open(QIODevice::WriteOnly);
        img.save(&buf, "PNG");
        pngs << png;
        QFile f(dir.filePath(QStringLiteral("png/picazoo-%1.png").arg(s)));
        if (!f.open(QIODevice::WriteOnly) || f.write(png) != png.size()) {
            out << "Écriture impossible : " << f.fileName() << "\n";
            return 1;
        }
    }

    // ICO : en-tête, répertoire (16 octets par image), puis les PNG.
    QByteArray ico;
    put16(ico, 0);
    put16(ico, 1);
    put16(ico, quint16(sizes.size()));
    quint32 offset = 6 + 16 * quint32(sizes.size());
    for (qsizetype i = 0; i < sizes.size(); ++i) {
        const int s = sizes[i];
        ico.append(char(s >= 256 ? 0 : s));
        ico.append(char(s >= 256 ? 0 : s));
        ico.append(char(0)); // palette
        ico.append(char(0)); // réservé
        put16(ico, 1);       // plans
        put16(ico, 32);      // bits par pixel
        put32(ico, quint32(pngs[i].size()));
        put32(ico, offset);
        offset += quint32(pngs[i].size());
    }
    for (const QByteArray& png : pngs)
        ico += png;
    QFile f(dir.filePath("picazoo.ico"));
    if (!f.open(QIODevice::WriteOnly) || f.write(ico) != ico.size()) {
        out << "Écriture impossible : " << f.fileName() << "\n";
        return 1;
    }
    out << "Icônes générées dans " << QDir::toNativeSeparators(dir.absolutePath()) << " (" << sizes.size()
        << " tailles, " << ico.size() / 1024 << " Ko)\n";
    return 0;
}

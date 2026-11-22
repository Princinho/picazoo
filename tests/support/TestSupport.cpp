#include "support/FixtureFactory.h"
#include "support/FsSnapshot.h"
#include "support/TempTree.h"

#include "media/JpegDecoder.h"
#include "platform/win/FileIdentity.h"

#include <windows.h>

#include <QBuffer>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QPainter>
#include <QProcess>

#include <vector>

namespace picazoo::test {

// ------------------------------------------------------------------ TempTree

QString TempTree::write(const QString& rel, const QByteArray& data) const
{
    const QString p = file(rel);
    QDir().mkpath(QFileInfo(p).absolutePath());
    QFile f(p);
    if (f.open(QIODevice::WriteOnly))
        f.write(data);
    return p;
}

// ---------------------------------------------------------------- FsSnapshot

FsSnapshot FsSnapshot::take(const QString& root)
{
    FsSnapshot s;
    QDirIterator it(root, QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    const QDir base(root);
    while (it.hasNext()) {
        it.next();
        const QFileInfo fi = it.fileInfo();
        Entry e;
        e.isDir = fi.isDir();
        // La date d'un dossier n'est pas comparée : NTFS la met à jour en différé après la
        // création de son contenu. Toute création, suppression ou modification d'un fichier
        // reste détectée par les entrées de fichiers (chemin, taille, date, hachage).
        e.mtime = e.isDir ? 0 : fi.lastModified(QTimeZone::UTC).toMSecsSinceEpoch();
        if (!e.isDir && !fi.isJunction() && !fi.isSymLink()) {
            e.size = fi.size();
            e.hash = platform::fullHash(QDir::toNativeSeparators(fi.absoluteFilePath())).value_or(0);
        }
        s.entries_.insert(base.relativeFilePath(fi.absoluteFilePath()), e);
    }
    return s;
}

QStringList FsSnapshot::diff(const FsSnapshot& after) const
{
    QStringList out;
    for (auto it = entries_.cbegin(); it != entries_.cend(); ++it) {
        const auto other = after.entries_.constFind(it.key());
        if (other == after.entries_.cend())
            out << QStringLiteral("supprimé ou renommé : ") + it.key();
        else if (!(*other == *it))
            out << QStringLiteral("modifié : %1 (taille %2 -> %3, date %4 -> %5, hachage %6 -> %7)")
                       .arg(it.key())
                       .arg(it->size)
                       .arg(other->size)
                       .arg(QDateTime::fromMSecsSinceEpoch(it->mtime, QTimeZone::UTC).toString(Qt::ISODateWithMs))
                       .arg(QDateTime::fromMSecsSinceEpoch(other->mtime, QTimeZone::UTC).toString(Qt::ISODateWithMs))
                       .arg(it->hash)
                       .arg(other->hash);
    }
    for (auto it = after.entries_.cbegin(); it != after.entries_.cend(); ++it) {
        if (!entries_.contains(it.key()))
            out << QStringLiteral("créé : ") + it.key();
    }
    return out;
}

// ------------------------------------------------------------ FixtureFactory

namespace {

struct IfdEntry {
    quint16 tag;
    quint16 type; // 2 ASCII, 3 SHORT, 4 LONG
    quint32 count;
    QByteArray data; // > 4 octets : stockés après l'IFD
    quint32 value = 0;
};

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

IfdEntry ascii(quint16 tag, const QString& s)
{
    QByteArray d = s.toLatin1();
    d.append('\0');
    IfdEntry e{tag, 2, quint32(d.size()), {}};
    if (d.size() <= 4) {
        d.resize(4, '\0');
        e.value = quint8(d[0]) | (quint8(d[1]) << 8) | (quint8(d[2]) << 16) | (quint32(quint8(d[3])) << 24);
    } else {
        e.data = d;
    }
    return e;
}

// IFD + sa zone de données, placés à l'offset absolu `start` dans le bloc TIFF.
QByteArray buildIfd(quint32 start, const std::vector<IfdEntry>& entries, quint32 next)
{
    QByteArray ifd;
    QByteArray data;
    const quint32 dataStart = start + 2 + quint32(entries.size()) * 12 + 4;
    put16(ifd, quint16(entries.size()));
    for (const auto& e : entries) {
        put16(ifd, e.tag);
        put16(ifd, e.type);
        put32(ifd, e.count);
        if (!e.data.isEmpty()) {
            put32(ifd, dataStart + quint32(data.size()));
            data.append(e.data);
            if (data.size() % 2)
                data.append('\0');
        } else if (e.type == 3) {
            put16(ifd, quint16(e.value));
            put16(ifd, 0);
        } else {
            put32(ifd, e.value);
        }
    }
    put32(ifd, next);
    return ifd + data;
}

} // namespace

QImage FixtureFactory::twoTone(QSize size, QColor top, QColor bottom)
{
    QImage img(size, QImage::Format_RGB32);
    img.fill(bottom);
    QPainter p(&img);
    p.fillRect(QRect(0, 0, size.width(), size.height() / 2), top);
    return img;
}

QByteArray FixtureFactory::exifBlock(const JpegSpec& spec, const QByteArray& thumbJpeg)
{
    std::vector<IfdEntry> ifd0;
    if (!spec.make.isEmpty())
        ifd0.push_back(ascii(0x010F, spec.make));
    if (!spec.model.isEmpty())
        ifd0.push_back(ascii(0x0110, spec.model));
    ifd0.push_back(IfdEntry{0x0112, 3, 1, {}, quint32(spec.orientation)});
    const bool hasExif = spec.takenAt.has_value();
    if (hasExif)
        ifd0.push_back(IfdEntry{0x8769, 4, 1, {}, 0});

    // Première passe pour connaître la taille de l'IFD0.
    const quint32 ifd0Len = quint32(buildIfd(8, ifd0, 0).size());
    std::vector<IfdEntry> exifIfd;
    if (hasExif)
        exifIfd.push_back(ascii(0x9003, spec.takenAt->toString(QStringLiteral("yyyy:MM:dd HH:mm:ss"))));
    const quint32 exifStart = 8 + ifd0Len;
    const QByteArray exifBytes = hasExif ? buildIfd(exifStart, exifIfd, 0) : QByteArray();
    const quint32 ifd1Start = exifStart + quint32(exifBytes.size());

    QByteArray ifd1Bytes;
    if (!thumbJpeg.isEmpty()) {
        std::vector<IfdEntry> ifd1{
            IfdEntry{0x0103, 3, 1, {}, 6},
            IfdEntry{0x0201, 4, 1, {}, 0},
            IfdEntry{0x0202, 4, 1, {}, quint32(thumbJpeg.size())},
        };
        const quint32 ifd1Len = quint32(buildIfd(ifd1Start, ifd1, 0).size());
        ifd1[1].value = ifd1Start + ifd1Len;
        ifd1Bytes = buildIfd(ifd1Start, ifd1, 0) + thumbJpeg;
    }
    if (hasExif)
        ifd0.back().value = exifStart;

    QByteArray tiff("II", 2);
    put16(tiff, 42);
    put32(tiff, 8);
    tiff += buildIfd(8, ifd0, thumbJpeg.isEmpty() ? 0 : ifd1Start);
    tiff += exifBytes;
    tiff += ifd1Bytes;
    return QByteArray("Exif\0\0", 6) + tiff;
}

QByteArray FixtureFactory::jpeg(const JpegSpec& spec)
{
    const QByteArray main = JpegDecoder::encode(twoTone(spec.size, spec.top, spec.bottom), 90);
    QByteArray thumb;
    if (spec.embeddedThumb)
        thumb = JpegDecoder::encode(twoTone(QSize(16, 12), spec.top, spec.bottom), 80);
    const QByteArray exif = exifBlock(spec, thumb);
    QByteArray app1("\xFF\xE1", 2);
    const int len = int(exif.size()) + 2;
    app1.append(char(len >> 8));
    app1.append(char(len & 0xFF));
    app1 += exif;
    return main.left(2) + app1 + main.mid(2);
}

QByteArray FixtureFactory::png(QSize size, QColor color)
{
    QImage img(size, QImage::Format_RGB32);
    img.fill(color);
    QByteArray out;
    QBuffer b(&out);
    b.open(QIODevice::WriteOnly);
    img.save(&b, "PNG");
    return out;
}

QString FixtureFactory::writeBytes(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (f.open(QIODevice::WriteOnly))
        f.write(data);
    return path;
}

QString FixtureFactory::writeJpeg(const QString& path, const JpegSpec& spec) { return writeBytes(path, jpeg(spec)); }

QString FixtureFactory::writePng(const QString& path, QSize size) { return writeBytes(path, png(size)); }

QString FixtureFactory::writeTruncatedJpeg(const QString& path)
{
    JpegSpec spec;
    spec.size = QSize(256, 256);
    const QByteArray full = jpeg(spec);
    return writeBytes(path, full.left(full.size() / 3));
}

bool FixtureFactory::setHidden(const QString& path)
{
    const std::wstring w = QDir::toNativeSeparators(path).toStdWString();
    const DWORD attrs = GetFileAttributesW(w.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && SetFileAttributesW(w.c_str(), attrs | FILE_ATTRIBUTE_HIDDEN);
}

bool FixtureFactory::makeJunction(const QString& link, const QString& target)
{
    return QProcess::execute(QStringLiteral("cmd.exe"),
                             {QStringLiteral("/c"), QStringLiteral("mklink"), QStringLiteral("/J"),
                              QDir::toNativeSeparators(link), QDir::toNativeSeparators(target)}) == 0;
}

bool FixtureFactory::setMtime(const QString& path, const QDateTime& when)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadWrite))
        return false;
    return f.setFileTime(when, QFileDevice::FileModificationTime);
}

} // namespace picazoo::test

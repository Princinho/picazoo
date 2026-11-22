#include "media/Metadata.h"

#include "platform/win/FileIO.h"

#include <TinyEXIF.h>

#include <QDateTime>
#include <QTimeZone>
#include <QTransform>

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace picazoo {

namespace {

constexpr qint64 kMetadataPrefix = 256 * 1024;

quint16 rd16(const uchar* p, bool le) { return le ? quint16(p[0] | (p[1] << 8)) : quint16((p[0] << 8) | p[1]); }

quint32 rd32(const uchar* p, bool le)
{
    return le ? quint32(p[0]) | (quint32(p[1]) << 8) | (quint32(p[2]) << 16) | (quint32(p[3]) << 24)
              : (quint32(p[0]) << 24) | (quint32(p[1]) << 16) | (quint32(p[2]) << 8) | quint32(p[3]);
}

// Cherche la vignette JPEG de l'IFD1 (balises 0x0201 / 0x0202) d'un bloc TIFF.
QByteArray findIfd1Thumbnail(const uchar* tiff, size_t len)
{
    if (len < 8)
        return {};
    bool le;
    if (tiff[0] == 'I' && tiff[1] == 'I')
        le = true;
    else if (tiff[0] == 'M' && tiff[1] == 'M')
        le = false;
    else
        return {};
    quint32 ifd0 = rd32(tiff + 4, le);
    if (ifd0 + 2 > len)
        return {};
    const quint16 n0 = rd16(tiff + ifd0, le);
    const size_t next = size_t(ifd0) + 2 + size_t(n0) * 12;
    if (next + 4 > len)
        return {};
    const quint32 ifd1 = rd32(tiff + next, le);
    if (ifd1 == 0 || ifd1 + 2 > len)
        return {};
    const quint16 n1 = rd16(tiff + ifd1, le);
    quint32 offset = 0, length = 0;
    for (quint16 i = 0; i < n1; ++i) {
        const size_t e = size_t(ifd1) + 2 + size_t(i) * 12;
        if (e + 12 > len)
            return {};
        const quint16 tag = rd16(tiff + e, le);
        const quint16 type = rd16(tiff + e + 2, le);
        const quint32 value = type == 3 ? rd16(tiff + e + 8, le) : rd32(tiff + e + 8, le);
        if (tag == 0x0201)
            offset = value;
        else if (tag == 0x0202)
            length = value;
    }
    if (offset == 0 || length < 4 || size_t(offset) + length > len)
        return {};
    if (tiff[offset] != 0xFF || tiff[offset + 1] != 0xD8)
        return {};
    return QByteArray(reinterpret_cast<const char*>(tiff + offset), qsizetype(length));
}

QString cameraName(const std::string& make, const std::string& model)
{
    QString mk = QString::fromStdString(make).trimmed();
    QString md = QString::fromStdString(model).trimmed();
    if (md.startsWith(mk, Qt::CaseInsensitive))
        return md; // « Canon Canon EOS 5D » -> « Canon EOS 5D »
    if (mk.isEmpty())
        return md;
    if (md.isEmpty())
        return mk;
    return mk + u' ' + md;
}

} // namespace

std::optional<qint64> parseExifDate(const std::string& s)
{
    if (s.size() < 19)
        return std::nullopt;
    const QString q = QString::fromLatin1(s.data(), 19);
    const QDate d(q.mid(0, 4).toInt(), q.mid(5, 2).toInt(), q.mid(8, 2).toInt());
    const QTime t(q.mid(11, 2).toInt(), q.mid(14, 2).toInt(), q.mid(17, 2).toInt());
    if (!d.isValid() || !t.isValid() || d.year() < 1900)
        return std::nullopt;
    return QDateTime(d, t, QTimeZone::UTC).toMSecsSinceEpoch();
}

Metadata parseExifBlock(const QByteArray& block)
{
    Metadata m;
    QByteArray exif = block;
    if (!exif.startsWith(QByteArray("Exif\0\0", 6)))
        exif.prepend(QByteArray("Exif\0\0", 6));
    TinyEXIF::EXIFInfo info;
    const auto* data = reinterpret_cast<const uint8_t*>(exif.constData());
    if (info.parseFromEXIFSegment(data, unsigned(exif.size())) != TinyEXIF::PARSE_SUCCESS)
        return m;
    m.ok = true;
    m.takenAt = parseExifDate(info.DateTimeOriginal);
    if (!m.takenAt)
        m.takenAt = parseExifDate(info.DateTimeDigitized);
    m.orientation = (info.Orientation >= 1 && info.Orientation <= 8) ? int(info.Orientation) : 1;
    m.camera = cameraName(info.Make, info.Model);
    if (info.ImageWidth > 0 && info.ImageHeight > 0)
        m.size = QSize(int(info.ImageWidth), int(info.ImageHeight));
    m.embeddedThumb = findIfd1Thumbnail(data + 6, size_t(exif.size()) - 6);
    return m;
}

Metadata parseJpegMetadata(const QByteArray& prefix)
{
    Metadata m;
    const auto* p = reinterpret_cast<const uchar*>(prefix.constData());
    const qsizetype n = prefix.size();
    if (n < 4 || p[0] != 0xFF || p[1] != 0xD8)
        return m;
    QSize sofSize;
    qsizetype i = 2;
    while (i + 4 <= n) {
        if (p[i] != 0xFF) {
            ++i;
            continue;
        }
        const uchar marker = p[i + 1];
        if (marker == 0xFF) {
            ++i;
            continue;
        }
        if (marker == 0xD8 || (marker >= 0xD0 && marker <= 0xD7) || marker == 0x01) {
            i += 2;
            continue;
        }
        if (marker == 0xD9 || marker == 0xDA)
            break; // fin d'image ou début des données compressées
        const int segLen = (p[i + 2] << 8) | p[i + 3];
        if (segLen < 2)
            break;
        const qsizetype body = i + 4;
        const qsizetype bodyLen = segLen - 2;
        if (marker == 0xE1 && body + 6 <= n && std::memcmp(p + body, "Exif\0\0", 6) == 0 && !m.ok) {
            const qsizetype avail = std::min<qsizetype>(bodyLen, n - body);
            const Metadata exif = parseExifBlock(prefix.mid(body, avail));
            if (exif.ok)
                m = exif;
        } else if (marker >= 0xC0 && marker <= 0xCF && marker != 0xC4 && marker != 0xC8 && marker != 0xCC &&
                   body + 5 <= n) {
            sofSize = QSize((p[body + 3] << 8) | p[body + 4], (p[body + 1] << 8) | p[body + 2]);
        }
        i += 2 + segLen;
    }
    if (sofSize.isValid() && !sofSize.isEmpty())
        m.size = sofSize; // les dimensions réelles priment sur celles déclarées dans l'EXIF
    if (m.size.isValid())
        m.ok = true;
    return m;
}

Metadata readJpegMetadata(const QString& path)
{
    bool ok = false;
    const QByteArray prefix = platform::readFileRange(path, 0, kMetadataPrefix, &ok);
    if (!ok)
        return {};
    return parseJpegMetadata(prefix);
}

QSize orientedSize(QSize size, int orientation)
{
    return orientation >= 5 && orientation <= 8 ? size.transposed() : size;
}

QImage applyOrientation(QImage image, int orientation)
{
    switch (orientation) {
    case 2: return image.flipped(Qt::Horizontal);
    case 3: return image.flipped(Qt::Horizontal | Qt::Vertical);
    case 4: return image.flipped(Qt::Vertical);
    case 5: return image.transformed(QTransform().rotate(90)).flipped(Qt::Horizontal);
    case 6: return image.transformed(QTransform().rotate(90));
    case 7: return image.transformed(QTransform().rotate(-90)).flipped(Qt::Horizontal);
    case 8: return image.transformed(QTransform().rotate(-90));
    default: return image;
    }
}

} // namespace picazoo

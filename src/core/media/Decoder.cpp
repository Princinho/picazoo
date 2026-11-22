#include "media/Decoder.h"

#include "media/Formats.h"
#include "media/HeifDecoder.h"
#include "media/JpegDecoder.h"
#include "media/QtImageDecoder.h"
#include "media/RawPreview.h"
#include "platform/win/FileIO.h"

namespace picazoo {

namespace {

void setStatus(DecodeStatus* s, DecodeStatus v)
{
    if (s)
        *s = v;
}

std::optional<DecodeResult> decodeQt(const QByteArray& data, QSize target)
{
    QSize full;
    auto img = QtImageDecoder::decode(data, target, &full);
    if (!img)
        return std::nullopt;
    DecodeResult r;
    r.image = *img;
    r.fullSize = full.isValid() ? full : img->size();
    return r;
}

std::optional<DecodeResult> decodeJpeg(const DecodeRequest& req, const QByteArray& data)
{
    const int o = req.orientation;
    QSize pre = req.target;
    if (o >= 5)
        pre.transpose();
    if (auto img = JpegDecoder::decode(data, pre)) {
        DecodeResult r;
        r.image = applyOrientation(*img, o);
        if (auto h = JpegDecoder::readHeader(data))
            r.fullSize = orientedSize(h->size, o);
        return r;
    }
    // CMYK, JPEG progressif corrompu… : QImageReader applique lui-même l'orientation.
    return decodeQt(data, req.target);
}

} // namespace

std::optional<DecodeResult> Decoder::decode(const DecodeRequest& req, DecodeStatus* status)
{
    setStatus(status, DecodeStatus::Unreadable);
    switch (req.format) {
    case Format::Mp4: case Format::Mov: case Format::Avi: case Format::Mkv: case Format::Unknown:
        setStatus(status, DecodeStatus::Unsupported);
        return std::nullopt;
    default:
        break;
    }

    if (kindOf(req.format) == Kind::Raw) {
        QSize previewSize;
        auto img = RawPreview::decode(req.path, req.target, &previewSize);
        if (!img) {
            setStatus(status, DecodeStatus::NoPreview);
            return std::nullopt;
        }
        setStatus(status, DecodeStatus::Ok);
        return DecodeResult{*img, previewSize.isValid() ? previewSize : img->size(), true};
    }

    bool ok = false;
    const QByteArray data = platform::readWholeFile(req.path, &ok);
    if (!ok || data.isEmpty())
        return std::nullopt;

    std::optional<DecodeResult> r;
    if (req.format == Format::Jpeg) {
        r = decodeJpeg(req, data);
    } else if (req.format == Format::Heic) {
        QSize full;
        if (auto img = HeifDecoder::decode(data, req.target, &full))
            r = DecodeResult{*img, full, false};
    } else {
        r = decodeQt(data, req.target);
    }
    if (r)
        setStatus(status, DecodeStatus::Ok);
    return r;
}

std::optional<QImage> Decoder::decodeRegion(const DecodeRequest& req, QRect region, int denominator)
{
    if (req.format != Format::Jpeg || req.orientation != 1)
        return std::nullopt;
    bool ok = false;
    const QByteArray data = platform::readWholeFile(req.path, &ok);
    if (!ok)
        return std::nullopt;
    return JpegDecoder::decodeRegion(data, region, denominator);
}

std::optional<QImage> Decoder::embeddedThumbnail(const QString& path, Format format)
{
    if (format == Format::Jpeg) {
        const Metadata m = readJpegMetadata(path);
        if (m.embeddedThumb.isEmpty())
            return std::nullopt;
        auto img = JpegDecoder::decode(m.embeddedThumb);
        if (!img)
            return std::nullopt;
        return applyOrientation(*img, m.orientation);
    }
    if (format == Format::Heic) {
        bool ok = false;
        const QByteArray data = platform::readWholeFile(path, &ok);
        return ok ? HeifDecoder::thumbnail(data) : std::nullopt;
    }
    return std::nullopt;
}

Metadata Decoder::readMetadata(const QString& path, Format format)
{
    if (format == Format::Jpeg)
        return readJpegMetadata(path);
    if (kindOf(format) == Kind::Raw)
        return RawPreview::metadata(path);
    if (kindOf(format) == Kind::Video)
        return {};
    bool ok = false;
    QByteArray data = format == Format::Heic || format == Format::Tiff
                          ? platform::readWholeFile(path, &ok)
                          : platform::readFileRange(path, 0, 256 * 1024, &ok);
    if (!ok)
        return {};
    if (format == Format::Heic)
        return HeifDecoder::metadata(data);
    Metadata m;
    m.size = QtImageDecoder::readSize(data);
    m.ok = m.size.isValid();
    return m;
}

} // namespace picazoo

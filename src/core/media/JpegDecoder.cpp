#include "media/JpegDecoder.h"

#include <turbojpeg.h>

#include <algorithm>

namespace picazoo {

namespace {

// Un handle TurboJPEG par thread : les handles ne sont pas partageables.
struct TjHandle {
    tjhandle h = nullptr;
    explicit TjHandle(int init) : h(tj3Init(init)) {}
    ~TjHandle()
    {
        if (h)
            tj3Destroy(h);
    }
};

tjhandle decompressor()
{
    thread_local TjHandle handle(TJINIT_DECOMPRESS);
    return handle.h;
}

tjhandle compressor()
{
    thread_local TjHandle handle(TJINIT_COMPRESS);
    return handle.h;
}

bool header(tjhandle h, const QByteArray& jpeg, QSize& size)
{
    if (!h || jpeg.size() < 4)
        return false;
    if (tj3DecompressHeader(h, reinterpret_cast<const unsigned char*>(jpeg.constData()), size_t(jpeg.size())) != 0)
        return false;
    size = QSize(tj3Get(h, TJPARAM_JPEGWIDTH), tj3Get(h, TJPARAM_JPEGHEIGHT));
    return size.width() > 0 && size.height() > 0;
}

} // namespace

int JpegDecoder::scaleDenominator(QSize full, QSize target)
{
    if (target.isEmpty() || full.isEmpty())
        return 1;
    const QSize fit = full.scaled(target, Qt::KeepAspectRatio);
    if (fit.width() >= full.width())
        return 1;
    for (int d : {8, 4, 2}) {
        const int w = (full.width() + d - 1) / d;
        const int h = (full.height() + d - 1) / d;
        if (w >= fit.width() && h >= fit.height())
            return d;
    }
    return 1;
}

std::optional<JpegInfo> JpegDecoder::readHeader(const QByteArray& jpeg)
{
    QSize size;
    if (!header(decompressor(), jpeg, size))
        return std::nullopt;
    return JpegInfo{size};
}

std::optional<QImage> JpegDecoder::decode(const QByteArray& jpeg, QSize target)
{
    tjhandle h = decompressor();
    QSize full;
    if (!header(h, jpeg, full))
        return std::nullopt;
    const int d = scaleDenominator(full, target);
    tj3SetScalingFactor(h, tjscalingfactor{1, d});
    tj3SetCroppingRegion(h, TJUNCROPPED);
    const int w = TJSCALED(full.width(), (tjscalingfactor{1, d}));
    const int hh = TJSCALED(full.height(), (tjscalingfactor{1, d}));
    QImage img(w, hh, QImage::Format_RGB32);
    if (img.isNull())
        return std::nullopt;
    tj3Set(h, TJPARAM_STOPONWARNING, 0);
    const int rc = tj3Decompress8(h, reinterpret_cast<const unsigned char*>(jpeg.constData()), size_t(jpeg.size()),
                                  img.bits(), int(img.bytesPerLine()), TJPF_BGRX);
    if (rc != 0 && tj3GetErrorCode(h) == TJERR_FATAL)
        return std::nullopt; // fichier corrompu ou CMYK : l'appelant essaie un autre décodeur
    return img;
}

std::optional<QImage> JpegDecoder::decodeRegion(const QByteArray& jpeg, QRect region, int denominator)
{
    tjhandle h = decompressor();
    QSize full;
    if (!header(h, jpeg, full))
        return std::nullopt;
    const tjscalingfactor sf{1, std::clamp(denominator, 1, 8)};
    tj3SetScalingFactor(h, sf);
    const int sw = TJSCALED(full.width(), sf);
    const int sh = TJSCALED(full.height(), sf);
    // Le coin gauche doit être aligné sur la taille d'un MCU (à l'échelle).
    const int subsamp = tj3Get(h, TJPARAM_SUBSAMP);
    const int mcuW = TJSCALED(subsamp >= 0 ? tjMCUWidth[subsamp] : 8, sf);
    int x = TJSCALED(region.x(), sf);
    int y = TJSCALED(region.y(), sf);
    int rw = TJSCALED(region.width(), sf);
    int rh = TJSCALED(region.height(), sf);
    const int alignedX = mcuW > 0 ? (x / mcuW) * mcuW : x;
    rw += x - alignedX;
    x = alignedX;
    rw = std::min(rw, sw - x);
    rh = std::min(rh, sh - y);
    if (rw <= 0 || rh <= 0)
        return std::nullopt;
    if (tj3SetCroppingRegion(h, tjregion{x, y, rw, rh}) != 0)
        return std::nullopt;
    QImage img(rw, rh, QImage::Format_RGB32);
    const int rc = tj3Decompress8(h, reinterpret_cast<const unsigned char*>(jpeg.constData()), size_t(jpeg.size()),
                                  img.bits(), int(img.bytesPerLine()), TJPF_BGRX);
    tj3SetCroppingRegion(h, TJUNCROPPED);
    if (rc != 0 && tj3GetErrorCode(h) == TJERR_FATAL)
        return std::nullopt;
    return img;
}

QByteArray JpegDecoder::encode(const QImage& source, int quality)
{
    tjhandle h = compressor();
    if (!h || source.isNull())
        return {};
    const QImage img = source.format() == QImage::Format_RGB32 ? source : source.convertToFormat(QImage::Format_RGB32);
    tj3Set(h, TJPARAM_QUALITY, std::clamp(quality, 1, 100));
    tj3Set(h, TJPARAM_SUBSAMP, TJSAMP_420);
    unsigned char* out = nullptr;
    size_t outSize = 0;
    if (tj3Compress8(h, img.constBits(), img.width(), int(img.bytesPerLine()), img.height(), TJPF_BGRX, &out,
                     &outSize) != 0) {
        tj3Free(out);
        return {};
    }
    QByteArray result(reinterpret_cast<const char*>(out), qsizetype(outSize));
    tj3Free(out);
    return result;
}

QImage fitInto(const QImage& image, QSize box)
{
    if (image.isNull() || box.isEmpty())
        return image;
    if (image.width() <= box.width() && image.height() <= box.height())
        return image;
    return image.scaled(box, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

} // namespace picazoo

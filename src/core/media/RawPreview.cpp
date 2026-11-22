#include "media/RawPreview.h"

#include "media/JpegDecoder.h"
#include "platform/win/Paths.h"

#include <windows.h>

#include <libraw/libraw.h>

#include <QDateTime>
#include <QTimeZone>

#include <memory>

namespace picazoo {

namespace {

// Fichier ouvert en lecture seule avec partage total, projeté en mémoire par LibRaw.
struct RawFile {
    HANDLE file = INVALID_HANDLE_VALUE;
    std::unique_ptr<LibRaw_windows_datastream> stream;
    std::unique_ptr<LibRaw> raw = std::make_unique<LibRaw>();

    bool open(const QString& path)
    {
        const std::wstring w = platform::toWin32(path);
        file = CreateFileW(w.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return false;
        try {
            stream = std::make_unique<LibRaw_windows_datastream>(file);
        } catch (...) {
            return false;
        }
        if (!stream->valid())
            return false;
        return raw->open_datastream(stream.get()) == LIBRAW_SUCCESS;
    }

    ~RawFile()
    {
        raw->recycle();
        raw.reset();
        stream.reset();
        if (file != INVALID_HANDLE_VALUE)
            CloseHandle(file);
    }
};

int orientationFromFlip(int flip)
{
    switch (flip) {
    case 3: return 3;
    case 5: return 8;
    case 6: return 6;
    default: return 1;
    }
}

} // namespace

std::optional<RawPreviewData> RawPreview::extract(const QString& path)
{
    RawFile f;
    if (!f.open(path))
        return std::nullopt;
    if (f.raw->unpack_thumb() != LIBRAW_SUCCESS)
        return std::nullopt;
    int err = 0;
    libraw_processed_image_t* th = f.raw->dcraw_make_mem_thumb(&err);
    if (!th)
        return std::nullopt;
    RawPreviewData out;
    out.orientation = orientationFromFlip(f.raw->imgdata.sizes.flip);
    if (th->type == LIBRAW_IMAGE_JPEG) {
        out.jpeg = QByteArray(reinterpret_cast<const char*>(th->data), qsizetype(th->data_size));
    } else if (th->type == LIBRAW_IMAGE_BITMAP && th->bits == 8 && th->colors == 3) {
        out.bitmap = QImage(th->data, th->width, th->height, qsizetype(th->width) * 3, QImage::Format_RGB888)
                         .convertToFormat(QImage::Format_RGB32);
    }
    LibRaw::dcraw_clear_mem(th);
    if (out.jpeg.isEmpty() && out.bitmap.isNull())
        return std::nullopt;
    return out;
}

std::optional<QImage> RawPreview::decode(const QString& path, QSize target, QSize* previewSize)
{
    auto data = extract(path);
    if (!data)
        return std::nullopt;
    std::optional<QImage> img;
    if (!data->jpeg.isEmpty()) {
        QSize pre = target;
        if (data->orientation >= 5)
            pre.transpose();
        img = JpegDecoder::decode(data->jpeg, pre);
        if (previewSize) {
            if (auto h = JpegDecoder::readHeader(data->jpeg))
                *previewSize = orientedSize(h->size, data->orientation);
        }
    } else {
        img = data->bitmap;
        if (previewSize)
            *previewSize = orientedSize(data->bitmap.size(), data->orientation);
    }
    if (!img || img->isNull())
        return std::nullopt;
    QImage out = applyOrientation(*img, data->orientation);
    if (!target.isEmpty())
        out = fitInto(out, target);
    return out;
}

Metadata RawPreview::metadata(const QString& path)
{
    Metadata m;
    RawFile f;
    if (!f.open(path))
        return m;
    const auto& d = f.raw->imgdata;
    m.ok = true;
    m.orientation = orientationFromFlip(d.sizes.flip);
    QString make = QString::fromLatin1(d.idata.make).trimmed();
    QString model = QString::fromLatin1(d.idata.model).trimmed();
    m.camera = model.startsWith(make, Qt::CaseInsensitive) || make.isEmpty() ? model : make + u' ' + model;
    m.size = QSize(d.sizes.width, d.sizes.height);
    if (d.other.timestamp > 0) {
        // LibRaw convertit la date EXIF en heure locale ; on garde l'heure de l'appareil (UTC naïf).
        const QDateTime local = QDateTime::fromSecsSinceEpoch(qint64(d.other.timestamp));
        m.takenAt = QDateTime(local.date(), local.time(), QTimeZone::UTC).toMSecsSinceEpoch();
    }
    return m;
}

} // namespace picazoo

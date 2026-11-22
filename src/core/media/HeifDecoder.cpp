#include "media/HeifDecoder.h"

#include "media/JpegDecoder.h"

#include <libheif/heif.h>

#include <memory>
#include <mutex>
#include <vector>

namespace picazoo {

namespace {

void ensureInit()
{
    static std::once_flag once;
    std::call_once(once, [] { heif_init(nullptr); });
}

struct Ctx {
    heif_context* ctx = heif_context_alloc();
    heif_image_handle* primary = nullptr;
    ~Ctx()
    {
        if (primary)
            heif_image_handle_release(primary);
        heif_context_free(ctx);
    }
    bool open(const QByteArray& data)
    {
        ensureInit();
        if (!ctx)
            return false;
        heif_error err = heif_context_read_from_memory_without_copy(ctx, data.constData(), size_t(data.size()), nullptr);
        if (err.code != heif_error_Ok)
            return false;
        err = heif_context_get_primary_image_handle(ctx, &primary);
        return err.code == heif_error_Ok;
    }
};

std::optional<QImage> decodeHandle(heif_image_handle* handle)
{
    heif_decoding_options* opts = heif_decoding_options_alloc();
    heif_image* img = nullptr;
    const heif_error err = heif_decode_image(handle, &img, heif_colorspace_RGB, heif_chroma_interleaved_RGBA, opts);
    heif_decoding_options_free(opts);
    if (err.code != heif_error_Ok || !img)
        return std::nullopt;
    size_t stride = 0;
    const uint8_t* plane = heif_image_get_plane_readonly2(img, heif_channel_interleaved, &stride);
    const int w = heif_image_get_width(img, heif_channel_interleaved);
    const int h = heif_image_get_height(img, heif_channel_interleaved);
    std::optional<QImage> out;
    if (plane && w > 0 && h > 0) {
        out = QImage(plane, w, h, qsizetype(stride), QImage::Format_RGBA8888)
                  .convertToFormat(QImage::Format_RGB32); // copie profonde
    }
    heif_image_release(img);
    return out;
}

} // namespace

std::optional<QImage> HeifDecoder::decode(const QByteArray& data, QSize target, QSize* fullSize)
{
    Ctx c;
    if (!c.open(data))
        return std::nullopt;
    if (fullSize)
        *fullSize = QSize(heif_image_handle_get_width(c.primary), heif_image_handle_get_height(c.primary));
    auto img = decodeHandle(c.primary);
    if (img && !target.isEmpty())
        *img = fitInto(*img, target);
    return img;
}

std::optional<QImage> HeifDecoder::thumbnail(const QByteArray& data)
{
    Ctx c;
    if (!c.open(data))
        return std::nullopt;
    const int n = heif_image_handle_get_number_of_thumbnails(c.primary);
    if (n <= 0)
        return std::nullopt;
    std::vector<heif_item_id> ids(static_cast<size_t>(n));
    heif_image_handle_get_list_of_thumbnail_IDs(c.primary, ids.data(), n);
    heif_image_handle* th = nullptr;
    if (heif_image_handle_get_thumbnail(c.primary, ids[0], &th).code != heif_error_Ok || !th)
        return std::nullopt;
    auto img = decodeHandle(th);
    heif_image_handle_release(th);
    return img;
}

Metadata HeifDecoder::metadata(const QByteArray& data)
{
    Metadata m;
    Ctx c;
    if (!c.open(data))
        return m;
    heif_item_id exifId = 0;
    if (heif_image_handle_get_list_of_metadata_block_IDs(c.primary, "Exif", &exifId, 1) == 1) {
        const size_t size = heif_image_handle_get_metadata_size(c.primary, exifId);
        QByteArray block(qsizetype(size), Qt::Uninitialized);
        if (size > 4 && heif_image_handle_get_metadata(c.primary, exifId, block.data()).code == heif_error_Ok) {
            // Le bloc commence par un décalage (32 bits gros-boutiste) vers l'en-tête TIFF.
            const auto* p = reinterpret_cast<const uchar*>(block.constData());
            const quint32 skip = (quint32(p[0]) << 24) | (quint32(p[1]) << 16) | (quint32(p[2]) << 8) | p[3];
            if (4 + qsizetype(skip) < block.size())
                m = parseExifBlock(block.mid(4 + qsizetype(skip)));
        }
    }
    m.ok = true;
    m.orientation = 1; // déjà appliquée par libheif ; l'EXIF ne doit pas être réappliqué
    m.embeddedThumb.clear();
    m.size = QSize(heif_image_handle_get_width(c.primary), heif_image_handle_get_height(c.primary));
    return m;
}

} // namespace picazoo

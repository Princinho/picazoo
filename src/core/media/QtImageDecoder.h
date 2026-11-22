#pragma once

// PNG, GIF (première image), BMP, TIFF, WebP via QImageReader (research R7).
// Le fichier est lu par platform::readWholeFile (lecture seule, partage total),
// puis décodé depuis la mémoire.

#include <QByteArray>
#include <QImage>
#include <QSize>

#include <optional>

namespace picazoo {

class QtImageDecoder {
public:
    // `target` vide : pleine résolution. Orientation EXIF appliquée quand le format la porte.
    static std::optional<QImage> decode(const QByteArray& data, QSize target = {}, QSize* fullSize = nullptr);
    static QSize readSize(const QByteArray& data);
};

} // namespace picazoo

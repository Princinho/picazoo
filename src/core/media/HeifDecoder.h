#pragma once

// HEIC / HEIF via libheif + libde265 (research R7). L'orientation (irot/imir)
// est appliquée par libheif au décodage.

#include "media/Metadata.h"

#include <QByteArray>
#include <QImage>
#include <QSize>

#include <optional>

namespace picazoo {

class HeifDecoder {
public:
    // Image principale, réduite pour tenir dans `target` (vide : pleine résolution).
    static std::optional<QImage> decode(const QByteArray& data, QSize target = {}, QSize* fullSize = nullptr);
    // Vignette intégrée (souvent 320×240 sur iPhone).
    static std::optional<QImage> thumbnail(const QByteArray& data);
    // Dimensions, date, appareil (bloc EXIF de l'image principale).
    static Metadata metadata(const QByteArray& data);
};

} // namespace picazoo

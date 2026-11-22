#pragma once

// Façade de décodage (contracts/core-api.md) : aiguille par format vers le
// décodeur le plus rapide. Aucun effet de bord sur les fichiers.

#include "library/Types.h"
#include "media/Metadata.h"

#include <QImage>
#include <QRect>
#include <QSize>
#include <QString>

#include <optional>

namespace picazoo {

struct DecodeRequest {
    QString path;
    Format format = Format::Unknown;
    QSize target;        // vide : pleine résolution
    int orientation = 1; // orientation EXIF connue (JPEG)
};

struct DecodeResult {
    QImage image;     // orientation appliquée
    QSize fullSize;   // dimensions pleine résolution, orientation appliquée
    bool isPreview = false; // RAW : aperçu intégré
};

enum class DecodeStatus { Ok, Unreadable, NoPreview, Unsupported };

class Decoder {
public:
    static std::optional<DecodeResult> decode(const DecodeRequest& request, DecodeStatus* status = nullptr);
    // Région de l'image pleine résolution (coordonnées orientées). JPEG seulement ;
    // les autres formats renvoient vide et l'appelant décode l'image entière.
    static std::optional<QImage> decodeRegion(const DecodeRequest& request, QRect region, int denominator);
    static std::optional<QImage> embeddedThumbnail(const QString& path, Format format);
    static Metadata readMetadata(const QString& path, Format format);
};

} // namespace picazoo

#pragma once

// Aperçu intégré des RAW via LibRaw : open + unpack_thumb, jamais dcraw_process
// (clarification du 2026-11-22 : pas de développement du RAW).

#include "media/Metadata.h"

#include <QByteArray>
#include <QImage>
#include <QSize>
#include <QString>

#include <optional>

namespace picazoo {

struct RawPreviewData {
    QByteArray jpeg;    // aperçu JPEG, ou
    QImage bitmap;      // aperçu bitmap déjà décodé
    int orientation = 1;
};

class RawPreview {
public:
    // Vide si le fichier n'a pas d'aperçu exploitable.
    static std::optional<RawPreviewData> extract(const QString& path);
    // Aperçu décodé, orientation appliquée, réduit à `target` si non vide.
    static std::optional<QImage> decode(const QString& path, QSize target, QSize* previewSize = nullptr);
    static Metadata metadata(const QString& path);
};

} // namespace picazoo

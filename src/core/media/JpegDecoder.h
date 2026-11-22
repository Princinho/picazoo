#pragma once

// Décodage et encodage JPEG avec TurboJPEG 3 (research R7, R8).

#include <QByteArray>
#include <QImage>
#include <QRect>
#include <QSize>

#include <optional>

namespace picazoo {

struct JpegInfo {
    QSize size;
};

class JpegDecoder {
public:
    // Dénominateur du facteur de réduction (1, 2, 4 ou 8) : le plus grand qui donne
    // encore une image au moins aussi grande que l'ajustement de `full` dans `target`.
    // `target` vide : pleine résolution (1).
    static int scaleDenominator(QSize full, QSize target);

    static std::optional<JpegInfo> readHeader(const QByteArray& jpeg);

    // Décode avec réduction DCT. Sans orientation appliquée.
    static std::optional<QImage> decode(const QByteArray& jpeg, QSize target = {});

    // Décode une région (en pixels de l'image pleine résolution) à l'échelle 1/denominator.
    static std::optional<QImage> decodeRegion(const QByteArray& jpeg, QRect region, int denominator = 1);

    // Encode en JPEG (qualité 1–100, sous-échantillonnage 4:2:0).
    static QByteArray encode(const QImage& image, int quality = 85);
};

// Réduit `image` pour tenir dans `box` (sans agrandir), lissage de qualité.
QImage fitInto(const QImage& image, QSize box);

} // namespace picazoo

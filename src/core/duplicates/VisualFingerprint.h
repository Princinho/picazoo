#pragma once

// Empreinte visuelle d'une photo, calculée sur sa vignette (research D2) : pHash 64 bits
// (DCT 32×32, 8×8 basses fréquences comparées à leur médiane) pour les quatre quarts de
// tour, et un score de détail pour écarter les images trop simples (FR-003).

#include <QImage>

#include <array>
#include <cstdint>

namespace picazoo {

struct VisualPrint {
    bool ok = false;
    // phash[k] : empreinte de l'image tournée de k × 90° (sens horaire).
    std::array<quint64, 4> phash{};
    double detail = 0; // écart-type du détail local (0–255) ; bas = image unie ou très simple
};

class VisualFingerprint {
public:
    static constexpr double kLowDetail = 3.0;
    static constexpr int kMaxDistance = 12; // distance de Hamming maximale d'une paire candidate

    static VisualPrint compute(const QImage& image);

    static int distance(quint64 a, quint64 b);
    // Plus petite distance entre `a` (0°) et les rotations de `b` ; `rotation` reçoit l'angle
    // à appliquer à b pour retrouver a.
    static int bestDistance(quint64 a, const std::array<quint64, 4>& b, int* rotation = nullptr);
};

} // namespace picazoo

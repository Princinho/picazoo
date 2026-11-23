#include "duplicates/PairVerifier.h"

#include <algorithm>
#include <cmath>

namespace picazoo {

namespace {

constexpr int S = 64;

std::vector<float> rotated(const std::vector<float>& g, int rotation)
{
    if (rotation % 360 == 0)
        return g;
    std::vector<float> r(g.size());
    const int k = ((rotation / 90) % 4 + 4) % 4;
    for (int y = 0; y < S; ++y)
        for (int x = 0; x < S; ++x) {
            int nx = x, ny = y;
            switch (k) {
            case 1: nx = S - 1 - y; ny = x; break;          // 90° horaire
            case 2: nx = S - 1 - x; ny = S - 1 - y; break;  // 180°
            case 3: nx = y; ny = S - 1 - x; break;          // 270° horaire
            default: break;
            }
            r[size_t(ny * S + nx)] = g[size_t(y * S + x)];
        }
    return r;
}

void normalize(std::vector<float>& v)
{
    double sum = 0, sum2 = 0;
    for (float f : v) {
        sum += f;
        sum2 += double(f) * f;
    }
    const double n = double(v.size());
    const double mean = sum / n;
    const double sd = std::sqrt(std::max(1e-6, sum2 / n - mean * mean));
    for (float& f : v)
        f = float((f - mean) / sd);
}

} // namespace

VisualSample PairVerifier::sample(const QImage& thumbnail)
{
    VisualSample s;
    if (thumbnail.isNull())
        return s;
    QImage img = thumbnail.convertToFormat(QImage::Format_RGB32);
    if (img.width() > 2 * S || img.height() > 2 * S)
        img = img.scaled(2 * S, 2 * S, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    img = img.scaled(S, S, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    s.luma.resize(size_t(S * S));
    double sumL = 0, sumL2 = 0, r = 0, g = 0, b = 0;
    for (int y = 0; y < S; ++y) {
        const QRgb* line = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for (int x = 0; x < S; ++x) {
            const double l = 0.299 * qRed(line[x]) + 0.587 * qGreen(line[x]) + 0.114 * qBlue(line[x]);
            s.luma[size_t(y * S + x)] = float(l);
            sumL += l;
            sumL2 += l * l;
            r += qRed(line[x]);
            g += qGreen(line[x]);
            b += qBlue(line[x]);
        }
    }
    const double n = S * S;
    s.meanL = sumL / n;
    s.stdL = std::sqrt(std::max(0.0, sumL2 / n - s.meanL * s.meanL));
    s.meanR = r / n;
    s.meanG = g / n;
    s.meanB = b / n;
    s.ok = true;
    return s;
}

double PairVerifier::ssim(const VisualSample& a, const VisualSample& b, int rotationOfB)
{
    if (!a.ok || !b.ok)
        return 0;
    std::vector<float> x = a.luma;
    std::vector<float> y = rotated(b.luma, rotationOfB);
    normalize(x);
    normalize(y);
    // SSIM sur fenêtres 8×8 au pas de 4 ; données normalisées (moyenne 0, écart-type 1).
    constexpr double C1 = 0.0064, C2 = 0.0576;
    double total = 0;
    int windows = 0;
    for (int wy = 0; wy + 8 <= S; wy += 4)
        for (int wx = 0; wx + 8 <= S; wx += 4) {
            double mx = 0, my = 0;
            for (int j = 0; j < 8; ++j)
                for (int i = 0; i < 8; ++i) {
                    mx += x[size_t((wy + j) * S + wx + i)];
                    my += y[size_t((wy + j) * S + wx + i)];
                }
            mx /= 64;
            my /= 64;
            double vx = 0, vy = 0, cxy = 0;
            for (int j = 0; j < 8; ++j)
                for (int i = 0; i < 8; ++i) {
                    const double dx = x[size_t((wy + j) * S + wx + i)] - mx;
                    const double dy = y[size_t((wy + j) * S + wx + i)] - my;
                    vx += dx * dx;
                    vy += dy * dy;
                    cxy += dx * dy;
                }
            vx /= 63;
            vy /= 63;
            cxy /= 63;
            total += ((2 * mx * my + C1) * (2 * cxy + C2)) / ((mx * mx + my * my + C1) * (vx + vy + C2));
            ++windows;
        }
    return windows ? total / windows : 0;
}

bool PairVerifier::distinctShots(std::optional<qint64> takenAtA, std::optional<qint64> takenAtB)
{
    return takenAtA && takenAtB && std::llabs(*takenAtA - *takenAtB) >= 1000;
}

PairVerdict PairVerifier::verify(const VisualSample& a, const VisualSample& b, int rotationOfB,
                                 std::optional<qint64> takenAtA, std::optional<qint64> takenAtB)
{
    PairVerdict v;
    if (distinctShots(takenAtA, takenAtB))
        return v;
    v.ssim = ssim(a, b, rotationOfB);
    if (v.ssim < kSimilar)
        return v;
    v.match = true;
    // Retouche probable : luminosité, contraste ou couleurs qui ont bougé.
    const bool photometric = std::abs(a.meanL - b.meanL) > 4.0 ||
                             (a.stdL > 1 && (b.stdL / a.stdL < 0.92 || b.stdL / a.stdL > 1.08)) ||
                             std::abs(a.meanR - b.meanR) > 6.0 || std::abs(a.meanG - b.meanG) > 6.0 ||
                             std::abs(a.meanB - b.meanB) > 6.0;
    v.level = (v.ssim >= kSameImage && !photometric) ? DupLevel::SameImage : DupLevel::Similar;
    return v;
}

} // namespace picazoo

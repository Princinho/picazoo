#include "duplicates/VisualFingerprint.h"

#include <QTransform>

#include <algorithm>
#include <bit>
#include <cmath>
#include <numbers>
#include <vector>

namespace picazoo {

namespace {

constexpr int N = 32;

using Grid = std::array<double, N * N>;

Grid luminance32(const QImage& image)
{
    // Réduction en deux temps (lissage) pour limiter le crénelage, puis luminance.
    QImage small = image.convertToFormat(QImage::Format_RGB32);
    if (small.width() > 4 * N || small.height() > 4 * N)
        small = small.scaled(4 * N, 4 * N, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    small = small.scaled(N, N, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    Grid g{};
    for (int y = 0; y < N; ++y) {
        const QRgb* line = reinterpret_cast<const QRgb*>(small.constScanLine(y));
        for (int x = 0; x < N; ++x)
            g[y * N + x] = 0.299 * qRed(line[x]) + 0.587 * qGreen(line[x]) + 0.114 * qBlue(line[x]);
    }
    return g;
}

Grid rotate90(const Grid& g) // sens horaire
{
    Grid r{};
    for (int y = 0; y < N; ++y)
        for (int x = 0; x < N; ++x)
            r[x * N + (N - 1 - y)] = g[y * N + x];
    return r;
}

const std::array<double, N * N>& cosTable()
{
    static const std::array<double, N * N> table = [] {
        std::array<double, N * N> t{};
        for (int u = 0; u < N; ++u)
            for (int x = 0; x < N; ++x)
                t[u * N + x] = std::cos((2 * x + 1) * u * std::numbers::pi / (2 * N));
        return t;
    }();
    return table;
}

quint64 phashOf(const Grid& g)
{
    const auto& c = cosTable();
    // DCT-II séparable, limitée aux 8 premières fréquences dans chaque direction.
    std::array<double, 8 * N> rows{};
    for (int y = 0; y < N; ++y)
        for (int u = 0; u < 8; ++u) {
            double s = 0;
            for (int x = 0; x < N; ++x)
                s += g[y * N + x] * c[u * N + x];
            rows[u * N + y] = s;
        }
    std::array<double, 64> coef{};
    for (int v = 0; v < 8; ++v)
        for (int u = 0; u < 8; ++u) {
            double s = 0;
            for (int y = 0; y < N; ++y)
                s += rows[u * N + y] * c[v * N + y];
            coef[v * 8 + u] = s;
        }
    std::vector<double> ac(coef.begin() + 1, coef.end()); // sans la composante continue
    std::nth_element(ac.begin(), ac.begin() + qsizetype(ac.size() / 2), ac.end());
    const double median = ac[ac.size() / 2];
    quint64 bits = 0;
    for (int i = 1; i < 64; ++i)
        if (coef[i] > median)
            bits |= quint64(1) << i;
    return bits;
}

double detailOf(const Grid& g)
{
    // Détail local : écart à la moyenne des blocs 4×4 (une image unie ou en deux aplats ≈ 0).
    double sum = 0, sum2 = 0;
    int n = 0;
    for (int by = 0; by < N; by += 4)
        for (int bx = 0; bx < N; bx += 4) {
            double m = 0;
            for (int y = 0; y < 4; ++y)
                for (int x = 0; x < 4; ++x)
                    m += g[(by + y) * N + bx + x];
            m /= 16;
            for (int y = 0; y < 4; ++y)
                for (int x = 0; x < 4; ++x) {
                    const double d = g[(by + y) * N + bx + x] - m;
                    sum += d;
                    sum2 += d * d;
                    ++n;
                }
        }
    const double mean = sum / n;
    // Détail global : variation entre blocs (dégradés, formes larges).
    double bsum = 0, bsum2 = 0;
    int bn = 0;
    for (int by = 0; by < N; by += 4)
        for (int bx = 0; bx < N; bx += 4) {
            double m = 0;
            for (int y = 0; y < 4; ++y)
                for (int x = 0; x < 4; ++x)
                    m += g[(by + y) * N + bx + x];
            m /= 16;
            bsum += m;
            bsum2 += m * m;
            ++bn;
        }
    const double bmean = bsum / bn;
    const double local = std::sqrt(std::max(0.0, sum2 / n - mean * mean));
    const double global = std::sqrt(std::max(0.0, bsum2 / bn - bmean * bmean));
    // Une image en deux aplats a du détail global mais presque aucun détail local.
    return std::min(local * 2.0, local + global / 8.0);
}

} // namespace

VisualPrint VisualFingerprint::compute(const QImage& image)
{
    VisualPrint p;
    if (image.isNull() || image.width() < 8 || image.height() < 8)
        return p;
    Grid g = luminance32(image);
    p.detail = detailOf(g);
    for (int k = 0; k < 4; ++k) {
        p.phash[size_t(k)] = phashOf(g);
        g = rotate90(g);
    }
    p.ok = true;
    return p;
}

int VisualFingerprint::distance(quint64 a, quint64 b) { return std::popcount(a ^ b); }

int VisualFingerprint::bestDistance(quint64 a, const std::array<quint64, 4>& b, int* rotation)
{
    int best = 65, rot = 0;
    for (int k = 0; k < 4; ++k) {
        const int d = distance(a, b[size_t(k)]);
        if (d < best) {
            best = d;
            rot = k * 90;
        }
    }
    if (rotation)
        *rotation = rot;
    return best;
}

} // namespace picazoo

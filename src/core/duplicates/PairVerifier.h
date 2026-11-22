#pragma once

// Vérification d'une paire candidate (research D4) : deux prises de vue distinctes (dates
// EXIF différentes) ne sont jamais regroupées ; sinon on compare les vignettes réduites à
// 64×64, normalisées en luminosité et en contraste (SSIM), et on qualifie la paire :
// « Même image » si la structure est la même et que la luminosité, le contraste et les
// couleurs n'ont pas bougé ; « Très ressemblante » si une retouche est probable.

#include "duplicates/DuplicateTypes.h"

#include <QImage>

#include <optional>
#include <vector>

namespace picazoo {

struct VisualSample {
    bool ok = false;
    std::vector<float> luma; // 64×64, rotation 0
    double meanL = 0, stdL = 0;
    double meanR = 0, meanG = 0, meanB = 0;
};

struct PairVerdict {
    bool match = false;
    DupLevel level = DupLevel::Similar;
    double ssim = 0;
};

class PairVerifier {
public:
    static constexpr double kSameImage = 0.95;
    static constexpr double kSimilar = 0.85;

    static VisualSample sample(const QImage& thumbnail);
    // `rotationOfB` : angle (0, 90, 180, 270, sens horaire) qui ramène b sur a.
    static double ssim(const VisualSample& a, const VisualSample& b, int rotationOfB);
    static PairVerdict verify(const VisualSample& a, const VisualSample& b, int rotationOfB,
                              std::optional<qint64> takenAtA, std::optional<qint64> takenAtB);
    // Vrai si les deux dates de prise de vue existent et désignent deux instants différents.
    static bool distinctShots(std::optional<qint64> takenAtA, std::optional<qint64> takenAtB);
};

} // namespace picazoo

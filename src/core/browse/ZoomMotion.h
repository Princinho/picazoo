#pragma once

// Moteur de zoom commun aux deux visionneuses (007, research Z1–Z5). Pur : le temps est
// passé en paramètre, rien n'est animé tout seul ; la scène appelle advance() à chaque image
// tant que moving() est vrai.
//
// Modèle : l'image (taille `imageSize`, 1 = taille réelle) est affichée à l'échelle scale(),
// son coin haut gauche en offset() dans la vue (mêmes conventions que ZoomMath).
//  - L'échelle affichée poursuit une cible, en logarithme, avec une constante de temps
//    kTauMs et une vitesse plancher kMinSpeed (fin du mouvement en temps fini, sans rebond).
//  - Un « geste » (suite de crans de molette, appui de touche avec sa répétition, clic de
//    bouton, pincement) commencé en zoom avant sous kCeilingThreshold reçoit un plafond kCeiling.
//  - Les crans de molette rapprochés sont accélérés (élan) ; jamais le clavier ni les boutons.

#include <QPointF>
#include <QSizeF>

#include <optional>

namespace picazoo::zoom {

enum class Source { Wheel = 0, Key = 1, Button = 2, Pinch = 3 };

inline constexpr double kTauMs = 90.0;             // poursuite (Z2)
inline constexpr double kMinSpeed = 0.3;           // vitesse plancher, en logarithme d'échelle par seconde
inline constexpr double kMaxLead = 2.5;            // avance maximale de la cible, en logarithme
inline constexpr double kGestureGapMs = 300.0;     // fin d'un geste de molette (FR-013)
inline constexpr double kCeilingThreshold = 0.9;   // seuil de la butée (FR-010)
inline constexpr double kCeiling = 1.0;            // butée (FR-010)
inline constexpr double kCadenceHalfLifeMs = 100.0; // moyenne glissante de la cadence (Z3)
inline constexpr double kCadenceLow = 8.0;         // crans/s sans accélération
inline constexpr double kCadenceHigh = 20.0;       // crans/s pour le gain maximal
inline constexpr double kMaxGain = 2.0;

class ZoomMotion {
public:
    void setImageSize(QSizeF image);
    void setView(QSizeF view, double margin);
    void setImmediate(bool immediate);

    void reset();
    void wheel(double angleDeltaY, QPointF at, double nowMs);
    void step(int direction, Source source, bool autoRepeat, QPointF at, double nowMs);
    void pinchBegin(double nowMs);
    void pinchUpdate(double factorSinceBegin, QPointF centroid, double nowMs);
    void pinchEnd();
    void setTarget(double scale, QPointF at);
    void fit();
    void panTo(QPointF offset);
    void stop();

    // Avance de `dtMs` ; vrai tant que le mouvement continue.
    bool advance(double dtMs);

    double scale() const { return scale_; }
    QPointF offset() const { return offset_; }
    double target() const { return target_; }
    double fitScale() const;
    bool fitMode() const { return fitMode_; }
    bool moving() const { return moving_; }
    bool immediate() const { return immediate_; }
    QSizeF imageSize() const { return image_; }
    QSizeF viewSize() const { return view_; }
    std::optional<double> ceiling() const;
    bool canPan() const;

private:
    struct Gesture {
        Source source = Source::Wheel;
        int direction = 0;
        double startScale = 1;
        std::optional<double> ceiling;
        double lastInputMs = 0;
    };

    QSizeF fitView() const;
    double bounded(double scale) const;
    void beginGesture(Source source, int direction, double nowMs);
    bool continuesGesture(Source source, int direction, bool autoRepeat, double nowMs) const;
    void retarget(double newTarget, QPointF at);
    void clampNow();

    QSizeF image_{1, 1};
    QSizeF view_{1, 1};
    double margin_ = 0;
    bool immediate_ = false;

    double scale_ = 1;
    QPointF offset_;
    double target_ = 1;
    bool fitMode_ = true;
    bool moving_ = false;

    // Mouvement en cours : ancre et extrémités (ZoomMath::zoomStepOffset).
    QPointF anchor_;
    double startScale_ = 1;
    QPointF startOffset_;
    QPointF endOffset_;

    std::optional<Gesture> gesture_;
    double cadence_ = 0; // crans par seconde
    double pinchStart_ = 1;
};

} // namespace picazoo::zoom

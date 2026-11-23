#pragma once

// Calcul du zoom de la visionneuse autonome (research V10). Modèle : l'image
// pleine résolution (taille `image`, orientée) est affichée à l'échelle `scale`
// (1 = taille réelle), son coin haut gauche en `offset` dans la vue. Un point
// image p apparaît en s = offset + scale·p.

#include <QPointF>
#include <QSizeF>

namespace picazoo::zoom {

inline constexpr double kMaxScale = 16.0;
inline constexpr double kWheelStep = 1.25; // par cran de molette (120 unités)

// Échelle d'ajustement : l'image entière tient dans la vue, jamais agrandie.
double fitScale(QSizeF image, QSizeF view);

// Borne l'échelle entre l'ajustement et kMaxScale.
double clampScale(double scale, QSizeF image, QSizeF view);

// Facteur d'échelle pour un mouvement de molette (proportionnel pour les pavés tactiles).
double wheelFactor(double angleDeltaY);

// Nouvel offset pour passer de `scale` à `newScale` en gardant fixe le point de la
// vue `anchor` (le point image qui s'y trouve y reste).
QPointF zoomAround(QPointF offset, double scale, double newScale, QPointF anchor);

// Borne l'offset : une image plus grande que la vue ne laisse jamais apparaître de
// vide au-delà de ses bords ; une image plus petite est centrée.
QPointF clampOffset(QPointF offset, double scale, QSizeF image, QSizeF view);

// Offset qui centre l'image dans la vue.
QPointF centered(double scale, QSizeF image, QSizeF view);

} // namespace picazoo::zoom

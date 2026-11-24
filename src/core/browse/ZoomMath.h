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

// Point de la vue autour duquel zoomer : `anchor`, ramené sur le bord de l'image quand
// le curseur est à côté (un zoom avant ancré dans le vide pousserait l'image hors de vue).
QPointF anchorOnImage(QPointF anchor, QPointF offset, double scale, QSizeF image);

// Offset à la fin d'un zoom de `scale` à `newScale` autour de `anchor`. En avant, le
// point image sous `anchor` y reste exactement, sans recentrage (le vide laissé autour
// d'une image encore plus petite que la vue ne fait que diminuer). En arrière, l'image
// regagne les bornes de clampOffset (centrée à l'ajustement).
QPointF zoomEndOffset(QPointF offset, double scale, double newScale, QPointF anchor, QSizeF image, QSizeF view);

// Offset pendant ce zoom (animation, pincement), à l'échelle intermédiaire `current` :
// ancré sur `anchor`, plus la correction de fin de zoom au prorata du chemin parcouru,
// pour qu'il n'y ait aucun saut.
QPointF zoomStepOffset(QPointF startOffset, double startScale, double endScale, QPointF endOffset,
                       double current, QPointF anchor);

// Borne d'un déplacement à la souris : comme clampOffset, mais un écart déjà présent
// (laissé par un zoom avant) peut subsister sans jamais s'aggraver.
QPointF clampPan(QPointF offset, QPointF previous, double scale, QSizeF image, QSizeF view);

} // namespace picazoo::zoom

#include "browse/ZoomMath.h"

#include <algorithm>
#include <cmath>

namespace picazoo::zoom {

double fitScale(QSizeF image, QSizeF view)
{
    if (image.width() <= 0 || image.height() <= 0 || view.width() <= 0 || view.height() <= 0)
        return 1.0;
    return std::min({1.0, view.width() / image.width(), view.height() / image.height()});
}

double clampScale(double scale, QSizeF image, QSizeF view)
{
    return std::clamp(scale, fitScale(image, view), kMaxScale);
}

double wheelFactor(double angleDeltaY) { return std::pow(kWheelStep, angleDeltaY / 120.0); }

QPointF zoomAround(QPointF offset, double scale, double newScale, QPointF anchor)
{
    if (scale <= 0)
        return offset;
    const double r = newScale / scale;
    return anchor - (anchor - offset) * r;
}

namespace {

double clampAxis(double offset, double extent, double view)
{
    if (extent <= view)
        return (view - extent) / 2.0;
    return std::clamp(offset, view - extent, 0.0);
}

// Bornes de clampAxis élargies jusqu'à `previous` : on peut s'en rapprocher, pas s'en éloigner.
double clampAxisFrom(double offset, double previous, double extent, double view)
{
    const double lo = extent <= view ? (view - extent) / 2.0 : view - extent;
    const double hi = extent <= view ? lo : 0.0;
    return std::clamp(offset, std::min(lo, previous), std::max(hi, previous));
}

} // namespace

QPointF clampOffset(QPointF offset, double scale, QSizeF image, QSizeF view)
{
    return {clampAxis(offset.x(), image.width() * scale, view.width()),
            clampAxis(offset.y(), image.height() * scale, view.height())};
}

QPointF centered(double scale, QSizeF image, QSizeF view)
{
    return {(view.width() - image.width() * scale) / 2.0, (view.height() - image.height() * scale) / 2.0};
}

QPointF anchorOnImage(QPointF anchor, QPointF offset, double scale, QSizeF image)
{
    if (scale <= 0)
        return anchor;
    const QPointF p = (anchor - offset) / scale;
    return offset + QPointF(std::clamp(p.x(), 0.0, image.width()), std::clamp(p.y(), 0.0, image.height())) * scale;
}

QPointF zoomEndOffset(QPointF offset, double scale, double newScale, QPointF anchor, QSizeF image, QSizeF view)
{
    const QPointF anchored = zoomAround(offset, scale, newScale, anchor);
    if (newScale >= scale)
        return anchored;
    return clampOffset(anchored, newScale, image, view);
}

QPointF zoomStepOffset(QPointF startOffset, double startScale, double endScale, QPointF endOffset,
                       double current, QPointF anchor)
{
    const QPointF anchored = zoomAround(startOffset, startScale, current, anchor);
    if (std::abs(endScale - startScale) < 1e-12)
        return endOffset;
    const double t = std::clamp((current - startScale) / (endScale - startScale), 0.0, 1.0);
    const QPointF correction = endOffset - zoomAround(startOffset, startScale, endScale, anchor);
    return anchored + correction * t;
}

QPointF clampPan(QPointF offset, QPointF previous, double scale, QSizeF image, QSizeF view)
{
    return {clampAxisFrom(offset.x(), previous.x(), image.width() * scale, view.width()),
            clampAxisFrom(offset.y(), previous.y(), image.height() * scale, view.height())};
}

} // namespace picazoo::zoom

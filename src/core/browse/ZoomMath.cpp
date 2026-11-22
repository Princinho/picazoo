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

} // namespace picazoo::zoom

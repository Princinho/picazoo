#include "browse/ZoomController.h"

#include <algorithm>

namespace picazoo {

ZoomController::ZoomController(QObject* parent) : QObject(parent) { clock_.start(); }

void ZoomController::setImmediate(bool v)
{
    if (v == m_.immediate())
        return;
    m_.setImmediate(v);
    emit changed();
}

void ZoomController::applyImage()
{
    if (imageW_ > 0 && imageH_ > 0)
        m_.setImageSize({imageW_, imageH_});
    emit changed();
}

void ZoomController::applyView()
{
    if (viewW_ > 0 && viewH_ > 0)
        m_.setView({viewW_, viewH_}, margin_);
    emit changed();
}

void ZoomController::setImageWidth(double v)
{
    if (v == imageW_)
        return;
    imageW_ = v;
    applyImage();
}

void ZoomController::setImageHeight(double v)
{
    if (v == imageH_)
        return;
    imageH_ = v;
    applyImage();
}

void ZoomController::setViewWidth(double v)
{
    if (v == viewW_)
        return;
    viewW_ = v;
    applyView();
}

void ZoomController::setViewHeight(double v)
{
    if (v == viewH_)
        return;
    viewH_ = v;
    applyView();
}

void ZoomController::setMargin(double v)
{
    if (v == margin_)
        return;
    margin_ = v;
    applyView();
}

void ZoomController::wheel(double angleDeltaY, double x, double y)
{
    m_.wheel(angleDeltaY, {x, y}, now());
    emit changed();
}

void ZoomController::step(int direction, int source, bool autoRepeat, double x, double y)
{
    m_.step(direction, static_cast<zoom::Source>(std::clamp(source, 0, 3)), autoRepeat, {x, y}, now());
    emit changed();
}

void ZoomController::stepCentered(int direction, int source, bool autoRepeat)
{
    step(direction, source, autoRepeat, viewW_ / 2, viewH_ / 2);
}

void ZoomController::pinchBegin()
{
    m_.pinchBegin(now());
    emit changed();
}

void ZoomController::pinchUpdate(double factorSinceBegin, double x, double y)
{
    m_.pinchUpdate(factorSinceBegin, {x, y}, now());
    emit changed();
}

void ZoomController::pinchEnd()
{
    m_.pinchEnd();
    emit changed();
}

void ZoomController::setTarget(double scale, double x, double y)
{
    m_.setTarget(scale, {x, y});
    emit changed();
}

void ZoomController::fit()
{
    m_.fit();
    emit changed();
}

void ZoomController::actualSize() { setTarget(1.0, viewW_ / 2, viewH_ / 2); }

void ZoomController::toggleAt(double x, double y)
{
    setTarget(m_.fitMode() ? 1.0 : m_.fitScale(), x, y);
}

void ZoomController::panTo(double x, double y)
{
    m_.panTo({x, y});
    emit changed();
}

void ZoomController::stop()
{
    m_.stop();
    emit changed();
}

void ZoomController::reset()
{
    m_.reset();
    emit changed();
}

void ZoomController::advance(double frameTimeSeconds)
{
    if (!m_.moving())
        return;
    // Une image très en retard (fenêtre cachée, machine chargée) ne doit pas faire sauter le zoom.
    m_.advance(std::min(frameTimeSeconds * 1000.0, 100.0));
    emit changed();
}

} // namespace picazoo

#include "browse/ZoomMotion.h"

#include "browse/ZoomMath.h"

#include <algorithm>
#include <cmath>

namespace picazoo::zoom {

namespace {

double gainFor(double cadence)
{
    if (cadence <= kCadenceLow)
        return 1.0;
    const double t = std::min(1.0, (cadence - kCadenceLow) / (kCadenceHigh - kCadenceLow));
    return 1.0 + (kMaxGain - 1.0) * t;
}

} // namespace

QSizeF ZoomMotion::fitView() const
{
    return {std::max(1.0, view_.width() - 2 * margin_), std::max(1.0, view_.height() - 2 * margin_)};
}

double ZoomMotion::fitScale() const { return zoom::fitScale(image_, fitView()); }

double ZoomMotion::bounded(double s) const { return clampScale(s, image_, fitView()); }

std::optional<double> ZoomMotion::ceiling() const
{
    return gesture_ ? gesture_->ceiling : std::nullopt;
}

bool ZoomMotion::canPan() const
{
    return image_.width() * scale_ > view_.width() + 0.5 || image_.height() * scale_ > view_.height() + 0.5;
}

void ZoomMotion::clampNow() { offset_ = clampOffset(offset_, scale_, image_, view_); }

void ZoomMotion::setImageSize(QSizeF image)
{
    if (image.width() <= 0 || image.height() <= 0 || image == image_)
        return;
    if (fitMode_ && !moving_) {
        image_ = image;
        scale_ = target_ = fitScale();
        clampNow();
        return;
    }
    // Taille affichée constante : seule l'unité de l'échelle change (aperçu → pleine résolution).
    const double ratio = image_.width() / image.width();
    image_ = image;
    scale_ *= ratio;
    target_ = bounded(target_ * ratio);
    startScale_ *= ratio;
    pinchStart_ *= ratio;
    if (gesture_) {
        gesture_->startScale *= ratio;
        // Le plafond reste « 100 % de la photo » : sa valeur ne change pas.
        if (gesture_->ceiling && target_ > *gesture_->ceiling && gesture_->direction > 0)
            target_ = std::max(scale_, *gesture_->ceiling);
    }
    if (moving_) {
        startScale_ = scale_;
        startOffset_ = offset_;
        endOffset_ = zoomEndOffset(offset_, scale_, target_, anchor_, image_, view_);
    } else {
        target_ = scale_;
        clampNow();
    }
}

void ZoomMotion::setView(QSizeF view, double margin)
{
    if (view.width() <= 0 || view.height() <= 0)
        return;
    if (view == view_ && margin == margin_)
        return;
    view_ = view;
    margin_ = margin;
    if (moving_)
        return;
    if (fitMode_)
        scale_ = target_ = fitScale();
    clampNow();
}

void ZoomMotion::setImmediate(bool immediate)
{
    immediate_ = immediate;
    if (immediate_ && moving_) {
        scale_ = target_;
        offset_ = endOffset_;
        moving_ = false;
    }
}

void ZoomMotion::reset()
{
    gesture_.reset();
    cadence_ = 0;
    moving_ = false;
    fitMode_ = true;
    scale_ = target_ = fitScale();
    offset_ = centered(scale_, image_, view_);
    clampNow();
}

void ZoomMotion::stop()
{
    gesture_.reset();
    cadence_ = 0;
    if (moving_) {
        target_ = scale_;
        moving_ = false;
    }
    fitMode_ = scale_ <= fitScale() * 1.0001;
}

void ZoomMotion::retarget(double newTarget, QPointF at)
{
    const double nt = bounded(newTarget);
    const QPointF a = anchorOnImage(at, offset_, scale_, image_);
    anchor_ = a;
    startScale_ = scale_;
    startOffset_ = offset_;
    endOffset_ = zoomEndOffset(offset_, scale_, nt, a, image_, view_);
    target_ = nt;
    fitMode_ = nt <= fitScale() * 1.0001;
    if (immediate_ || std::abs(nt - scale_) < 1e-12) {
        scale_ = nt;
        offset_ = endOffset_;
        moving_ = false;
    } else {
        moving_ = true;
    }
}

void ZoomMotion::beginGesture(Source source, int direction, double nowMs)
{
    Gesture g;
    g.source = source;
    g.direction = direction;
    g.startScale = scale_;
    if (direction > 0 && scale_ < kCeilingThreshold - 1e-9)
        g.ceiling = kCeiling;
    g.lastInputMs = nowMs;
    gesture_ = g;
    cadence_ = 0;
}

bool ZoomMotion::continuesGesture(Source source, int direction, bool autoRepeat, double nowMs) const
{
    if (!gesture_ || gesture_->source != source || gesture_->direction != direction)
        return false;
    switch (source) {
    case Source::Wheel: return nowMs - gesture_->lastInputMs <= kGestureGapMs;
    case Source::Key: return autoRepeat;
    case Source::Button:
    case Source::Pinch: return false;
    }
    return false;
}

void ZoomMotion::wheel(double angleDeltaY, QPointF at, double nowMs)
{
    if (angleDeltaY == 0)
        return;
    const int dir = angleDeltaY > 0 ? 1 : -1;
    const double notches = std::abs(angleDeltaY) / 120.0;
    if (continuesGesture(Source::Wheel, dir, false, nowMs)) {
        const double dt = std::max(1.0, nowMs - gesture_->lastInputMs);
        const double instant = notches / (dt / 1000.0);
        const double alpha = 1.0 - std::pow(2.0, -dt / kCadenceHalfLifeMs);
        cadence_ += alpha * (instant - cadence_);
    } else {
        // Un cran en sens inverse arrête aussitôt l'élan (FR-042).
        if (moving_ && (target_ > scale_ ? 1 : -1) != dir)
            stop();
        beginGesture(Source::Wheel, dir, nowMs);
    }
    gesture_->lastInputMs = nowMs;

    const double gain = immediate_ ? 1.0 : gainFor(cadence_);
    double nt = target_ * std::pow(kWheelStep, dir * notches * gain);
    nt = dir > 0 ? std::min(nt, scale_ * std::exp(kMaxLead)) : std::max(nt, scale_ * std::exp(-kMaxLead));
    if (dir > 0 && gesture_->ceiling)
        nt = std::min(nt, *gesture_->ceiling);
    nt = bounded(nt);
    if (std::abs(nt - target_) > 1e-12)
        retarget(nt, at);
}

void ZoomMotion::step(int direction, Source source, bool autoRepeat, QPointF at, double nowMs)
{
    if (direction == 0)
        return;
    const int dir = direction > 0 ? 1 : -1;
    if (!continuesGesture(source, dir, autoRepeat, nowMs)) {
        if (moving_ && (target_ > scale_ ? 1 : -1) != dir)
            stop();
        beginGesture(source, dir, nowMs);
    }
    gesture_->lastInputMs = nowMs;
    double nt = target_ * std::pow(kWheelStep, dir);
    nt = dir > 0 ? std::min(nt, scale_ * std::exp(kMaxLead)) : std::max(nt, scale_ * std::exp(-kMaxLead));
    if (dir > 0 && gesture_->ceiling)
        nt = std::min(nt, *gesture_->ceiling);
    nt = bounded(nt);
    if (std::abs(nt - target_) > 1e-12)
        retarget(nt, at);
}

void ZoomMotion::pinchBegin(double nowMs)
{
    stop();
    Gesture g;
    g.source = Source::Pinch;
    g.direction = 1;
    g.startScale = scale_;
    if (scale_ < kCeilingThreshold - 1e-9)
        g.ceiling = kCeiling;
    g.lastInputMs = nowMs;
    gesture_ = g;
    pinchStart_ = scale_;
}

void ZoomMotion::pinchUpdate(double factorSinceBegin, QPointF centroid, double nowMs)
{
    if (!gesture_ || gesture_->source != Source::Pinch)
        pinchBegin(nowMs);
    double ns = pinchStart_ * factorSinceBegin;
    if (gesture_->ceiling)
        ns = std::min(ns, *gesture_->ceiling);
    ns = bounded(ns);
    gesture_->lastInputMs = nowMs;
    const QPointF a = anchorOnImage(centroid, offset_, scale_, image_);
    offset_ = zoomEndOffset(offset_, scale_, ns, a, image_, view_);
    scale_ = target_ = ns;
    moving_ = false;
    fitMode_ = ns <= fitScale() * 1.0001;
}

void ZoomMotion::pinchEnd()
{
    if (gesture_ && gesture_->source == Source::Pinch)
        gesture_.reset();
}

void ZoomMotion::setTarget(double s, QPointF at)
{
    gesture_.reset();
    cadence_ = 0;
    retarget(s, at);
}

void ZoomMotion::fit() { setTarget(fitScale(), {view_.width() / 2, view_.height() / 2}); }

void ZoomMotion::panTo(QPointF offset)
{
    if (moving_)
        stop();
    offset_ = clampPan(offset, offset_, scale_, image_, view_);
}

bool ZoomMotion::advance(double dtMs)
{
    if (!moving_)
        return false;
    if (dtMs <= 0)
        return true;
    const double gap = std::log(target_ / scale_);
    double d = gap * (1.0 - std::exp(-dtMs / kTauMs));
    const double floor = kMinSpeed * dtMs / 1000.0;
    if (std::abs(d) < floor)
        d = gap > 0 ? floor : -floor;
    if (std::abs(d) >= std::abs(gap)) {
        scale_ = target_;
        offset_ = endOffset_;
        moving_ = false;
        return false;
    }
    scale_ *= std::exp(d);
    offset_ = zoomStepOffset(startOffset_, startScale_, target_, endOffset_, scale_, anchor_);
    return true;
}

} // namespace picazoo::zoom

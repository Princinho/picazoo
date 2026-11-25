#pragma once

// Façade QML de ZoomMotion (007, research Z1), enregistrée sous `Picazoo.Zoom 1.0` dans les
// deux exécutables. La scène lie la position et la taille de l'image à offsetX / offsetY /
// scale et fait tourner une FrameAnimation qui appelle advance(frameTime) tant que `moving`.

#include "browse/ZoomMotion.h"

#include <QElapsedTimer>
#include <QObject>

namespace picazoo {

class ZoomController : public QObject {
    Q_OBJECT
    Q_PROPERTY(double scale READ scale NOTIFY changed)
    Q_PROPERTY(double target READ target NOTIFY changed)
    Q_PROPERTY(double offsetX READ offsetX NOTIFY changed)
    Q_PROPERTY(double offsetY READ offsetY NOTIFY changed)
    Q_PROPERTY(double percent READ percent NOTIFY changed)
    Q_PROPERTY(double fitScale READ fitScale NOTIFY changed)
    Q_PROPERTY(bool fitMode READ fitMode NOTIFY changed)
    Q_PROPERTY(bool moving READ moving NOTIFY changed)
    Q_PROPERTY(bool canPan READ canPan NOTIFY changed)
    Q_PROPERTY(bool immediate READ immediate WRITE setImmediate NOTIFY changed)
    Q_PROPERTY(double imageWidth READ imageWidth WRITE setImageWidth NOTIFY changed)
    Q_PROPERTY(double imageHeight READ imageHeight WRITE setImageHeight NOTIFY changed)
    Q_PROPERTY(double viewWidth READ viewWidth WRITE setViewWidth NOTIFY changed)
    Q_PROPERTY(double viewHeight READ viewHeight WRITE setViewHeight NOTIFY changed)
    Q_PROPERTY(double margin READ margin WRITE setMargin NOTIFY changed)

public:
    enum Source { Wheel = 0, Key = 1, Button = 2, Pinch = 3 };
    Q_ENUM(Source)

    explicit ZoomController(QObject* parent = nullptr);

    double scale() const { return m_.scale(); }
    double target() const { return m_.target(); }
    double offsetX() const { return m_.offset().x(); }
    double offsetY() const { return m_.offset().y(); }
    double percent() const { return m_.scale() * 100.0; }
    double fitScale() const { return m_.fitScale(); }
    bool fitMode() const { return m_.fitMode(); }
    bool moving() const { return m_.moving(); }
    bool canPan() const { return m_.canPan(); }
    bool immediate() const { return m_.immediate(); }
    double imageWidth() const { return imageW_; }
    double imageHeight() const { return imageH_; }
    double viewWidth() const { return viewW_; }
    double viewHeight() const { return viewH_; }
    double margin() const { return margin_; }

    void setImmediate(bool v);
    void setImageWidth(double v);
    void setImageHeight(double v);
    void setViewWidth(double v);
    void setViewHeight(double v);
    void setMargin(double v);

    Q_INVOKABLE void wheel(double angleDeltaY, double x, double y);
    Q_INVOKABLE void step(int direction, int source, bool autoRepeat, double x, double y);
    // Pas au centre de la vue (clavier, boutons).
    Q_INVOKABLE void stepCentered(int direction, int source, bool autoRepeat);
    Q_INVOKABLE void pinchBegin();
    Q_INVOKABLE void pinchUpdate(double factorSinceBegin, double x, double y);
    Q_INVOKABLE void pinchEnd();
    Q_INVOKABLE void setTarget(double scale, double x, double y);
    Q_INVOKABLE void fit();
    Q_INVOKABLE void actualSize(); // 100 % au centre
    Q_INVOKABLE void toggleAt(double x, double y); // double-clic : ajustement ↔ 100 %
    Q_INVOKABLE void panTo(double x, double y);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void reset();
    Q_INVOKABLE void advance(double frameTimeSeconds);

signals:
    void changed();

private:
    double now() const { return clock_.nsecsElapsed() / 1e6; }
    void applyImage();
    void applyView();

    zoom::ZoomMotion m_;
    QElapsedTimer clock_;
    double imageW_ = 0, imageH_ = 0, viewW_ = 0, viewH_ = 0, margin_ = 0;
};

} // namespace picazoo

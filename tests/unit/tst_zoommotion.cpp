// 007 — moteur de zoom commun (contracts/core-api.md, garanties 1 à 10) : poursuite sans
// rebond, butée à 100 % pour un geste commencé sous 90 %, élan selon la cadence, interruption.

#include "browse/ZoomMath.h"
#include "browse/ZoomMotion.h"

#include <QtTest>

#include <cmath>

using namespace picazoo;
using zoom::Source;
using zoom::ZoomMotion;

namespace {

const QSizeF kImage(6000, 4000);
const QSizeF kView(1920, 1080);
const QPointF kCenter(960, 540);

ZoomMotion fitted(QSizeF image = kImage)
{
    ZoomMotion m;
    m.setView(kView, 0);
    m.setImageSize(image);
    m.reset();
    return m;
}

// Fait avancer par images de 16 ms ; renvoie le temps écoulé jusqu'à l'arrêt.
double settle(ZoomMotion& m, double frame = 16.0, double limit = 5000.0)
{
    double t = 0;
    while (m.moving() && t < limit) {
        m.advance(frame);
        t += frame;
    }
    return t;
}

// Place l'échelle affichée exactement à `s` (commande absolue, sans animation).
void placeAt(ZoomMotion& m, double s)
{
    m.setImmediate(true);
    m.setTarget(s, kCenter);
    m.setImmediate(false);
}

} // namespace

class TstZoomMotion : public QObject {
    Q_OBJECT

private slots:
    // --- Poursuite (garanties 2, 6, 7, 8, 9)

    void chaseNeverOvershootsAndLandsExactly()
    {
        ZoomMotion m = fitted();
        placeAt(m, 0.5);
        m.step(+1, Source::Key, false, kCenter, 0);
        const double target = m.target();
        QCOMPARE(target, 0.625);
        double previous = m.scale();
        while (m.moving()) {
            m.advance(16);
            QVERIFY(m.scale() <= target + 1e-12);
            QVERIFY(m.scale() >= previous);
            previous = m.scale();
        }
        QCOMPARE(m.scale(), target);
    }

    void isolatedStepDuration()
    {
        ZoomMotion m = fitted();
        placeAt(m, 0.5);
        m.step(+1, Source::Button, false, kCenter, 0);
        const double path = std::log(m.target() / m.scale());
        const double from = m.scale();
        double t = 0;
        while (std::log(m.scale() / from) < 0.98 * path) {
            m.advance(1);
            t += 1;
        }
        QVERIFY2(t >= 250 && t <= 350, qPrintable(QString::number(t)));
        QVERIFY(settle(m, 1) + t <= 400);
    }

    void stopAndResetEndMotionAtOnce()
    {
        ZoomMotion m = fitted();
        placeAt(m, 0.5);
        m.step(+1, Source::Key, false, kCenter, 0);
        m.advance(40);
        const double shown = m.scale();
        m.stop();
        QVERIFY(!m.moving());
        QCOMPARE(m.target(), shown);
        m.step(+1, Source::Key, false, kCenter, 100);
        QVERIFY(m.moving());
        m.reset();
        QVERIFY(!m.moving());
        QVERIFY(m.fitMode());
        QCOMPARE(m.scale(), m.fitScale());
    }

    void anchorStaysUnderCursorDuringZoomIn()
    {
        ZoomMotion m = fitted();
        placeAt(m, 0.5);
        const QPointF cursor(1400, 300);
        const QPointF imagePoint = (cursor - m.offset()) / m.scale();
        for (int i = 0; i < 6; ++i)
            m.wheel(120, cursor, i * 40.0);
        while (m.moving()) {
            m.advance(16);
            const QPointF onScreen = m.offset() + imagePoint * m.scale();
            QVERIFY2(std::abs(onScreen.x() - cursor.x()) < 0.5 && std::abs(onScreen.y() - cursor.y()) < 0.5,
                     qPrintable(QStringLiteral("%1, %2").arg(onScreen.x()).arg(onScreen.y())));
        }
    }

    void immediateMode()
    {
        ZoomMotion m = fitted();
        m.setImmediate(true);
        placeAt(m, 0.5);
        m.setImmediate(true);
        m.wheel(120, kCenter, 0);
        QVERIFY(!m.moving());
        QCOMPARE(m.scale(), 0.625);
        m.wheel(120, kCenter, 10); // cadence élevée : pas d'accélération en mode immédiat
        QCOMPARE(m.scale(), 0.625 * 1.25);
    }

    void absoluteTargetsAndFit()
    {
        ZoomMotion m = fitted();
        QVERIFY(m.fitMode());
        m.setTarget(1.0, kCenter);
        settle(m);
        QCOMPARE(m.scale(), 1.0);
        QVERIFY(!m.fitMode());
        m.fit();
        settle(m);
        QCOMPARE(m.scale(), m.fitScale());
        QVERIFY(m.fitMode());
        // À l'ajustement, un changement de vue réajuste l'image.
        m.setView({1280, 720}, 0);
        QCOMPARE(m.scale(), zoom::fitScale(kImage, {1280, 720}));
    }

    // --- Butée (garanties 1, 3, 4, 10)

    void fastWheelFromFitStopsAt100()
    {
        ZoomMotion m = fitted(); // ajustement ≈ 0,27
        QVERIFY(m.scale() < 0.3);
        double t = 0;
        for (int i = 0; i < 30; ++i, t += 50) {
            m.wheel(120, kCenter, t);
            m.advance(50);
            QVERIFY(m.target() <= 1.0);
        }
        settle(m);
        QCOMPARE(m.scale(), 1.0);
    }

    void notchesAtCeilingDoNotAccumulate()
    {
        ZoomMotion m = fitted();
        double t = 0;
        for (int i = 0; i < 12; ++i, t += 30)
            m.wheel(120, kCenter, t);
        settle(m);
        QCOMPARE(m.scale(), 1.0);
        for (int i = 0; i < 10; ++i, t += 30)
            m.wheel(120, kCenter, t);
        QVERIFY(!m.moving());
        QCOMPARE(m.scale(), 1.0);
        // Une pause de plus de 300 ms : nouveau geste, parti de 100 % (≥ 90 %), sans butée.
        t += 400;
        m.wheel(120, kCenter, t);
        settle(m);
        QCOMPARE(m.scale(), 1.25);
    }

    void thresholdEvaluatedAtGestureStart()
    {
        ZoomMotion m = fitted();
        placeAt(m, 0.8);
        double t = 0;
        for (int i = 0; i < 5; ++i, t += 30)
            m.wheel(120, kCenter, t);
        settle(m);
        QCOMPARE(m.scale(), 1.0);

        placeAt(m, 0.9);
        m.wheel(120, kCenter, 10'000);
        settle(m);
        QCOMPARE(m.scale(), 1.125);
    }

    void zoomOutHasNoCeiling()
    {
        ZoomMotion m = fitted();
        placeAt(m, 4.0);
        m.wheel(-120, kCenter, 0);
        QVERIFY(!m.ceiling());
        settle(m);
        QCOMPARE(m.scale(), 4.0 / 1.25);
    }

    void heldKeyIsOneGesture()
    {
        ZoomMotion m = fitted();
        placeAt(m, 0.5);
        double t = 0;
        m.step(+1, Source::Key, false, kCenter, t);
        for (int i = 0; i < 10; ++i) {
            t += 33;
            m.step(+1, Source::Key, true, kCenter, t);
            m.advance(33);
        }
        settle(m);
        QCOMPARE(m.scale(), 1.0);
        m.step(+1, Source::Key, false, kCenter, t + 500);
        settle(m);
        QCOMPARE(m.scale(), 1.25);
    }

    void buttonClickIsOneGesture()
    {
        ZoomMotion m = fitted();
        placeAt(m, 0.85);
        m.step(+1, Source::Button, false, kCenter, 0);
        settle(m);
        QCOMPARE(m.scale(), 1.0);
        m.step(+1, Source::Button, false, kCenter, 100);
        settle(m);
        QCOMPARE(m.scale(), 1.25);
    }

    void pinchStartedBelowThresholdStaysAt100()
    {
        ZoomMotion m = fitted();
        placeAt(m, 0.5);
        m.pinchBegin(0);
        m.pinchUpdate(1.5, kCenter, 10);
        QCOMPARE(m.scale(), 0.75);
        m.pinchUpdate(4.0, kCenter, 20);
        QCOMPARE(m.scale(), 1.0);
        m.pinchUpdate(8.0, kCenter, 30);
        QCOMPARE(m.scale(), 1.0);
        m.pinchEnd();
        m.pinchBegin(40);
        m.pinchUpdate(2.0, kCenter, 50);
        QCOMPARE(m.scale(), 2.0);
    }

    void reversingStartsANewGesture()
    {
        ZoomMotion m = fitted();
        placeAt(m, 2.0);
        m.wheel(-120, kCenter, 0);
        m.advance(30);
        const double shown = m.scale();
        m.wheel(120, kCenter, 40); // sens inverse : l'élan s'arrête, nouveau geste
        QVERIFY(std::abs(m.target() - shown * 1.25) < 1e-9);
        QVERIFY(!m.ceiling()); // parti de ≥ 0,9
    }

    void smallImageFitAboveThresholdHasNoCeiling()
    {
        ZoomMotion m = fitted({2021, 1137}); // ajustement ≈ 0,95 dans 1920 × 1080
        QVERIFY(m.fitScale() >= 0.9);
        m.wheel(120, kCenter, 0);
        QVERIFY(!m.ceiling());
        settle(m);
        QVERIFY(m.scale() > 1.0);
    }

    void ceilingSurvivesRealSizeArrival()
    {
        // Aperçu de 3000 px de large, puis la vraie taille (6000 px) arrive pendant le geste.
        ZoomMotion m = fitted({3000, 2000});
        placeAt(m, 0.6);
        m.wheel(120, kCenter, 0);
        m.advance(16);
        const double shownWidth = m.scale() * 3000;
        m.setImageSize(kImage);
        QVERIFY(std::abs(m.scale() * 6000 - shownWidth) < 1e-6); // taille affichée inchangée
        double t = 30;
        for (int i = 0; i < 20; ++i, t += 30)
            m.wheel(120, kCenter, t);
        settle(m);
        QCOMPARE(m.scale(), 1.0);
    }

    // --- Élan (garanties 5, 6)

    void isolatedNotchIsExactStep()
    {
        ZoomMotion m = fitted();
        placeAt(m, 1.0);
        m.wheel(120, kCenter, 0);
        QCOMPARE(m.target(), 1.25);
    }

    void fastGestureGoesFurther()
    {
        auto run = [](double intervalMs) {
            ZoomMotion m = fitted();
            placeAt(m, 1.0);
            double t = 0;
            for (int i = 0; i < 6; ++i, t += intervalMs) {
                m.wheel(120, kCenter, t);
                m.advance(intervalMs);
            }
            settle(m);
            return m.scale();
        };
        const double slow = run(250); // 4 crans/s
        const double fast = run(50);  // 20 crans/s
        QCOMPARE(slow, std::pow(1.25, 6));
        QVERIFY2(fast > slow * 1.1, qPrintable(QStringLiteral("%1 vs %2").arg(fast).arg(slow)));
    }

    void glideEndsWithin600ms()
    {
        ZoomMotion m = fitted();
        placeAt(m, 1.0);
        double t = 0;
        for (int i = 0; i < 10; ++i, t += 40) {
            m.wheel(120, kCenter, t);
            m.advance(40);
        }
        QVERIFY(m.moving()); // l'élan continue après le dernier cran
        QVERIFY2(settle(m) < 600, "élan trop long");
    }

    void keysAndButtonsNeverAccelerate()
    {
        ZoomMotion m = fitted();
        placeAt(m, 1.0);
        double t = 0;
        m.step(+1, Source::Key, false, kCenter, t);
        for (int i = 0; i < 5; ++i)
            m.step(+1, Source::Key, true, kCenter, t += 20);
        QVERIFY(std::abs(m.target() - std::pow(1.25, 6)) < 1e-9);
    }

    void acceleratedGestureStillStopsAt100()
    {
        ZoomMotion m = fitted();
        double t = 0;
        for (int i = 0; i < 20; ++i, t += 25) {
            m.wheel(120, kCenter, t);
            m.advance(25);
        }
        settle(m);
        QCOMPARE(m.scale(), 1.0);
    }

    void stopFreezesGlide()
    {
        ZoomMotion m = fitted();
        placeAt(m, 1.0);
        for (int i = 0; i < 5; ++i)
            m.wheel(120, kCenter, i * 40.0);
        m.advance(50);
        const double shown = m.scale();
        m.stop();
        m.advance(100);
        QCOMPARE(m.scale(), shown);
    }

    void panHasNoInertia()
    {
        ZoomMotion m = fitted();
        placeAt(m, 1.0);
        const QPointF before = m.offset();
        m.panTo(before + QPointF(-100, -50));
        QVERIFY(!m.moving());
        QCOMPARE(m.offset(), before + QPointF(-100, -50));
        m.advance(100);
        QCOMPARE(m.offset(), before + QPointF(-100, -50));
    }
};

QTEST_GUILESS_MAIN(TstZoomMotion)
#include "tst_zoommotion.moc"

// T005/T008 (005) — zoom ancré sur le curseur (SC-006), bornes et déplacement.

#include "browse/ZoomMath.h"

#include <QtTest>

using namespace picazoo;

class TstZoomMath : public QObject {
    Q_OBJECT

private slots:
    void fitNeverEnlarges()
    {
        QCOMPARE(zoom::fitScale({4000, 3000}, {1920, 1200}), 0.4);
        QCOMPARE(zoom::fitScale({64, 64}, {1920, 1200}), 1.0); // petite image : taille réelle
        QCOMPARE(zoom::clampScale(100, {4000, 3000}, {1920, 1200}), zoom::kMaxScale);
        QCOMPARE(zoom::clampScale(0.01, {4000, 3000}, {1920, 1200}), 0.4);
    }

    void wheelSteps()
    {
        QCOMPARE(zoom::wheelFactor(120), 1.25);
        QVERIFY(qFuzzyCompare(zoom::wheelFactor(-120), 1 / 1.25));
        QVERIFY(qFuzzyCompare(zoom::wheelFactor(60), std::sqrt(1.25))); // pavé tactile
    }

    // Le point image sous le curseur y reste, cran après cran, à moins de 2 px près.
    void anchorStaysUnderCursor()
    {
        const QSizeF image(4000, 3000), view(1920, 1200);
        const QPointF anchor(1500, 900);
        double scale = 1.0;
        QPointF offset = zoom::clampOffset({-1000, -800}, scale, image, view);
        const QPointF imagePoint = (anchor - offset) / scale;
        for (int i = 0; i < 12; ++i) {
            const double next = zoom::clampScale(scale * zoom::wheelFactor(i < 8 ? 120 : -120), image, view);
            offset = zoom::clampOffset(zoom::zoomAround(offset, scale, next, anchor), next, image, view);
            scale = next;
            const QPointF onScreen = offset + imagePoint * scale;
            QVERIFY2(QLineF(onScreen, anchor).length() < 2.0,
                     qPrintable(QStringLiteral("écart à l'étape %1 : %2").arg(i).arg(QLineF(onScreen, anchor).length())));
        }
    }

    void panIsBounded()
    {
        const QSizeF image(4000, 3000), view(1920, 1200);
        // Image plus grande que la vue : jamais de vide au-delà des bords.
        QCOMPARE(zoom::clampOffset({500, 500}, 1.0, image, view), QPointF(0, 0));
        QCOMPARE(zoom::clampOffset({-9000, -9000}, 1.0, image, view), QPointF(1920 - 4000, 1200 - 3000));
        // Image plus petite : centrée.
        QCOMPARE(zoom::clampOffset({0, 0}, 0.25, image, view), QPointF((1920 - 1000) / 2.0, (1200 - 750) / 2.0));
        QCOMPARE(zoom::centered(0.25, image, view), QPointF(460, 225));
    }
};

QTEST_GUILESS_MAIN(TstZoomMath)
#include "tst_zoommath.moc"

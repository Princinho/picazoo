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

    // Depuis l'ajustement (image plus petite que la vue, centrée), chaque cran de zoom
    // avant garde le point sous le curseur exactement en place, y compris à chaque image
    // intermédiaire de l'animation — l'image n'est plus recentrée en cours de route.
    void zoomInFromFitKeepsCursorPoint()
    {
        const QSizeF image(6000, 4000), view(1600, 900);
        const QSizeF fitView(view.width() - 48, view.height() - 48);
        double scale = zoom::fitScale(image, fitView);
        QPointF offset = zoom::clampOffset({}, scale, image, view);
        const QPointF anchor = offset + QPointF(600, 400) * scale; // haut gauche de l'image
        const QPointF imagePoint = (anchor - offset) / scale;
        for (int notch = 0; notch < 10; ++notch) {
            const double next = zoom::clampScale(scale * zoom::wheelFactor(120), image, fitView);
            const QPointF a = zoom::anchorOnImage(anchor, offset, scale, image);
            QCOMPARE(a, anchor);
            const QPointF end = zoom::zoomEndOffset(offset, scale, next, a, image, view);
            for (int f = 1; f <= 8; ++f) {
                const double current = scale + (next - scale) * f / 8.0;
                const QPointF o = zoom::zoomStepOffset(offset, scale, next, end, current, a);
                const double drift = QLineF(o + imagePoint * current, anchor).length();
                QVERIFY2(drift < 0.01, qPrintable(QStringLiteral("cran %1, image %2 : écart %3").arg(notch).arg(f).arg(drift)));
            }
            offset = end;
            scale = next;
        }
    }

    // Le zoom arrière ramène l'image dans ses bornes sans saut, centrée à l'ajustement.
    void zoomOutReturnsToCenteredFit()
    {
        const QSizeF image(6000, 4000), view(1600, 900);
        const QSizeF fitView(view.width() - 48, view.height() - 48);
        const double fit = zoom::fitScale(image, fitView);
        const QPointF anchor(300, 200);
        // État laissé par un zoom avant près d'un coin : du vide en haut à gauche.
        const double scale = fit * 1.25;
        const QPointF offset(150, 60);
        const QPointF end = zoom::zoomEndOffset(offset, scale, fit, anchor, image, view);
        QCOMPARE(end, zoom::centered(fit, image, view));
        // Première image de l'animation : aucun saut.
        const double first = scale + (fit - scale) / 100.0;
        QVERIFY(QLineF(zoom::zoomStepOffset(offset, scale, fit, end, first, anchor), offset).length() < 2.0);
        QCOMPARE(zoom::zoomStepOffset(offset, scale, fit, end, fit, anchor), end);
    }

    // Curseur à côté de l'image : on zoome autour du bord le plus proche, l'image reste visible.
    void anchorOutsideImageSnapsToEdge()
    {
        const QSizeF image(1000, 1000);
        const QPointF offset(300, 100);
        QCOMPARE(zoom::anchorOnImage({50, 400}, offset, 0.5, image), QPointF(300, 400));
        QCOMPARE(zoom::anchorOnImage({900, 900}, offset, 0.5, image), QPointF(800, 600));
    }

    // Le déplacement n'agrandit jamais un vide laissé par le zoom, mais peut le réduire.
    void panNeverWidensAGap()
    {
        const QSizeF image(4000, 3000), view(1920, 1200);
        const QPointF previous(100, -500); // 100 px de vide à gauche
        QCOMPARE(zoom::clampPan({300, -500}, previous, 1.0, image, view), QPointF(100, -500));
        QCOMPARE(zoom::clampPan({-50, -500}, previous, 1.0, image, view), QPointF(-50, -500));
        QCOMPARE(zoom::clampPan({-9000, 400}, previous, 1.0, image, view), QPointF(1920 - 4000, 0));
    }
};

QTEST_GUILESS_MAIN(TstZoomMath)
#include "tst_zoommath.moc"

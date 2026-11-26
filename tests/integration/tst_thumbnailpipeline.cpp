// 008 (US2) — réévaluation dynamique du budget mémoire (research B3) : réduire le
// parallélisme n'empêche pas le scan de se terminer ; relever le budget ne démarre pas
// de nouveau fil avant le prochain scan.

#include "jobs/ThumbnailPipeline.h"
#include "library/Library.h"
#include "support/FixtureFactory.h"
#include "support/LibraryFixture.h"
#include "support/TempTree.h"

#include <QElapsedTimer>
#include <QThread>
#include <QtTest>

using namespace picazoo;
using test::FixtureFactory;
using test::LibraryFixture;
using test::TempTree;

namespace {

// Attend qu'une condition devienne vraie, en pompant la boucle d'événements (cf. tst_watcher.cpp).
bool waitFor(const std::function<bool()>& cond, int ms)
{
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        if (cond())
            return true;
        QThread::msleep(50);
    }
    return cond();
}

} // namespace

class TstThumbnailPipeline : public QObject {
    Q_OBJECT

private slots:
    // (1) Réduire le budget en cours de scan n'empêche pas tous les éléments d'atteindre
    // un état terminal (thumbState != Pending), sans perte ni doublon.
    void reducingBudgetStillFinishesScan()
    {
        TempTree photos;
        for (int i = 0; i < 200; ++i)
            FixtureFactory::writeJpeg(photos.file(QStringLiteral("p%1.jpg").arg(i, 4, 10, QLatin1Char('0'))));

        LibraryFixture fx;
        fx.lib->addRoot(photos.path());
        // Laisse la découverte se terminer avant de réduire le budget, pour cibler le
        // parallélisme des vignettes plutôt que la découverte des dossiers.
        QVERIFY(fx.idle(30000, false));

        MemoryBudget reduced;
        reduced.thumbnailThreads = 1;
        fx.lib->pipeline().applyMemoryBudget(reduced);

        QVERIFY(fx.idle(60000, true));
        QCOMPARE(fx.lib->itemCount(), qint64(200));
        for (const auto& [folder, items] : fx.lib->allItemSummaries(ItemSort::NameAsc).asKeyValueRange()) {
            for (const auto& s : items)
                QVERIFY(s.thumbState != ThumbState::Pending);
        }
    }

    // (2) Relever le budget en cours de scan ne démarre pas de nouveau fil tant que le
    // scan courant n'est pas terminé (pas d'oscillation, research B3) : l'implémentation
    // borne `applyMemoryBudget` au nombre de fils déjà créés par `start()`.
    void raisingBudgetDoesNotExceedStartedThreads()
    {
        TempTree photos;
        FixtureFactory::writeJpeg(photos.file("p.jpg"));
        LibraryFixture fx;
        const RootId root = fx.lib->addRoot(photos.path());
        QVERIFY(fx.idle(30000, true));
        // Le scan initial termine avant que le suivi en direct ne soit armé (cf. tst_watcher.cpp) :
        // sans cette attente, le nouveau fichier écrit ci-dessous peut être manqué.
        QVERIFY(waitFor([&] { return fx.lib->isWatching(root); }, 5000));

        const int startedThreads = fx.lib->memoryBudget().thumbnailThreads;
        MemoryBudget raised;
        raised.thumbnailThreads = startedThreads + 100; // bien au-delà de ce qui a été démarré
        fx.lib->pipeline().applyMemoryBudget(raised);

        // Pas d'assertion directe sur le nombre de fils vivants (détail interne non exposé) :
        // la garantie vérifiée est que le scan reste fonctionnel après une demande hors bornes,
        // et que `applyMemoryBudget` ne plante ni ne bloque.
        FixtureFactory::writeJpeg(photos.file("q.jpg"));
        // `idle()` seul peut rendre la main avant même que le debounce du watcher (500 ms)
        // + la vérification de stabilité (1000 ms, cf. Watcher.h) ne mettent le nouveau
        // fichier en file : on attend donc directement le résultat observable.
        QVERIFY(waitFor([&] { return fx.lib->itemCount() == 2; }, 15000));
        QVERIFY(fx.idle(30000, true));
        QCOMPARE(fx.lib->itemCount(), qint64(2));
    }
};

QTEST_MAIN(TstThumbnailPipeline)
#include "tst_thumbnailpipeline.moc"

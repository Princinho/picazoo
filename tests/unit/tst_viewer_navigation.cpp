// T051 — navigation de la visionneuse à travers les dossiers, préchargement.

#include "library/GridLayout.h"
#include "library/Navigation.h"

#include <QtTest>

using namespace picazoo;

class TstViewerNavigation : public QObject {
    Q_OBJECT

private:
    // Deux dossiers : 1..3 puis 10..11, dans l'ordre d'affichage de la grille.
    static Navigation twoFolders()
    {
        Navigation n;
        n.setOrder({1, 2, 3, 10, 11});
        return n;
    }

private slots:
    void crossesFolders()
    {
        const Navigation n = twoFolders();
        QCOMPARE(n.next(3), ItemId(10));     // dernier du dossier 1 -> premier du dossier 2
        QCOMPARE(n.previous(10), ItemId(3)); // premier du dossier 2 -> dernier du dossier 1
    }

    void bounds()
    {
        const Navigation n = twoFolders();
        QCOMPARE(n.previous(1), ItemId(0));
        QCOMPARE(n.next(11), ItemId(0));
        QCOMPARE(n.next(999), ItemId(0));
    }

    void independentTestScenario()
    {
        // US2 : ouvrir la 3e photo d'un dossier de 10, 4 × suivant -> 7e.
        Navigation n;
        QList<ItemId> ids;
        for (int i = 1; i <= 10; ++i)
            ids << i;
        n.setOrder(ids);
        ItemId cur = 3;
        for (int k = 0; k < 4; ++k)
            cur = n.next(cur);
        QCOMPARE(cur, ItemId(7));
    }

    void prefetchOrder()
    {
        const Navigation n = twoFolders();
        QCOMPARE(n.prefetch(3), (QList<ItemId>{10, 2, 11, 1}));
        QCOMPARE(n.prefetch(1), (QList<ItemId>{2, 3}));
        QCOMPARE(n.prefetch(11), (QList<ItemId>{10, 3}));
    }
};

QTEST_GUILESS_MAIN(TstViewerNavigation)
#include "tst_viewer_navigation.moc"

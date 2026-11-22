// T030 — découpage de la grille aplatie.

#include "library/GridLayout.h"

#include <QtTest>

using namespace picazoo;

class TstGridRowModel : public QObject {
    Q_OBJECT

private slots:
    void columns()
    {
        GridParams p;
        p.width = 12 * 2 + 4 * 160 + 3 * 8; // exactement 4 vignettes
        p.thumbSize = 160;
        QCOMPARE(GridLayout::columnsFor(p), 4);
        p.width -= 1;
        QCOMPARE(GridLayout::columnsFor(p), 3);
        p.width = 10;
        QCOMPARE(GridLayout::columnsFor(p), 1);
    }

    void rowsPerFolder()
    {
        GridParams p;
        p.width = 12 * 2 + 4 * 100 + 3 * 8;
        p.thumbSize = 100;
        GridLayout g;
        g.build({{1, 9}, {2, 4}, {3, 0}, {4, 1}}, p);
        QCOMPARE(g.columns(), 4);
        // 1 : en-tête + 3 rangées (4,4,1) ; 2 : en-tête + 1 ; 3 : en-tête ; 4 : en-tête + 1
        QCOMPARE(g.rows().size(), 4 + 2 + 1 + 2);
        QCOMPARE(g.rows()[3].count, 1);
        QCOMPARE(g.rows()[3].first, 8);
        QCOMPARE(g.block(1), qMakePair(0, 4));
        QCOMPARE(g.block(2), qMakePair(4, 2));
        QCOMPARE(g.rowOfItem(1, 0), 1);
        QCOMPARE(g.rowOfItem(1, 7), 2);
        QCOMPARE(g.rowOfItem(1, 8), 3);
        QCOMPARE(g.rowOfItem(1, 9), -1);
        const double cell = 108;
        QCOMPARE(g.yOfFolder(2), 44 + 3 * cell);
        QCOMPARE(g.totalHeight(), 4 * 44 + 5 * cell);
    }

    void rowAtY()
    {
        GridParams p;
        p.width = 500;
        p.thumbSize = 100;
        GridLayout g;
        QCOMPARE(g.rowAtY(10), -1);
        QList<QPair<FolderId, int>> blocks;
        for (int i = 1; i <= 2000; ++i)
            blocks.append({i, 50});
        g.build(blocks, p);
        for (int r : {0, 1, 7, 500, int(g.rows().size()) - 1}) {
            const auto& row = g.rows()[r];
            QCOMPARE(g.rowAtY(row.y), r);
            QCOMPARE(g.rowAtY(row.y + row.height - 0.5), r);
        }
        QCOMPARE(g.rowAtY(-50), 0);
        QCOMPARE(g.rowAtY(1e12), int(g.rows().size()) - 1);
    }

    void rebuildOnResize()
    {
        GridParams p;
        p.thumbSize = 100;
        p.width = 12 * 2 + 2 * 100 + 8;
        GridLayout g;
        g.build({{1, 10}}, p);
        QCOMPARE(g.rows().size(), 1 + 5);
        p.width = 12 * 2 + 5 * 100 + 4 * 8;
        g.build({{1, 10}}, p);
        QCOMPARE(g.rows().size(), 1 + 2);
    }
};

QTEST_GUILESS_MAIN(TstGridRowModel)
#include "tst_gridrowmodel.moc"

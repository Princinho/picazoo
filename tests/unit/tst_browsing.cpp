// T071 — arborescence calculée à partir des chemins, sélection multiple, persistance
// des réglages (taille des vignettes, tri, mode de la liste).

#include "library/Library.h"
#include "models/FolderTreeModel.h"
#include "models/GridRowModel.h"
#include "models/ItemSelection.h"
#include "support/FixtureFactory.h"
#include "support/LibraryFixture.h"
#include "support/TempTree.h"

#include <QtTest>

using namespace picazoo;
using namespace picazoo::test;

namespace {

QStringList rowsOf(FolderTreeModel& m)
{
    QStringList out;
    for (int r = 0; r < m.rowCount(); ++r) {
        const QModelIndex i = m.index(r);
        out << QString(m.data(i, FolderTreeModel::DepthRole).toInt() * 2, u' ') +
                   m.data(i, FolderTreeModel::NameRole).toString() + u':' +
                   m.data(i, FolderTreeModel::RowKindRole).toString();
    }
    return out;
}

} // namespace

class TstBrowsing : public QObject {
    Q_OBJECT

private slots:
    void treeFromPaths()
    {
        TempTree photos;
        FixtureFactory::writeJpeg(photos.file("Racine/2019/Ete/a.jpg"));
        FixtureFactory::writeJpeg(photos.file("Racine/2019/Hiver/b.jpg"));
        FixtureFactory::writeJpeg(photos.file("Racine/c.jpg"));
        photos.mkdir("Racine/Vide/Rien");
        LibraryFixture fx;
        fx.lib->addRoot(photos.file("Racine"));
        QVERIFY(fx.idle());

        FolderTreeModel tree(*fx.lib);
        tree.setMode("tree");
        const QStringList rows = rowsOf(tree);
        // La racine (qui contient c.jpg), puis 2019 (dossier intermédiaire sans élément),
        // puis ses deux sous-dossiers ; la branche « Vide » n'apparaît pas.
        QCOMPARE(rows.size(), 4);
        QVERIFY(rows[0].endsWith(":folder"));
        QCOMPARE(rows[1], QStringLiteral("  2019:node"));
        QCOMPARE(rows[2], QStringLiteral("    Ete:folder"));
        QCOMPARE(rows[3], QStringLiteral("    Hiver:folder"));

        // Repli de 2019
        tree.toggle(1);
        QCOMPARE(tree.rowCount(), 2);
        // Retrouver un dossier replié le déplie.
        const auto folders = fx.lib->folders(FolderSort::NameAsc);
        const auto ete = std::find_if(folders.begin(), folders.end(), [](const Folder& f) { return f.name == u"Ete"; });
        QVERIFY(ete != folders.end());
        QCOMPARE(tree.rowOfFolder(ete->id), 2);

        tree.setMode("by_year");
        QVERIFY(rowsOf(tree).first().endsWith(":year"));
    }

    void multiSelection()
    {
        TempTree photos;
        for (int i = 0; i < 6; ++i)
            FixtureFactory::writeJpeg(photos.file(QStringLiteral("A/%1.jpg").arg(i)));
        for (int i = 0; i < 3; ++i)
            FixtureFactory::writeJpeg(photos.file(QStringLiteral("B/%1.jpg").arg(i)));
        LibraryFixture fx;
        fx.lib->setSetting("folder_sort", "name_asc");
        fx.lib->setSetting("item_sort", "name_asc");
        fx.lib->addRoot(photos.path());
        QVERIFY(fx.idle());
        GridRowModel grid(*fx.lib);
        QCOMPARE(grid.itemCount(), 9);
        ItemSelection sel(grid);

        const qint64 first = grid.itemAtFlat(0);
        const qint64 fourth = grid.itemAtFlat(3);
        const qint64 eighth = grid.itemAtFlat(7);
        sel.click(first, 0);
        QCOMPARE(sel.count(), 1);
        sel.click(fourth, 1); // Ctrl
        QCOMPARE(sel.count(), 2);
        sel.click(first, 0);
        sel.click(eighth, 2); // Maj : de A/0 à B/1, à travers les dossiers
        QCOMPARE(sel.count(), 8);
        sel.selectFolder(grid.folderOfItem(eighth));
        QCOMPARE(sel.count(), 3);

        // Sélection au cadre : 4 colonnes de 100 px ; cadre sur les 2 premières colonnes
        // de la première rangée de A (marge 12, en-tête 44).
        grid.setThumbSize(100);
        grid.setViewWidth(12 * 2 + 4 * 100 + 3 * 8);
        QCOMPARE(grid.columns(), 4);
        const QVariantList inRect = grid.itemsInRect(20, 44 + 10, 130, 44 + 90);
        QCOMPARE(inRect.size(), 2);
        QCOMPARE(inRect[0].toLongLong(), grid.itemAtFlat(0));
        QCOMPARE(inRect[1].toLongLong(), grid.itemAtFlat(1));
        sel.setSelection(inRect, false);
        QCOMPARE(sel.count(), 2);
        // Cadre sur deux rangées et deux dossiers (dernière rangée de A : 2 éléments ; B : 3 éléments).
        const QVariantList tall = grid.itemsInRect(0, 0, 2000, grid.totalHeight());
        QCOMPARE(tall.size(), 9);
        sel.clear();
        QCOMPARE(sel.count(), 0);
    }

    void settingsPersist()
    {
        LibraryFixture fx;
        {
            GridRowModel grid(*fx.lib);
            grid.setThumbSize(320);
            FolderTreeModel tree(*fx.lib);
            tree.setMode("tree");
            fx.lib->setSetting("folder_sort", "name_asc");
        }
        fx.reopen();
        GridRowModel grid(*fx.lib);
        FolderTreeModel tree(*fx.lib);
        QCOMPARE(grid.thumbSize(), 320);
        QCOMPARE(tree.mode(), QStringLiteral("tree"));
        QCOMPARE(fx.lib->setting("folder_sort").toString(), QStringLiteral("name_asc"));
    }
};

QTEST_MAIN(TstBrowsing)
#include "tst_browsing.moc"

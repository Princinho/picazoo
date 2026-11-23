// T015/T018 (005) — fusion de l'ordre de la fenêtre de l'Explorateur avec le listage.

#include "browse/BrowseOrder.h"

#include <QtTest>

using namespace picazoo;

namespace {

QList<BrowseEntry> listing(const QStringList& names)
{
    QList<BrowseEntry> out;
    for (const QString& n : names)
        out.push_back({QStringLiteral("C:\\d\\") + n, n, Format::Jpeg, 1, 1});
    return out;
}

QStringList names(const QList<BrowseEntry>& entries)
{
    QStringList out;
    for (const auto& e : entries)
        out << e.name;
    return out;
}

} // namespace

class TstBrowseOrder : public QObject {
    Q_OBJECT

private slots:
    void followsExplorerOrder()
    {
        const auto list = listing({"a.jpg", "b.jpg", "c.jpg", "d.jpg"});
        // Tri par date dans l'Explorateur, avec un texte et un dossier entre les images.
        const QStringList view{"c.jpg", "notes.txt", "A.JPG", "Vacances", "d.jpg", "b.jpg"};
        QCOMPARE(names(mergeOrder(view, list)), (QStringList{"c.jpg", "a.jpg", "d.jpg", "b.jpg"}));
    }

    void imagesHiddenByAFilterGoLast()
    {
        const auto list = listing({"a.jpg", "b.jpg", "c.jpg", "d.jpg"});
        QCOMPARE(names(mergeOrder({"d.jpg", "b.jpg"}, list)), (QStringList{"d.jpg", "b.jpg", "a.jpg", "c.jpg"}));
    }

    void emptyViewKeepsNaturalOrder()
    {
        const auto list = listing({"a.jpg", "b.jpg"});
        QCOMPARE(names(mergeOrder({}, list)), (QStringList{"a.jpg", "b.jpg"}));
        QCOMPARE(names(mergeOrder({"b.jpg", "b.jpg"}, list)), (QStringList{"b.jpg", "a.jpg"})); // doublon ignoré
    }
};

QTEST_GUILESS_MAIN(TstBrowseOrder)
#include "tst_browseorder.moc"

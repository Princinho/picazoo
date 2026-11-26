// 003 — images liées sur une vraie bibliothèque : premier repérage (US1), décisions durables
// (US2), suivi des ajouts (US3), persistance (US4). Aucune écriture dans les dossiers (SC-006).

#include "duplicates/DuplicateEngine.h"
#include "duplicates/DuplicateStore.h"
#include "library/Library.h"
#include "naming/NamingEngine.h"
#include "naming/NamingStore.h"
#include "support/FixtureFactory.h"
#include "support/FsSnapshot.h"
#include "support/LibraryFixture.h"
#include "support/TempTree.h"

#include <QElapsedTimer>
#include <QThread>
#include <QtTest>

#include <memory>

using namespace picazoo;
using namespace picazoo::test;

namespace {

// Bibliothèque, dépôt et moteur ; les deux derniers sont détruits avant la bibliothèque.
struct Naming {
    LibraryFixture fx;
    std::unique_ptr<NamingStore> store;
    std::unique_ptr<NamingEngine> engine;

    Naming() { start(); }
    ~Naming() { stop(); }

    void start()
    {
        store = std::make_unique<NamingStore>(fx.data.path() + QStringLiteral("/library.db"));
        engine = std::make_unique<NamingEngine>(*fx.lib, *store);
    }
    void stop()
    {
        engine.reset();
        store.reset();
    }
    void restart()
    {
        stop();
        fx.reopen();
        start();
    }
    bool idle() { return engine->waitForIdle(30000); }

    std::optional<NamingGroup> named(const QString& name) const
    {
        for (NamingGroupId id : store->groups()) {
            const auto g = store->group(id);
            if (g && g->displayName == name)
                return g;
        }
        return std::nullopt;
    }
    QStringList names() const
    {
        QStringList out;
        for (NamingGroupId id : store->groups())
            out << store->group(id)->displayName;
        return out;
    }
    QStringList files(const QString& group) const
    {
        QStringList out;
        if (const auto g = named(group))
            for (const auto& m : g->members)
                out << m.fileName;
        return out;
    }
    ItemId item(const QString& fileName) const
    {
        for (const auto& f : fx.lib->folders(FolderSort::NameAsc))
            for (const auto& s : fx.lib->itemSummaries(f.id, ItemSort::NameAsc))
                if (s.fileName == fileName)
                    return s.id;
        return 0;
    }
};

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

void writeSeries(const TempTree& photos, const QString& pattern, int from, int to)
{
    for (int i = from; i <= to; ++i)
        FixtureFactory::writeJpeg(photos.file(pattern.arg(i, 3, 10, QLatin1Char('0'))));
}

// Deux événements et des noms d'appareil.
void writeLibrary(const TempTree& photos)
{
    writeSeries(photos, QStringLiteral("Bali/vacances_bali_%1.jpg"), 1, 5);
    writeSeries(photos, QStringLiteral("Fete/Anniversaire Julie (%1).jpg"), 1, 3);
    writeSeries(photos, QStringLiteral("Divers/IMG_%1.jpg"), 1, 5);
    for (const char* name : {"chat", "porte", "ciel", "table", "arbre"})
        FixtureFactory::writeJpeg(photos.file(QStringLiteral("Divers/%1.jpg").arg(QLatin1String(name))));
}

} // namespace

class TstNamingEngine : public QObject {
    Q_OBJECT

private slots:
    // US1 (T010)
    void firstScanGroupsEventSeriesOnly()
    {
        TempTree photos;
        writeLibrary(photos);
        const FsSnapshot before = FsSnapshot::take(photos.path());
        Naming n;
        n.fx.lib->addRoot(photos.path());
        QVERIFY(n.idle());

        QCOMPARE(n.names(), (QStringList{"Anniversaire Julie", "vacances bali"}));
        QCOMPARE(n.store->summary().groupCount, 2);
        QCOMPARE(n.files("vacances bali").size(), 5);
        QCOMPARE(n.named("vacances bali")->confidence, Confidence::RegularSeries);
        QCOMPARE(n.store->groupOf(n.item("vacances_bali_001.jpg")), n.named("vacances bali")->id);
        QVERIFY(!n.store->groupOf(n.item("IMG_001.jpg")));
        const auto row = n.store->rowOf(n.item("vacances_bali_001.jpg"));
        QVERIFY(row.has_value());
        QCOMPARE(row->displayName, QStringLiteral("vacances bali"));
        QCOMPARE(row->memberCount, 5);
        QVERIFY(!n.store->rowOf(n.item("chat.jpg")));
        PICAZOO_VERIFY_UNTOUCHED(before, photos.path());
    }

    void genericRootsFollowTheSetting()
    {
        TempTree photos;
        writeLibrary(photos);
        Naming n;
        n.fx.lib->addRoot(photos.path());
        QVERIFY(n.idle());

        n.fx.lib->setSetting(QStringLiteral("naming_include_generic_roots"), 1);
        n.engine->rescanAll();
        QVERIFY(n.idle());
        QCOMPARE(n.store->summary().genericRoot, 1);
        QCOMPARE(n.files("IMG").size(), 5);

        n.fx.lib->setSetting(QStringLiteral("naming_include_generic_roots"), 0);
        n.engine->rescanAll();
        QVERIFY(n.idle());
        QCOMPARE(n.store->summary().groupCount, 2);
    }

    // US2 (T016)
    void removeAndAddSurviveRescan()
    {
        TempTree photos;
        writeLibrary(photos);
        const FsSnapshot before = FsSnapshot::take(photos.path());
        Naming n;
        n.fx.lib->addRoot(photos.path());
        QVERIFY(n.idle());
        const NamingGroupId bali = n.named("vacances bali")->id;

        QVERIFY(n.store->removeFromGroup(bali, n.item("vacances_bali_003.jpg")));
        QVERIFY(n.store->addToGroup(bali, n.item("chat.jpg")));
        n.engine->rescanAll();
        QVERIFY(n.idle());

        const QStringList files = n.files("vacances bali");
        QCOMPARE(files.size(), 5);
        QVERIFY(files.contains("chat.jpg"));
        QVERIFY(!files.contains("vacances_bali_003.jpg"));
        QCOMPARE(n.named("vacances bali")->state, NamingGroupState::Confirmed);

        // Remis à la main, il revient.
        QVERIFY(n.store->addToGroup(bali, n.item("vacances_bali_003.jpg")));
        QVERIFY(n.idle());
        QCOMPARE(n.files("vacances bali").size(), 6);
        PICAZOO_VERIFY_UNTOUCHED(before, photos.path());
    }

    void mergeSurvivesRescanAndCanBeUndone()
    {
        TempTree photos;
        writeLibrary(photos);
        const FsSnapshot before = FsSnapshot::take(photos.path());
        Naming n;
        n.fx.lib->addRoot(photos.path());
        QVERIFY(n.idle());
        const NamingGroupId bali = n.named("vacances bali")->id;

        QCOMPARE(n.store->mergeGroups(bali, n.named("Anniversaire Julie")->id), bali);
        QVERIFY(n.store->rename(bali, QStringLiteral("Été 2024")));
        n.engine->rescanAll();
        QVERIFY(n.idle());
        QCOMPARE(n.names(), QStringList{"Été 2024"});
        QCOMPARE(n.files("Été 2024").size(), 8);

        const auto decisions = n.store->decisions();
        QCOMPARE(decisions.size(), 1);
        QCOMPARE(decisions.first().kind, NamingDecision::Kind::MergedInto);
        QCOMPARE(decisions.first().targetName, QStringLiteral("Été 2024"));

        // Une nouvelle photo de la racine absorbée rejoint le groupe fusionné.
        writeSeries(photos, QStringLiteral("Fete/Anniversaire Julie (%1).jpg"), 4, 4);
        n.fx.lib->startScan();
        QVERIFY(n.idle());
        QCOMPARE(n.files("Été 2024").size(), 9);

        QVERIFY(n.store->undoDecision(decisions.first().rootKey));
        QVERIFY(n.idle());
        QCOMPARE(n.files("Été 2024").size(), 5);
        QCOMPARE(n.files("Anniversaire Julie").size(), 4);
        QCOMPARE(before.diff(FsSnapshot::take(photos.path())).size(), 1); // la photo ajoutée par le test
    }

    void removingAnAddedItemSendsItHome()
    {
        TempTree photos;
        writeLibrary(photos);
        Naming n;
        n.fx.lib->addRoot(photos.path());
        QVERIFY(n.idle());
        const NamingGroupId bali = n.named("vacances bali")->id;
        const ItemId julie = n.item("Anniversaire Julie (001).jpg");

        QVERIFY(n.store->addToGroup(bali, julie));
        QVERIFY(n.idle());
        QCOMPARE(n.files("Anniversaire Julie").size(), 2);
        QVERIFY(n.store->removeFromGroup(bali, julie));
        n.engine->rescanAll();
        QVERIFY(n.idle());
        QCOMPARE(n.files("Anniversaire Julie").size(), 3);
        QCOMPARE(n.files("vacances bali").size(), 5);

        // Même chose quand le groupe qui l'avait reçu est exclu.
        QVERIFY(n.store->addToGroup(bali, julie));
        QVERIFY(n.idle());
        QVERIFY(n.store->markNotAGroup(bali));
        n.engine->rescanAll();
        QVERIFY(n.idle());
        QCOMPARE(n.files("Anniversaire Julie").size(), 3);
    }

    void chainedMergesUndoOneAtATime()
    {
        TempTree photos;
        writeLibrary(photos);
        writeSeries(photos, QStringLiteral("Noel/noel_%1.jpg"), 1, 3);
        Naming n;
        n.fx.lib->addRoot(photos.path());
        QVERIFY(n.idle());

        QVERIFY(n.store->mergeGroups(n.named("Anniversaire Julie")->id, n.named("noel")->id) != 0);
        QVERIFY(n.store->mergeGroups(n.named("vacances bali")->id, n.named("Anniversaire Julie")->id) != 0);
        QVERIFY(n.idle());
        QCOMPARE(n.files("vacances bali").size(), 11);

        auto decisionFor = [&](const QString& label) {
            for (const auto& d : n.store->decisions())
                if (d.label == label)
                    return d;
            return NamingDecision{};
        };
        QCOMPARE(decisionFor("noel").targetName, QStringLiteral("vacances bali"));
        QVERIFY(n.store->undoDecision(decisionFor("Anniversaire Julie").rootKey));
        QVERIFY(n.idle());
        QCOMPARE(n.files("vacances bali").size(), 5);
        QCOMPARE(n.files("Anniversaire Julie").size(), 6); // noel la suit

        QVERIFY(n.store->undoDecision(decisionFor("noel").rootKey));
        QVERIFY(n.idle());
        QCOMPARE(n.files("Anniversaire Julie").size(), 3);
        QCOMPARE(n.files("noel").size(), 3);
    }

    void splitSurvivesRescan()
    {
        TempTree photos;
        writeLibrary(photos);
        const FsSnapshot before = FsSnapshot::take(photos.path());
        Naming n;
        n.fx.lib->addRoot(photos.path());
        QVERIFY(n.idle());
        const NamingGroupId bali = n.named("vacances bali")->id;
        const QList<ItemId> part{n.item("vacances_bali_004.jpg"), n.item("vacances_bali_005.jpg")};

        // Chaque partie doit garder au moins deux éléments.
        QCOMPARE(n.store->splitGroup(bali, {part.first()}, QStringLiteral("Seul")), NamingGroupId(0));
        const NamingGroupId plage = n.store->splitGroup(bali, part, QStringLiteral("Plage"));
        QVERIFY(plage != 0);
        n.engine->rescanAll();
        QVERIFY(n.idle());
        QCOMPARE(n.files("vacances bali").size(), 3);
        QCOMPARE(n.files("Plage"), (QStringList{"vacances_bali_004.jpg", "vacances_bali_005.jpg"}));

        // Un troisième élément de la racine rejoint le groupe d'origine.
        writeSeries(photos, QStringLiteral("Bali/vacances_bali_%1.jpg"), 6, 6);
        n.fx.lib->startScan();
        QVERIFY(n.idle());
        QCOMPARE(n.files("vacances bali").size(), 4);
        QCOMPARE(n.files("Plage").size(), 2);

        // Écarter le groupe scindé ne fait pas revenir ses éléments ; l'annulation, si.
        QVERIFY(n.store->markNotAGroup(plage));
        n.engine->rescanAll();
        QVERIFY(n.idle());
        QCOMPARE(n.files("vacances bali").size(), 4);
        QVERIFY(!n.store->groupOf(part.first()));
        QVERIFY(n.store->undoDecision(n.store->decisions().first().rootKey));
        QVERIFY(n.idle());
        QCOMPARE(n.files("vacances bali").size(), 6);
        QCOMPARE(before.diff(FsSnapshot::take(photos.path())).size(), 1);
    }

    void rejectedRootStaysRejectedUntilUndone()
    {
        TempTree photos;
        writeLibrary(photos);
        const FsSnapshot before = FsSnapshot::take(photos.path());
        Naming n;
        n.fx.lib->addRoot(photos.path());
        QVERIFY(n.idle());

        QVERIFY(n.store->markNotAGroup(n.named("vacances bali")->id));
        n.engine->rescanAll();
        QVERIFY(n.idle());
        QCOMPARE(n.names(), QStringList{"Anniversaire Julie"});
        PICAZOO_VERIFY_UNTOUCHED(before, photos.path());

        writeSeries(photos, QStringLiteral("Bali/vacances_bali_%1.jpg"), 6, 6);
        n.fx.lib->startScan();
        QVERIFY(n.idle());
        QCOMPARE(n.names(), QStringList{"Anniversaire Julie"});

        const auto decisions = n.store->decisions();
        QCOMPARE(decisions.size(), 1);
        QCOMPARE(decisions.first().label, QStringLiteral("vacances bali"));
        QVERIFY(n.store->undoDecision(decisions.first().rootKey));
        QVERIFY(n.idle());
        QCOMPARE(n.files("vacances bali").size(), 6);
    }

    // US3 (T021)
    void watchedAdditionsJoinWithoutFullRescan()
    {
        TempTree photos;
        writeLibrary(photos);
        Naming n;
        const RootId root = n.fx.lib->addRoot(photos.path());
        QVERIFY(n.idle());
        QVERIFY(waitFor([&] { return n.fx.lib->isWatching(root); }, 5000));
        const int fullScans = n.engine->fullScanCount();
        QCOMPARE(fullScans, 1);

        writeSeries(photos, QStringLiteral("Bali/vacances_bali_%1.jpg"), 6, 6);
        QVERIFY2(waitFor([&] { return n.files("vacances bali").size() == 6; }, 30000),
                 qPrintable(n.files("vacances bali").join(", ")));
        // Une série entièrement nouvelle forme un groupe.
        writeSeries(photos, QStringLiteral("Noel/noel_%1.jpg"), 1, 2);
        QVERIFY(waitFor([&] { return n.files("noel").size() == 2; }, 30000));
        // Une suppression retire l'élément ; un groupe tombé à un élément disparaît.
        QVERIFY(QFile::remove(photos.file("Noel/noel_002.jpg")));
        QVERIFY(waitFor([&] { return !n.named("noel"); }, 30000));
        // Un groupe jamais corrigé suit le nom des fichiers.
        QVERIFY(QFile::rename(photos.file("Bali/vacances_bali_006.jpg"), photos.file("Bali/sans rapport.jpg")));
        QVERIFY(waitFor([&] { return n.files("vacances bali").size() == 5; }, 30000));
        QCOMPARE(n.engine->fullScanCount(), fullScans);
    }

    void removedRootEmptiesItsGroups()
    {
        TempTree bali, fete;
        writeSeries(bali, QStringLiteral("vacances_bali_%1.jpg"), 1, 3);
        writeSeries(fete, QStringLiteral("fete_%1.jpg"), 1, 3);
        Naming n;
        const RootId first = n.fx.lib->addRoot(bali.path());
        n.fx.lib->addRoot(fete.path());
        QVERIFY(n.idle());
        QCOMPARE(n.store->summary().groupCount, 2);

        n.fx.lib->removeRoot(first);
        QVERIFY(n.idle());
        QCOMPARE(n.names(), QStringList{"fete"});
    }

    // FR-010 : un élément figure à la fois dans un groupe de doublons et d'images liées.
    void coexistsWithDuplicates()
    {
        TempTree photos;
        const QImage scene = FixtureFactory::scene(QSize(640, 480), 7);
        FixtureFactory::writeImage(photos.file("a/souvenir_001.jpg"), scene);
        QVERIFY(QDir().mkpath(photos.file("b")));
        QVERIFY(QFile::copy(photos.file("a/souvenir_001.jpg"), photos.file("b/souvenir_002.jpg")));
        Naming n;
        DuplicateEngine::Options options;
        options.respectBattery = false;
        options.autoStart = false;
        DuplicateEngine duplicates(*n.fx.lib, options);
        n.fx.lib->addRoot(photos.path());
        QVERIFY(duplicates.waitForIdle(30000));
        QVERIFY(n.idle());

        const ItemId first = n.item("souvenir_001.jpg");
        QVERIFY(duplicates.store().groupOf(first).has_value());
        QVERIFY(n.store->groupOf(first).has_value());
        QCOMPARE(duplicates.store().summary().groups, 1);
        QCOMPARE(n.store->summary().groupCount, 1);
        // Une décision sur les images liées ne touche pas aux doublons.
        QVERIFY(n.store->markNotAGroup(*n.store->groupOf(first)));
        QVERIFY(n.idle());
        QVERIFY(duplicates.store().groupOf(first).has_value());
    }

    // US4 (T024)
    void decisionsSurviveRestartAndDiskRename()
    {
        TempTree photos;
        writeLibrary(photos);
        Naming n;
        n.fx.lib->addRoot(photos.path());
        QVERIFY(n.idle());
        const ItemId renamed = n.item("vacances_bali_002.jpg");
        QVERIFY(n.store->rename(n.named("vacances bali")->id, QStringLiteral("Bali 2024")));
        QVERIFY(n.idle());

        n.stop();
        n.fx.close();
        QVERIFY(QFile::rename(photos.file("Bali/vacances_bali_002.jpg"), photos.file("Fete/coucher de soleil.jpg")));
        n.fx.open();
        n.start();
        QVERIFY(n.idle());

        const auto g = n.named("Bali 2024");
        QVERIFY(g.has_value());
        QVERIFY(g->renamed);
        QCOMPARE(g->state, NamingGroupState::Confirmed);
        QCOMPARE(g->members.size(), 5);
        QCOMPARE(n.item("coucher de soleil.jpg"), renamed);
        QCOMPARE(n.store->groupOf(renamed), g->id);
        QCOMPARE(n.store->summary().groupCount, 2);
    }
};

QTEST_MAIN(TstNamingEngine)
#include "tst_naming_engine.moc"

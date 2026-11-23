// 002 — mise à la corbeille réelle, sur des fichiers de test (principe I, US2) : rien sans
// plan, jamais d'effacement définitif, vérification juste avant d'agir, au moins une copie
// gardée, journal, annulation et retour à l'identique (SC-008).

#include "duplicates/DuplicateEngine.h"
#include "duplicates/DuplicateStore.h"
#include "fileops/TrashService.h"
#include "library/Library.h"
#include "platform/win/FileIdentity.h"
#include "platform/win/RecycleBin.h"
#include "support/FixtureFactory.h"
#include "support/FsSnapshot.h"
#include "support/LibraryFixture.h"
#include "support/RecycleBinCleanup.h"
#include "support/TempTree.h"

#include <QFile>
#include <QFileInfo>
#include <QtTest>

#include <memory>

using namespace picazoo;
using namespace picazoo::test;

namespace {

DuplicateEngine::Options testOptions()
{
    DuplicateEngine::Options o;
    o.respectBattery = false;
    o.autoStart = false;
    return o;
}

struct Rig {
    TempTree photos;
    QByteArray content;
    LibraryFixture fx;
    std::unique_ptr<DuplicateEngine> engine;
    std::unique_ptr<TrashService> trash;

    // `extra` écrit d'autres fichiers avant l'ajout du dossier à la bibliothèque (006).
    explicit Rig(const std::function<void(Rig&)>& extra = {})
    {
        content = FixtureFactory::jpegOf(FixtureFactory::scene(QSize(800, 600), 5));
        FixtureFactory::writeBytes(photos.file("A/photo.jpg"), content);
        FixtureFactory::writeBytes(photos.file("B/photo (copie).jpg"), content);
        FixtureFactory::writeBytes(photos.file("C/photo-2.jpg"), content);
        FixtureFactory::writeImage(photos.file("A/autre.jpg"), FixtureFactory::scene(QSize(800, 600), 6));
        if (extra)
            extra(*this);
        fx.lib->addRoot(photos.path());
        engine = std::make_unique<DuplicateEngine>(*fx.lib, testOptions());
        trash = std::make_unique<TrashService>(*fx.lib, *engine);
    }
    ~Rig()
    {
        trash.reset();
        engine.reset();
        fx.close();
        purgeRecycledUnder(photos.path()); // ne laisse rien dans la corbeille de l'utilisateur
    }
    bool idle() { return fx.idle(60000) && engine->waitForIdle(60000); }
    GroupId onlyGroup()
    {
        const auto g = engine->store().groups();
        return g.size() == 1 ? g.first() : 0;
    }
};

} // namespace

class TstTrash : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        TempTree probe;
        if (!platform::volumeHasRecycleBin(probe.path()))
            QSKIP("Le disque des fichiers temporaires n'a pas de corbeille.");
    }

    void trashThenUndoRestoresEverything()
    {
        Rig rig;
        QVERIFY(rig.idle());
        const FsSnapshot before = FsSnapshot::take(rig.photos.path());
        const GroupId gid = rig.onlyGroup();
        QVERIFY(gid);

        const TrashPlan plan = rig.trash->plan({gid});
        QCOMPARE(plan.toTrash.size(), 2);
        QVERIFY(plan.blocked.isEmpty());
        QVERIFY(plan.refused.isEmpty());
        QCOMPARE(plan.bytes, 2 * rig.content.size());
        // Le plan ne touche à rien.
        PICAZOO_VERIFY_UNTOUCHED(before, rig.photos.path());

        const OperationId op = rig.trash->execute(plan);
        QVERIFY(op);
        QVERIFY(rig.trash->waitForIdle(60000));
        QVERIFY(rig.idle());
        auto done = rig.trash->operation(op);
        QVERIFY(done);
        QCOMPARE(done->state, OperationState::Done);
        int inBin = 0;
        for (const auto& e : done->entries) {
            QCOMPARE(e.result, QStringLiteral("trashed"));
            QVERIFY(!QFileInfo::exists(e.path));
            for (const auto& r : platform::recycledEntries(e.path))
                inBin += platform::samePath(r.originalPath, e.path);
        }
        QCOMPARE(inBin, 2); // dans la corbeille, pas effacés
        QCOMPARE(rig.fx.lib->itemCount(), 2); // la bibliothèque a suivi (FR-023)
        QCOMPARE(rig.engine->store().summary().groups, 0);

        const OperationId back = rig.trash->undo(op);
        QVERIFY(back);
        QVERIFY(rig.trash->waitForIdle(60000));
        QCOMPARE(rig.trash->operation(op)->state, OperationState::Undone);
        for (const auto& e : rig.trash->operation(op)->entries)
            QCOMPARE(e.result, QStringLiteral("restored"));
        PICAZOO_VERIFY_UNTOUCHED(before, rig.photos.path()); // SC-008 : retour à l'identique
        QVERIFY(rig.idle());
        QCOMPARE(rig.fx.lib->itemCount(), 4);
        QCOMPARE(rig.engine->store().summary().groups, 1);
        QCOMPARE(rig.trash->journal().size(), 2);
    }

    void changedFileIsLeftInPlace()
    {
        Rig rig;
        QVERIFY(rig.idle());
        const TrashPlan plan = rig.trash->plan({rig.onlyGroup()});
        QCOMPARE(plan.toTrash.size(), 2);
        // Un autre logiciel modifie une des copies entre l'analyse et l'action.
        const QString touched = plan.toTrash.first().path;
        {
            QFile f(touched);
            QVERIFY(f.open(QIODevice::Append));
            f.write("modifié");
        }
        const OperationId op = rig.trash->execute(plan);
        QVERIFY(op);
        QVERIFY(rig.trash->waitForIdle(60000));
        const auto done = rig.trash->operation(op);
        QCOMPARE(done->state, OperationState::Partial);
        for (const auto& e : done->entries) {
            if (platform::samePath(e.path, touched)) {
                QCOMPARE(e.result, QStringLiteral("skipped_changed"));
                QVERIFY(QFileInfo::exists(touched));
            } else {
                QCOMPARE(e.result, QStringLiteral("trashed"));
            }
        }
    }

    void neverTrashesEveryCopy()
    {
        Rig rig;
        QVERIFY(rig.idle());
        const GroupId gid = rig.onlyGroup();
        const auto g = *rig.engine->store().group(gid);
        // Plan forgé à la main qui écarterait toutes les copies : refusé par le service.
        TrashPlan forged;
        for (const auto& c : g.copies) {
            TrashPlanEntry e;
            e.item = c.item;
            e.group = gid;
            e.path = rig.fx.lib->item(c.item)->path();
            e.keeper = g.center;
            e.keeperPath = rig.fx.lib->item(g.center)->path();
            forged.toTrash << e;
        }
        QCOMPARE(rig.trash->execute(forged), 0);
        for (const auto& e : forged.toTrash)
            QVERIFY(QFileInfo::exists(e.path));
        QVERIFY(rig.trash->journal().isEmpty());
    }

    void planAllIdenticalCoversIdenticalGroups()
    {
        Rig rig;
        QVERIFY(rig.idle());
        const TrashPlan plan = rig.trash->planAllIdentical();
        QCOMPARE(plan.toTrash.size(), 2);
        for (const auto& e : plan.toTrash)
            QVERIFY(e.identical);
    }

    // ------------------------------------------------------------------------- 006

    void planCoversOnlyRequestedGroups()
    {
        // Un second groupe, dans un autre dossier.
        Rig rig([](Rig& r) {
            const QByteArray other = FixtureFactory::jpegOf(FixtureFactory::scene(QSize(800, 600), 9));
            FixtureFactory::writeBytes(r.photos.file("D/second.jpg"), other);
            FixtureFactory::writeBytes(r.photos.file("E/second.jpg"), other);
        });
        QVERIFY(rig.idle());
        const QList<GroupId> groups = rig.engine->store().groups();
        QCOMPARE(groups.size(), 2);
        for (GroupId gid : groups) {
            const TrashPlan plan = rig.trash->plan({gid});
            for (const auto& e : plan.toTrash)
                QCOMPARE(e.group, gid); // SC-004 : rien hors des groupes demandés
            const SelectionTotals t = rig.engine->store().totals({gid});
            QCOMPARE(t.files, plan.toTrash.size());
            QCOMPARE(t.bytes, plan.bytes);
            for (const auto& e : plan.toTrash) {
                QCOMPARE(e.level, DupLevel::Identical);
                QVERIFY(!e.examined);
            }
        }
        // Un choix fait à la main rend le groupe « examiné » ; filtered() garde un sous-plan cohérent.
        const auto g = *rig.engine->store().group(groups.first());
        QVERIFY(rig.engine->store().setKeep(groups.first(), g.copies.last().item, true));
        const TrashPlan all = rig.trash->plan(groups);
        const TrashPlan examined = all.filtered([](const TrashPlanEntry& e) { return e.examined; });
        QVERIFY(!examined.toTrash.isEmpty());
        QVERIFY(examined.toTrash.size() < all.toTrash.size());
        qint64 bytes = 0;
        for (const auto& e : examined.toTrash)
            bytes += e.bytes;
        QCOMPARE(examined.bytes, bytes);
    }

    void planKeeperIsTheSuggestedCopy()
    {
        Rig rig;
        QVERIFY(rig.idle());
        const GroupId gid = rig.onlyGroup();
        // Règle : préférer le dossier C, qui ne contient pas le centre (chemin le plus court en A).
        rig.engine->store().setFolderRule(rig.photos.file("C"), RuleKind::Prefer);
        const auto g = *rig.engine->store().group(gid);
        QVERIFY(g.suggested != g.center);
        const TrashPlan plan = rig.trash->plan({gid});
        QCOMPARE(plan.toTrash.size(), 2);
        for (const auto& e : plan.toTrash) {
            QCOMPARE(e.keeper, g.suggested);
            QVERIFY(e.keeperPath.contains(QStringLiteral("\\C\\")));
            QVERIFY(!e.path.contains(QStringLiteral("\\C\\")));
        }
    }

    void stopLeavesAnExactJournalAndUndoes()
    {
        Rig rig([](Rig& r) {
            for (int i = 0; i < 40; ++i)
                FixtureFactory::writeBytes(r.photos.file(QStringLiteral("Lot/copie-%1.jpg").arg(i)), r.content);
        });
        QVERIFY(rig.idle());
        const FsSnapshot before = FsSnapshot::take(rig.photos.path());
        const GroupId gid = rig.onlyGroup();
        QVERIFY(gid);
        const TrashPlan plan = rig.trash->plan({gid});
        QCOMPARE(plan.toTrash.size(), 42);

        const OperationId op = rig.trash->execute(plan);
        QVERIFY(op);
        rig.trash->requestStop();
        QVERIFY(rig.trash->waitForIdle(60000));
        const auto done = rig.trash->operation(op);
        QVERIFY(done->stopped);
        int trashed = 0, pending = 0;
        for (const auto& e : done->entries) {
            if (e.result == u"trashed") {
                ++trashed;
                QVERIFY(!QFileInfo::exists(e.path));
            } else {
                QCOMPARE(e.result, QStringLiteral("pending"));
                QVERIFY(QFileInfo::exists(e.path)); // jamais touché
                ++pending;
            }
        }
        QCOMPARE(trashed + pending, 42);
        QVERIFY(pending > 0);
        QCOMPARE(done->state, trashed > 0 ? OperationState::Partial : OperationState::Failed);
        QVERIFY(rig.trash->journal().first().stopped);

        if (trashed > 0) {
            QVERIFY(rig.trash->undo(op));
            QVERIFY(rig.trash->waitForIdle(60000));
        }
        PICAZOO_VERIFY_UNTOUCHED(before, rig.photos.path());
        // Sans opération en cours : sans effet.
        rig.trash->requestStop();
        QVERIFY(!rig.trash->busy());
    }
};

QTEST_MAIN(TstTrash)
#include "tst_trash.moc"

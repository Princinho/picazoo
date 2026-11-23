// 006 — gestion des doublons en lot sur une vraie bibliothèque de test : actions en lot et
// annulation, filtres combinés, totaux, règles de dossier, paires de dossiers, « Écartés de la
// revue », migration 2 → 3. Aucun fichier de l'utilisateur n'est touché (rien n'est mis à la
// corbeille ici).

#include "duplicates/DuplicateEngine.h"
#include "duplicates/DuplicateStore.h"
#include "index/Database.h"
#include "index/Migrator.h"
#include "library/Library.h"
#include "support/FixtureFactory.h"
#include "support/FsSnapshot.h"
#include "support/LibraryFixture.h"
#include "support/TempTree.h"

#include <QDir>
#include <QFile>
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

// Images\2019 (3 photos) et Images\2020 (2 photos) recopiées dans Sauvegarde\2019 et
// Sauvegarde\2020, un doublon interne dans Images\2019 et une photo sans copie : 6 groupes.
struct Rig {
    TempTree photos;
    LibraryFixture fx;
    std::unique_ptr<DuplicateEngine> engine;

    Rig()
    {
        int seed = 100;
        auto pair = [&](const QString& year, const QString& name) {
            const QByteArray bytes = FixtureFactory::jpegOf(FixtureFactory::scene(QSize(480, 360), quint32(seed++)));
            FixtureFactory::writeBytes(photos.file(QStringLiteral("Images/%1/%2").arg(year, name)), bytes);
            FixtureFactory::writeBytes(photos.file(QStringLiteral("Sauvegarde/%1/%2").arg(year, name)), bytes);
        };
        pair(QStringLiteral("2019"), QStringLiteral("a0.jpg"));
        pair(QStringLiteral("2019"), QStringLiteral("a1.jpg"));
        pair(QStringLiteral("2019"), QStringLiteral("a2.jpg"));
        pair(QStringLiteral("2020"), QStringLiteral("b0.jpg"));
        pair(QStringLiteral("2020"), QStringLiteral("b1.jpg"));
        const QByteArray internal = FixtureFactory::jpegOf(FixtureFactory::scene(QSize(480, 360), 200));
        FixtureFactory::writeBytes(photos.file(QStringLiteral("Images/2019/c.jpg")), internal);
        FixtureFactory::writeBytes(photos.file(QStringLiteral("Images/2019/c (1).jpg")), internal);
        FixtureFactory::writeImage(photos.file(QStringLiteral("Images/2019/solo.jpg")),
                                   FixtureFactory::scene(QSize(480, 360), 300));
        fx.lib->addRoot(photos.path());
        engine = std::make_unique<DuplicateEngine>(*fx.lib, testOptions());
    }
    ~Rig()
    {
        engine.reset();
        fx.close();
    }
    bool idle() { return fx.idle(60000) && engine->waitForIdle(60000); }
    DuplicateStore& store() { return engine->store(); }
    QString dir(const QString& rel) const { return photos.file(rel); }
    // Dossier de la copie gardée (choix ou suggestion) d'un groupe.
    QStringList keptFolders(GroupId id)
    {
        QStringList out;
        const auto g = store().group(id);
        for (const auto& c : g->copies)
            if (c.keep)
                out << QDir::toNativeSeparators(QFileInfo(fx.lib->item(c.item)->path()).absolutePath());
        return out;
    }
    QList<GroupId> pairGroups()
    {
        GroupQuery q;
        q.pairA = dir(QStringLiteral("Images"));
        q.pairB = dir(QStringLiteral("Sauvegarde"));
        return store().groups(q);
    }
    GroupId internalGroup()
    {
        GroupQuery q;
        q.folder = dir(QStringLiteral("Images"));
        for (GroupId id : store().groups(q))
            if (!pairGroups().contains(id))
                return id;
        return 0;
    }
};

} // namespace

class TstDuplicatesBatch : public QObject {
    Q_OBJECT

private slots:
    void suggestionDefaultsToCenterWithoutRules()
    {
        Rig rig;
        QVERIFY(rig.idle());
        const auto ids = rig.store().groups();
        QCOMPARE(ids.size(), 6);
        for (GroupId id : ids) {
            const auto g = rig.store().group(id);
            QCOMPARE(g->suggested, g->center);
            QCOMPARE(g->suggestionReason, SuggestionReason::Quality);
            QCOMPARE(g->flags, 0);
            int kept = 0;
            for (const auto& c : g->copies)
                kept += c.keep ? 1 : 0;
            QCOMPARE(kept, 1);
        }
        // Totaux en une requête = une copie à la corbeille par groupe.
        const SelectionTotals all = rig.store().trashTotals();
        QCOMPARE(all.groups, 6);
        QCOMPARE(all.files, 6);
        QCOMPARE(rig.store().totals(ids).files, 6);
        QCOMPARE(rig.store().totals(ids.mid(0, 2)).groups, 2);
    }

    void queriesCombineFilters()
    {
        Rig rig;
        QVERIFY(rig.idle());
        GroupQuery q;
        QCOMPARE(rig.store().count(q), 6);
        q.folder = rig.dir(QStringLiteral("Sauvegarde"));
        QCOMPARE(rig.store().count(q), 5);
        q.folder = rig.dir(QStringLiteral("Sauvegarde")) + QStringLiteral("\\2019\\");
        QCOMPARE(rig.store().count(q), 3); // séparateur final toléré
        q.folder = rig.dir(QStringLiteral("Sauvegard"));
        QCOMPARE(rig.store().count(q), 0); // frontière de segment
        q = {};
        QCOMPARE(rig.pairGroups().size(), 5);
        q.type = GroupQuery::Type::Videos;
        QCOMPARE(rig.store().count(q), 0);
        q.type = GroupQuery::Type::Photos;
        QCOMPARE(rig.store().count(q), 6);
        q.type = GroupQuery::Type::RawJpeg;
        QCOMPARE(rig.store().count(q), 0);
        q = {};
        q.minBytes = 1024LL * 1024 * 1024;
        QCOMPARE(rig.store().count(q), 0);
        q.minBytes = 1;
        QCOMPARE(rig.store().count(q), 6);
        q = {};
        q.dateFrom = QDateTime::currentMSecsSinceEpoch() + 86400000LL * 365;
        QCOMPARE(rig.store().count(q), 0);
        q.dateFrom = 0;
        q.dateTo = QDateTime::currentMSecsSinceEpoch() + 86400000LL;
        QCOMPARE(rig.store().count(q), 6);

        // État : un choix fait à la main rend le groupe « modifié ».
        const GroupId g = rig.store().groups().first();
        const auto grp = rig.store().group(g);
        QVERIFY(rig.store().setKeep(g, grp->copies.last().item, true));
        q = {};
        q.state = GroupQuery::State::Modified;
        QCOMPARE(rig.store().groups(q), QList<GroupId>{g});
        q.state = GroupQuery::State::Untouched;
        QCOMPARE(rig.store().count(q), 5);

        for (auto sort : {GroupQuery::Sort::Copies, GroupQuery::Sort::Date, GroupQuery::Sort::Folder}) {
            q = {};
            q.sort = sort;
            QCOMPARE(rig.store().groups(q).size(), 6);
        }
    }

    void keepAllBatchHidesGroupsAndUndoBringsThemBack()
    {
        Rig rig;
        QVERIFY(rig.idle());
        const QList<GroupId> ids = rig.store().groups();
        const QList<GroupId> three = ids.mid(0, 3);
        const BatchResult r = rig.store().applyBatch(BatchKind::KeepAll, three);
        QVERIFY(r.batchId);
        QCOMPARE(r.changed, 3);
        QVERIFY(r.message.contains(QStringLiteral("« Garder toutes »")));
        QCOMPARE(rig.store().groups().size(), 3); // sortis de la revue tout de suite
        QCOMPARE(rig.store().batches().size(), 1);
        QCOMPARE(rig.store().batches().first().groups, 3);

        QVERIFY(rig.engine->waitForIdle(60000)); // le regroupement les garde « Garder toutes »
        QCOMPARE(rig.store().groups().size(), 3);

        const UndoResult u = rig.store().undoBatch(r.batchId);
        QCOMPARE(u.notRestored, 0);
        QCOMPARE(rig.store().groups().size(), 6);
        QVERIFY(rig.store().batches().first().undone);
        // Idempotent.
        QCOMPARE(rig.store().undoBatch(r.batchId).restored, 0);
        QVERIFY(rig.engine->waitForIdle(60000));
        QCOMPARE(rig.store().groups().size(), 6);
    }

    void notDuplicateBatchRemovesGroupsUntilUndone()
    {
        Rig rig;
        QVERIFY(rig.idle());
        const QList<GroupId> two = rig.store().groups().mid(0, 2);
        const BatchResult r = rig.store().applyBatch(BatchKind::NotDuplicate, two);
        QCOMPARE(r.changed, 2);
        QCOMPARE(rig.store().groups().size(), 4);
        QVERIFY(rig.engine->waitForIdle(60000));
        QCOMPARE(rig.store().groups().size(), 4);

        rig.store().undoBatch(r.batchId);
        QVERIFY(rig.engine->waitForIdle(60000)); // les groupes reviennent au regroupement
        QCOMPARE(rig.store().groups().size(), 6);
    }

    void resetBatchAndPartialUndo()
    {
        Rig rig;
        QVERIFY(rig.idle());
        const QList<GroupId> ids = rig.store().groups();
        const GroupId a = ids[0], b = ids[1];
        const auto ga = rig.store().group(a);
        ItemId other = 0;
        for (const auto& c : ga->copies)
            if (!c.isSuggested)
                other = c.item;
        QVERIFY(rig.store().setKeep(a, other, true)); // deux copies gardées dans a
        const SelectionTotals before = rig.store().totals({a});
        QCOMPARE(before.files, 0);

        const BatchResult r = rig.store().applyBatch(BatchKind::Reset, {a, b});
        QCOMPARE(r.changed, 1); // b n'avait pas de choix
        QCOMPARE(rig.store().totals({a}).files, 1);

        // Annulation : rétablit le choix d'avant.
        UndoResult u = rig.store().undoBatch(r.batchId);
        QCOMPARE(u.notRestored, 0);
        QVERIFY(u.restored >= 2);
        QCOMPARE(rig.store().totals({a}).files, 0);

        // Un groupe modifié après le lot n'est pas rétabli, et c'est signalé.
        const BatchResult r2 = rig.store().applyBatch(BatchKind::Reset, {a});
        QVERIFY(r2.batchId);
        QVERIFY(rig.store().setKeep(a, other, true));
        u = rig.store().undoBatch(r2.batchId);
        QCOMPARE(u.notRestored, 1);

        // Un lot sans effet n'est pas inscrit.
        const int count = rig.store().batches().size();
        QCOMPARE(rig.store().applyBatch(BatchKind::Reset, {b}).batchId, 0);
        QCOMPARE(rig.store().batches().size(), count);
    }

    void keepFolderBatchOnPairLeavesOtherGroupsAlone()
    {
        Rig rig;
        QVERIFY(rig.idle());
        const QList<GroupId> pair = rig.pairGroups();
        const GroupId internal = rig.internalGroup();
        QVERIFY(internal);
        const QString images = rig.dir(QStringLiteral("Images"));
        const BatchResult r = rig.store().applyBatch(BatchKind::KeepFolder, pair, images);
        QCOMPARE(r.changed, 5);
        QCOMPARE(r.flagged, 0);
        for (GroupId id : pair) {
            const QStringList kept = rig.keptFolders(id);
            QCOMPARE(kept.size(), 1);
            QVERIFY2(kept.first().startsWith(images, Qt::CaseInsensitive), qPrintable(kept.first()));
        }
        // Le doublon interne n'était pas dans le lot : aucun choix.
        for (const auto& c : rig.store().group(internal)->copies)
            QVERIFY(!c.userChoice);

        // Garder l'autre côté, puis annuler : retour au premier lot.
        const BatchResult back = rig.store().applyBatch(BatchKind::KeepFolder, pair, rig.dir(QStringLiteral("Sauvegarde")));
        QCOMPARE(back.changed, 5);
        QVERIFY(rig.keptFolders(pair.first()).first().contains(QStringLiteral("Sauvegarde")));
        rig.store().undoBatch(back.batchId);
        QVERIFY(rig.keptFolders(pair.first()).first().startsWith(images, Qt::CaseInsensitive));

        // Dossier sans copie : rien n'est fait.
        const BatchResult none = rig.store().applyBatch(BatchKind::KeepFolder, {internal}, rig.dir(QStringLiteral("Sauvegarde")));
        QCOMPARE(none.batchId, 0);
        QCOMPARE(none.skipped, 1);
    }

    void folderRulesOrientSuggestionsButNotManualChoices()
    {
        Rig rig;
        QVERIFY(rig.idle());
        const QList<GroupId> pair = rig.pairGroups();
        // Un choix fait à la main avant la règle : la copie de la sauvegarde gardée.
        const GroupId manual = pair.first();
        ItemId backupCopy = 0;
        for (const auto& c : rig.store().group(manual)->copies)
            if (rig.fx.lib->item(c.item)->path().contains(QStringLiteral("Sauvegarde")))
                backupCopy = c.item;
        QVERIFY(backupCopy);
        rig.store().setKeep(manual, backupCopy, true);
        for (const auto& c : rig.store().group(manual)->copies)
            if (c.item != backupCopy)
                rig.store().setKeep(manual, c.item, false);

        const QString backup = rig.dir(QStringLiteral("Sauvegarde"));
        const qint64 ruleId = rig.store().setFolderRule(backup, RuleKind::Avoid);
        QVERIFY(ruleId);
        for (GroupId id : pair) {
            const auto g = rig.store().group(id);
            QCOMPARE(g->suggestionReason, SuggestionReason::FolderAvoided);
            const auto suggested = rig.fx.lib->item(g->suggested)->path();
            QVERIFY2(!suggested.contains(QStringLiteral("Sauvegarde")), qPrintable(suggested));
        }
        QVERIFY(rig.keptFolders(manual).first().contains(QStringLiteral("Sauvegarde"))); // FR-008

        QList<FolderRule> rules = rig.store().folderRules();
        QCOMPARE(rules.size(), 1);
        QVERIFY(rules.first().found);
        QCOMPARE(rules.first().groups, 5);
        QCOMPARE(rules.first().kind, RuleKind::Avoid);

        // Les suggestions survivent au regroupement.
        rig.engine->rescanAll();
        QVERIFY(rig.engine->waitForIdle(60000));
        QCOMPARE(rig.store().group(rig.pairGroups().last())->suggestionReason, SuggestionReason::FolderAvoided);

        // Règle sur un dossier absent : signalée, sans effet.
        rig.store().setFolderRule(QStringLiteral("Z:\\Nulle part"), RuleKind::Prefer);
        bool sawMissing = false;
        for (const auto& r : rig.store().folderRules())
            sawMissing |= !r.found && r.path == QStringLiteral("Z:\\Nulle part");
        QVERIFY(sawMissing);

        rig.store().removeFolderRule(ruleId);
        for (GroupId id : rig.pairGroups())
            QCOMPARE(rig.store().group(id)->suggestionReason, SuggestionReason::Quality);
    }

    void allCopiesAvoidedKeepsBestAndFlags()
    {
        Rig rig;
        QVERIFY(rig.idle());
        rig.store().setFolderRule(rig.dir(QStringLiteral("Images")), RuleKind::Avoid);
        const GroupId internal = rig.internalGroup();
        const auto g = rig.store().group(internal);
        QCOMPARE(g->flags, int(FlagAllAvoided));
        QCOMPARE(g->suggested, g->center);
        GroupQuery q;
        q.state = GroupQuery::State::Flagged;
        QCOMPARE(rig.store().groups(q), QList<GroupId>{internal});
    }

    void folderPairsRollUpAndInternalDuplicates()
    {
        Rig rig;
        QVERIFY(rig.idle());
        const QList<FolderPair> pairs = rig.store().folderPairs();
        QVERIFY(!pairs.isEmpty());
        const FolderPair& top = pairs.first();
        // Images\2019 ↔ Sauvegarde\2019 et Images\2020 ↔ Sauvegarde\2020 : remontées à leurs racines.
        const QString images = rig.dir(QStringLiteral("Images")), backup = rig.dir(QStringLiteral("Sauvegarde"));
        QVERIFY2((top.a.compare(images, Qt::CaseInsensitive) == 0 && top.b.compare(backup, Qt::CaseInsensitive) == 0) ||
                     (top.a.compare(backup, Qt::CaseInsensitive) == 0 && top.b.compare(images, Qt::CaseInsensitive) == 0),
                 qPrintable(top.a + QStringLiteral(" <-> ") + top.b));
        QCOMPARE(top.groups, 5);
        QCOMPARE(top.subPairs, 2);
        QVERIFY(top.bytes > 0);

        const QList<FolderStat> internal = rig.store().internalDuplicates();
        QCOMPARE(internal.size(), 1);
        QVERIFY(internal.first().folder.endsWith(QStringLiteral("2019")));
        QCOMPARE(internal.first().groups, 1);

        const auto folders = rig.store().foldersOf(rig.pairGroups());
        QCOMPARE(folders.size(), 4);
        QCOMPARE(folders.first().second, 3); // Images\2019 ou Sauvegarde\2019
    }

    void setAsideListsDecisionsAndRestoresThem()
    {
        Rig rig;
        QVERIFY(rig.idle());
        const QList<GroupId> ids = rig.store().groups();
        rig.store().applyBatch(BatchKind::KeepAll, {ids[0]});
        rig.store().applyBatch(BatchKind::NotDuplicate, {ids[1]});
        QVERIFY(rig.engine->waitForIdle(60000));
        QCOMPARE(rig.store().groups().size(), 4);

        // Survit à un redémarrage.
        rig.engine.reset();
        rig.fx.reopen();
        rig.engine = std::make_unique<DuplicateEngine>(*rig.fx.lib, testOptions());
        QVERIFY(rig.idle());

        const QList<SetAsideEntry> aside = rig.store().setAside();
        QCOMPARE(aside.size(), 2);
        int kept = 0, notDup = 0;
        for (const auto& e : aside) {
            QCOMPARE(e.items.size(), 2);
            QVERIFY(e.decidedAt > 0);
            kept += e.kind == SetAsideEntry::Kind::KeptAll;
            notDup += e.kind == SetAsideEntry::Kind::NotDuplicate;
        }
        QCOMPARE(kept, 1);
        QCOMPARE(notDup, 1);

        rig.store().restore(aside);
        QVERIFY(rig.engine->waitForIdle(60000));
        QCOMPARE(rig.store().groups().size(), 6);
        QVERIFY(rig.store().setAside().isEmpty());
    }

    void migrationTwoToThreeKeepsBehaviour()
    {
        TempTree data;
        const QString path = data.file(QStringLiteral("library.db"));
        {
            auto db = Database::open(path);
            std::lock_guard lock(db->mutex());
            for (const char* name : {":/sql/001_initial.sql", ":/sql/002_doublons.sql"}) {
                QFile f(QString::fromLatin1(name));
                if (!f.open(QIODevice::ReadOnly)) {
                    // Les ressources de la bibliothèque statique sont chargées par migrateLibrary().
                    auto probe = Database::open(data.file(QStringLiteral("probe.db")));
                    migrateLibrary(*probe);
                    QVERIFY(f.open(QIODevice::ReadOnly));
                }
                Transaction tx(*db);
                db->exec(f.readAll().constData());
                tx.commit();
            }
            db->exec("INSERT INTO watched_root (id, path, added_at) VALUES (1, 'C:\\T', 0);"
                     "INSERT INTO folder (id, root_id, path, parent_path, name) VALUES (1, 1, 'C:\\T', 'C:\\', 'T');"
                     "INSERT INTO item (id, folder_id, file_name, kind, format, size_bytes, mtime, sort_date, last_seen_at)"
                     " VALUES (1, 1, 'a.jpg', 'image', 'jpeg', 100, 0, 0, 0), (2, 1, 'b.jpg', 'image', 'jpeg', 100, 0, 0, 0),"
                     " (3, 1, 'c.jpg', 'image', 'jpeg', 50, 0, 0, 0);"
                     "INSERT INTO dup_group (id, center_item_id, keeper_reason, level, member_count, reclaimable_bytes, updated_at)"
                     " VALUES (7, 2, 'path', 'identical', 3, 150, 0);"
                     "INSERT INTO dup_member (group_id, item_id, level) VALUES (7, 1, 'identical'), (7, 2, 'identical'),"
                     " (7, 3, 'identical');"
                     "INSERT INTO dup_choice (item_id, keep, decided_at) VALUES (3, 1, 0);");
        }
        auto db = Database::open(path);
        QCOMPARE(migrateLibrary(*db), 3);
        std::lock_guard lock(db->mutex());
        auto q = db->prepare("SELECT item_id, suggested_keep FROM dup_member ORDER BY item_id");
        QList<int> suggested;
        while (q.step())
            suggested << q.int32(1);
        QCOMPARE(suggested, (QList<int>{0, 1, 0})); // le centre
        auto g = db->prepare("SELECT flags, suggestion_reason IS NULL, reclaimable_bytes FROM dup_group WHERE id = 7");
        QVERIFY(g.step());
        QCOMPARE(g.int32(0), 0);
        QCOMPARE(g.int32(1), 1);
        QCOMPARE(g.int64(2), 150); // inchangée
        auto o = db->prepare("SELECT COUNT(*) FROM pragma_table_info('file_operation') WHERE name = 'stopped'");
        QVERIFY(o.step());
        QCOMPARE(o.int32(0), 1);
    }
};

QTEST_MAIN(TstDuplicatesBatch)
#include "tst_duplicates_batch.moc"

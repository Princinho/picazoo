// T028 — dossiers surveillés : ajout, exclusion, retrait (index seulement), racine
// imbriquée de nouveau couverte, dossier sans droit d'accès ; invariant SC-006.

#include "library/Library.h"
#include "support/FixtureFactory.h"
#include "support/FsSnapshot.h"
#include "support/LibraryFixture.h"
#include "support/TempTree.h"

#include <QProcess>
#include <QtTest>

using namespace picazoo;
using namespace picazoo::test;

namespace {

std::optional<WatchedRoot> rootFor(Library& lib, const QString& path)
{
    for (const auto& r : lib.roots())
        if (r.path.compare(QDir::toNativeSeparators(path), Qt::CaseInsensitive) == 0)
            return r;
    return std::nullopt;
}

} // namespace

class TstRoots : public QObject {
    Q_OBJECT

private slots:
    void addExcludeIncludeRemove()
    {
        TempTree photos;
        FixtureFactory::writeJpeg(photos.file("R/A/1.jpg"));
        FixtureFactory::writeJpeg(photos.file("R/A/2.jpg"));
        FixtureFactory::writeJpeg(photos.file("R/B/3.jpg"));
        FixtureFactory::writeJpeg(photos.file("S/C/4.jpg"));
        const FsSnapshot before = FsSnapshot::take(photos.path());

        LibraryFixture fx;
        const RootId r = fx.lib->addRoot(photos.file("R"));
        const RootId s = fx.lib->addRoot(photos.file("S"));
        QVERIFY(fx.idle());
        QCOMPARE(fx.lib->itemCount(), 4);

        fx.lib->excludePath(r, photos.file("R/A"));
        QVERIFY(fx.idle());
        QCOMPARE(fx.lib->itemCount(), 2);
        QCOMPARE(rootFor(*fx.lib, photos.file("R"))->excluded.size(), 1);

        fx.lib->includePath(r, QDir::toNativeSeparators(photos.file("R/A")));
        QVERIFY(fx.idle());
        QCOMPARE(fx.lib->itemCount(), 4);

        fx.lib->removeRoot(r);
        QVERIFY(fx.idle());
        QCOMPARE(fx.lib->itemCount(), 1);
        QCOMPARE(fx.countsByFolder().value("C"), 1);
        QVERIFY(!rootFor(*fx.lib, photos.file("R")));
        Q_UNUSED(s);

        fx.close();
        PICAZOO_VERIFY_UNTOUCHED(before, photos.path());
        QVERIFY(QFile::exists(photos.file("R/A/1.jpg")));
    }

    void removingOuterRootKeepsNestedRoot()
    {
        TempTree photos;
        FixtureFactory::writeJpeg(photos.file("O/x.jpg"));
        FixtureFactory::writeJpeg(photos.file("O/I/y.jpg"));
        LibraryFixture fx;
        const RootId outer = fx.lib->addRoot(photos.file("O"));
        fx.lib->addRoot(photos.file("O/I"));
        QVERIFY(fx.idle());
        QCOMPARE(fx.lib->itemCount(), 2);
        fx.lib->removeRoot(outer);
        QVERIFY(fx.idle());
        QCOMPARE(fx.lib->itemCount(), 1);
        QCOMPARE(fx.countsByFolder().value("I"), 1);
    }

    void deniedFolderIsIgnored()
    {
        TempTree photos;
        FixtureFactory::writeJpeg(photos.file("Ok/a.jpg"));
        FixtureFactory::writeJpeg(photos.file("Interdit/b.jpg"));
        const QString denied = photos.file("Interdit");
        const QString user = qEnvironmentVariable("USERNAME");
        if (QProcess::execute("icacls", {denied, "/deny", user + ":(RD)"}) != 0)
            QSKIP("Impossible de retirer le droit de lecture (icacls).");
        auto restore = qScopeGuard([&] { QProcess::execute("icacls", {denied, "/remove:d", user}); });

        LibraryFixture fx;
        fx.lib->addRoot(photos.file("Ok"));
        fx.lib->addRoot(denied);
        QVERIFY(fx.idle());
        QCOMPARE(fx.lib->itemCount(), 1);
        const auto r = rootFor(*fx.lib, denied);
        QVERIFY(r);
        QCOMPARE(int(r->state), int(RootState::Denied));
    }
};

QTEST_MAIN(TstRoots)
#include "tst_roots.moc"

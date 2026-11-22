// T065 — surveillance en direct : ajouts, suppression, renommage visibles en moins de
// 10 s ; fichier écrit par morceaux pris en compte une fois stable.

#include "library/Library.h"
#include "support/FixtureFactory.h"
#include "support/LibraryFixture.h"
#include "support/TempTree.h"

#include <QElapsedTimer>
#include <QThread>
#include <QtTest>

using namespace picazoo;
using namespace picazoo::test;

namespace {

QStringList names(Library& lib)
{
    QStringList out;
    for (const auto& f : lib.folders(FolderSort::NameAsc))
        for (const auto& s : lib.itemSummaries(f.id, ItemSort::NameAsc))
            out << s.fileName;
    out.sort();
    return out;
}

bool waitFor(Library& lib, const std::function<bool()>& cond, int ms)
{
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        if (cond())
            return true;
        QThread::msleep(50);
    }
    Q_UNUSED(lib);
    return cond();
}

} // namespace

class TstWatcher : public QObject {
    Q_OBJECT

private slots:
    void liveChanges()
    {
        TempTree photos;
        FixtureFactory::writeJpeg(photos.file("A/existe.jpg"));
        FixtureFactory::writeJpeg(photos.file("A/a-supprimer.jpg"));
        FixtureFactory::writeJpeg(photos.file("A/a-renommer.jpg"));
        LibraryFixture fx;
        const RootId root = fx.lib->addRoot(photos.path());
        QVERIFY(fx.idle());
        QVERIFY(waitFor(*fx.lib, [&] { return fx.lib->isWatching(root); }, 5000));
        const ItemId renamedId = [&] {
            for (const auto& f : fx.lib->folders(FolderSort::NameAsc))
                for (const auto& s : fx.lib->itemSummaries(f.id, ItemSort::NameAsc))
                    if (s.fileName == u"a-renommer.jpg")
                        return s.id;
            return ItemId(0);
        }();

        QElapsedTimer t;
        t.start();
        for (int i = 0; i < 5; ++i)
            FixtureFactory::writeJpeg(photos.file(QStringLiteral("A/nouvelle%1.jpg").arg(i)));
        QVERIFY(QFile::remove(photos.file("A/a-supprimer.jpg")));
        QVERIFY(QFile::rename(photos.file("A/a-renommer.jpg"), photos.file("A/renommee.jpg")));

        const QStringList expected{"existe.jpg", "nouvelle0.jpg", "nouvelle1.jpg", "nouvelle2.jpg", "nouvelle3.jpg",
                                   "nouvelle4.jpg", "renommee.jpg"};
        QVERIFY2(waitFor(*fx.lib, [&] { return names(*fx.lib) == expected; }, 10000),
                 qPrintable(names(*fx.lib).join(", ")));
        QVERIFY2(t.elapsed() < 10000, "SC-005 : plus de 10 s");
        qInfo() << "Changements visibles en" << t.elapsed() << "ms";
        // Le renommage conserve l'identifiant.
        bool sameId = false;
        for (const auto& f : fx.lib->folders(FolderSort::NameAsc))
            for (const auto& s : fx.lib->itemSummaries(f.id, ItemSort::NameAsc))
                sameId = sameId || (s.fileName == u"renommee.jpg" && s.id == renamedId);
        QVERIFY(sameId);
    }

    void newFolderAndRemovedFolder()
    {
        TempTree photos;
        FixtureFactory::writeJpeg(photos.file("Vieux/v.jpg"));
        LibraryFixture fx;
        const RootId root = fx.lib->addRoot(photos.path());
        QVERIFY(fx.idle());
        QVERIFY(waitFor(*fx.lib, [&] { return fx.lib->isWatching(root); }, 5000));
        FixtureFactory::writeJpeg(photos.file("Nouveau/Sous/n.jpg"));
        QVERIFY(QDir(photos.file("Vieux")).removeRecursively());
        QVERIFY2(waitFor(*fx.lib, [&] { return names(*fx.lib) == QStringList{"n.jpg"}; }, 10000),
                 qPrintable(names(*fx.lib).join(", ")));
    }

    void fileWrittenInChunks()
    {
        TempTree photos;
        photos.mkdir("C");
        LibraryFixture fx;
        const RootId root = fx.lib->addRoot(photos.path());
        QVERIFY(fx.idle());
        QVERIFY(waitFor(*fx.lib, [&] { return fx.lib->isWatching(root); }, 5000));
        JpegSpec spec;
        spec.size = QSize(512, 512);
        const QByteArray data = FixtureFactory::jpeg(spec);
        QFile f(photos.file("C/lente.jpg"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        const qsizetype chunk = data.size() / 6 + 1;
        for (qsizetype off = 0; off < data.size(); off += chunk) {
            f.write(data.mid(off, chunk));
            f.flush();
            QCoreApplication::processEvents();
            QThread::msleep(500);
        }
        f.close();
        QVERIFY(waitFor(*fx.lib, [&] { return names(*fx.lib) == QStringList{"lente.jpg"}; }, 10000));
        QVERIFY(fx.idle());
        for (const auto& fo : fx.lib->folders(FolderSort::NameAsc))
            for (const auto& s : fx.lib->itemSummaries(fo.id, ItemSort::NameAsc))
                QCOMPARE(int(s.thumbState), int(ThumbState::Done)); // décodé une fois complet
    }
};

QTEST_MAIN(TstWatcher)
#include "tst_watcher.moc"

// T030 (005) — « Ouvrir dans Picazoo » : la photo est montrée si son dossier est dans la
// bibliothèque ; sinon Picazoo propose d'ajouter le dossier, sans le faire d'office.

#include "controllers/AppController.h"
#include "support/FixtureFactory.h"
#include "support/LibraryFixture.h"
#include "support/TempTree.h"

#include <QSignalSpy>
#include <QtTest>

using namespace picazoo;
using namespace picazoo::test;

class TstShowPath : public QObject {
    Q_OBJECT

private slots:
    void showsAnIndexedPhoto()
    {
        TempTree photos;
        FixtureFactory::writeJpeg(photos.file(QStringLiteral("Vacances/IMG_1.jpg")));
        FixtureFactory::writeJpeg(photos.file(QStringLiteral("Vacances/IMG_2.jpg")));
        LibraryFixture f;
        f.lib->addRoot(photos.path());
        QVERIFY(f.idle(30000, false));

        AppController app(*f.lib);
        QSignalSpy shown(&app, &AppController::showItem);
        QSignalSpy asked(&app, &AppController::askAddFolder);
        // Casse différente et séparateurs « / » : comme un chemin venu de Windows.
        app.showPath(photos.path().toUpper().replace(u'\\', u'/') + QStringLiteral("/Vacances/img_2.JPG"));
        QCOMPARE(shown.count(), 1);
        QCOMPARE(asked.count(), 0);
        const auto item = f.lib->item(shown.first().first().toLongLong());
        QVERIFY(item);
        QCOMPARE(item->fileName, QStringLiteral("IMG_2.jpg"));
    }

    void asksBeforeAddingAnOutsideFolder()
    {
        TempTree photos, outside;
        FixtureFactory::writeJpeg(photos.file(QStringLiteral("A/1.jpg")));
        const QString stray = FixtureFactory::writeJpeg(outside.file(QStringLiteral("Ailleurs/2.jpg")));
        LibraryFixture f;
        f.lib->addRoot(photos.path());
        QVERIFY(f.idle(30000, false));

        AppController app(*f.lib);
        QSignalSpy shown(&app, &AppController::showItem);
        QSignalSpy asked(&app, &AppController::askAddFolder);
        app.showPath(stray);
        QCOMPARE(shown.count(), 0);
        QCOMPARE(asked.count(), 1);
        QCOMPARE(asked.first().first().toString(), outside.file(QStringLiteral("Ailleurs")));
        QCOMPARE(f.lib->roots().size(), 1); // rien n'a été ajouté d'office
    }

    void reportsAFolderNotYetIndexed()
    {
        TempTree photos;
        FixtureFactory::writeJpeg(photos.file(QStringLiteral("A/1.jpg")));
        LibraryFixture f;
        f.lib->addRoot(photos.path());
        QVERIFY(f.idle(30000, false));
        const QString fresh = FixtureFactory::writeJpeg(photos.file(QStringLiteral("Nouveau/3.jpg")));

        AppController app(*f.lib);
        QSignalSpy message(&app, &AppController::showMessage);
        QSignalSpy shown(&app, &AppController::showItem);
        QSignalSpy asked(&app, &AppController::askAddFolder);
        app.showPath(fresh);
        // Dossier surveillé : jamais de proposition d'ajout. Selon la vitesse de la
        // surveillance en direct, la photo est déjà indexée ou un message le dit.
        QCOMPARE(asked.count(), 0);
        QCOMPARE(message.count() + shown.count(), 1);
    }
};

QTEST_MAIN(TstShowPath)
#include "tst_showpath.moc"

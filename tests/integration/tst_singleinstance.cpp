// T009/T010 (005) — instance unique : élection par mutex, transmission du chemin.

#include "platform/win/SingleInstance.h"

#include <QSignalSpy>
#include <QUuid>
#include <QtTest>

#include <atomic>
#include <thread>

using namespace picazoo::platform;

class TstSingleInstance : public QObject {
    Q_OBJECT

private slots:
    void secondInstanceForwardsItsPath()
    {
        const QString key = QStringLiteral("Picazoo.Test.") + QUuid::createUuid().toString(QUuid::Id128);
        SingleInstance primary(key);
        QVERIFY(primary.isPrimary());
        QVERIFY(primary.listen());
        QSignalSpy received(&primary, &SingleInstance::messageReceived);

        // L'instance secondaire est un autre processus en vrai : ici, un autre thread, pendant
        // que celui-ci fait tourner la boucle d'événements de l'instance principale.
        const QString path = QStringLiteral("C:\\Photos\\Été 2026\\IMG_0042.jpg");
        std::atomic<int> secondaryWasPrimary{-1};
        std::atomic<int> delivered{-1};
        std::thread other([&] {
            SingleInstance secondary(key);
            secondaryWasPrimary = secondary.isPrimary() ? 1 : 0;
            delivered = secondary.sendToPrimary(path, 3000) ? 1 : 0;
        });
        QTRY_COMPARE_WITH_TIMEOUT(received.count(), 1, 4000);
        other.join();
        QCOMPARE(secondaryWasPrimary.load(), 0);
        QCOMPARE(delivered.load(), 1);
        QCOMPARE(received.first().first().toString(), path);
    }

    void noPrimaryMeansNoDelivery()
    {
        const QString key = QStringLiteral("Picazoo.Test.") + QUuid::createUuid().toString(QUuid::Id128);
        SingleInstance alone(key);
        QVERIFY(alone.isPrimary());
        SingleInstance other(key);
        QVERIFY(!other.sendToPrimary(QStringLiteral("C:\\x.jpg"), 300)); // personne n'écoute
    }
};

QTEST_GUILESS_MAIN(TstSingleInstance)
#include "tst_singleinstance.moc"

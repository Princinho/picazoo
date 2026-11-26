// 008 — budget mémoire du scan : fonction pure, RAM injectée (contracts/core-api.md).

#include "jobs/MemoryBudget.h"

#include <QtTest>

using namespace picazoo;

namespace {
constexpr qint64 kGiB = 1024LL * 1024 * 1024;
constexpr qint64 kMiB = 1024LL * 1024;
}

class TstMemoryBudget : public QObject {
    Q_OBJECT

private slots:
    // (1) Jamais 0 thread, même sans RAM disponible.
    void neverZeroThreads_data()
    {
        QTest::addColumn<qint64>("total");
        QTest::addColumn<qint64>("available");
        QTest::addColumn<int>("cpuThreads");
        QTest::newRow("aucune RAM dispo") << 8 * kGiB << qint64(0) << 8;
        QTest::newRow("RAM dispo négative (défensif)") << 8 * kGiB << qint64(-1) << 8;
        QTest::newRow("très peu de RAM dispo") << 4 * kGiB << 64 * kMiB << 4;
        QTest::newRow("1 seul cœur recommandé") << 8 * kGiB << qint64(0) << 1;
    }

    void neverZeroThreads()
    {
        QFETCH(qint64, total);
        QFETCH(qint64, available);
        QFETCH(int, cpuThreads);
        const MemoryBudget b = computeMemoryBudget({total, available, cpuThreads});
        QVERIFY(b.thumbnailThreads >= 1);
    }

    // (2) Jamais plus de threads que ce que recommande le CPU.
    void neverExceedsCpuRecommendation_data()
    {
        QTest::addColumn<qint64>("total");
        QTest::addColumn<qint64>("available");
        QTest::addColumn<int>("cpuThreads");
        QTest::newRow("RAM généreuse, peu de cœurs") << 64 * kGiB << 48 * kGiB << 2;
        QTest::newRow("RAM généreuse, beaucoup de cœurs") << 64 * kGiB << 48 * kGiB << 16;
        QTest::newRow("machine de référence") << 8 * kGiB << 4 * kGiB << 8;
        QTest::newRow("RAM rare, beaucoup de cœurs") << 8 * kGiB << 512 * kMiB << 16;
    }

    void neverExceedsCpuRecommendation()
    {
        QFETCH(qint64, total);
        QFETCH(qint64, available);
        QFETCH(int, cpuThreads);
        const MemoryBudget b = computeMemoryBudget({total, available, cpuThreads});
        QVERIFY(b.thumbnailThreads <= cpuThreads);
    }

    // (3) Machine généreuse (32 Go total, 24 Go dispo) : pas de bridage par rapport
    // au comportement actuel (32 Mo de cache SQLite, 256 Mo de cache LRU).
    void generousMachineIsNotThrottled()
    {
        const MemoryBudget b = computeMemoryBudget({32 * kGiB, 24 * kGiB, 8});
        QCOMPARE(b.thumbnailThreads, 8);
        QVERIFY(b.sqliteCacheBytesPerConnection >= 32 * kMiB);
        QVERIFY(b.thumbnailLruCacheBytes >= 256 * kMiB);
    }

    // (4) Machine de référence (8 Go total, 3-5 Go dispo) : le budget combiné reste
    // sous une marge compatible avec le pic visé de 1,2-1,5 Go (SC-001/SC-002).
    // Hypothèse de coût pic par décodage documentée à côté de computeMemoryBudget.
    void referenceMachineStaysUnderBudget_data()
    {
        QTest::addColumn<qint64>("available");
        QTest::newRow("3 Go dispo") << 3 * kGiB;
        QTest::newRow("4 Go dispo") << 4 * kGiB;
        QTest::newRow("5 Go dispo") << 5 * kGiB;
    }

    void referenceMachineStaysUnderBudget()
    {
        QFETCH(qint64, available);
        const MemoryBudget b = computeMemoryBudget({8 * kGiB, available, 8});
        // Coût pic supposé par décodage HEIC/RAW/JPEG en vol (après H1, la miniature
        // HEIC ramène le cas courant à quelques dizaines de Mo ; on retient une
        // hypothèse prudente de 40 Mo par thread pour couvrir les formats sans
        // miniature intégrée et les pics JPEG/RAW).
        constexpr qint64 kAssumedPeakPerThread = 40 * kMiB;
        const qint64 total = b.thumbnailThreads * kAssumedPeakPerThread +
                             5 * b.sqliteCacheBytesPerConnection + b.thumbnailLruCacheBytes;
        QVERIFY2(total <= qint64(1.3 * double(kGiB)),
                 qPrintable(QStringLiteral("budget total %1 Mo dépasse la marge visée").arg(total / double(kMiB))));
        QVERIFY(b.thumbnailThreads >= 1);
    }

    // (5) Monotonie : à RAM totale égale, moins de RAM dispo ne donne jamais un
    // budget plus généreux (threads, caches).
    void isMonotonic_data()
    {
        QTest::addColumn<qint64>("total");
        QTest::addColumn<qint64>("availableLow");
        QTest::addColumn<qint64>("availableHigh");
        QTest::addColumn<int>("cpuThreads");
        QTest::newRow("machine de référence") << 8 * kGiB << 1 * kGiB << 6 * kGiB << 8;
        QTest::newRow("grosse machine") << 32 * kGiB << 2 * kGiB << 28 * kGiB << 16;
        QTest::newRow("petite machine") << 4 * kGiB << 256 * kMiB << 3 * kGiB << 4;
    }

    void isMonotonic()
    {
        QFETCH(qint64, total);
        QFETCH(qint64, availableLow);
        QFETCH(qint64, availableHigh);
        QFETCH(int, cpuThreads);
        const MemoryBudget low = computeMemoryBudget({total, availableLow, cpuThreads});
        const MemoryBudget high = computeMemoryBudget({total, availableHigh, cpuThreads});
        QVERIFY(low.thumbnailThreads <= high.thumbnailThreads);
        QVERIFY(low.sqliteCacheBytesPerConnection <= high.sqliteCacheBytesPerConnection);
        QVERIFY(low.thumbnailLruCacheBytes <= high.thumbnailLruCacheBytes);
    }
};

QTEST_MAIN(TstMemoryBudget)
#include "tst_memorybudget.moc"

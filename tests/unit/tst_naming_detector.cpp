// 003 — détecteur de racine de nommage : fonction pure, aucune bibliothèque, aucune E/S
// (specs/003-images-liees/research.md N1/N2/N4).

#include "naming/NamingDetector.h"

#include <QtTest>

using namespace picazoo;

class TstNamingDetector : public QObject {
    Q_OBJECT

private slots:
    // --- detectRoot() : les 4 formes de numérotation reconnues (N1)

    void detectsSeparatorDigits()
    {
        const auto r = detectRoot(QStringLiteral("vacances_bali_001"));
        QVERIFY(r.has_value());
        QCOMPARE(r->shape, RootShape::SeparatorDigits);
        QCOMPARE(r->normalizedKey, QStringLiteral("vacances_bali"));
        QVERIFY(!r->generic);
    }

    void detectsParenthesized()
    {
        const auto r1 = detectRoot(QStringLiteral("photo (12)"));
        QVERIFY(r1.has_value());
        QCOMPARE(r1->shape, RootShape::Parenthesized);
        QCOMPARE(r1->normalizedKey, QStringLiteral("photo"));

        const auto r2 = detectRoot(QStringLiteral("Anniversaire Julie(2)"));
        QVERIFY(r2.has_value());
        QCOMPARE(r2->shape, RootShape::Parenthesized);
        QCOMPARE(r2->normalizedKey, QStringLiteral("anniversaire julie"));
    }

    void detectsCopySuffix()
    {
        const auto r1 = detectRoot(QStringLiteral("photo - copie"));
        QVERIFY(r1.has_value());
        QCOMPARE(r1->shape, RootShape::CopySuffix);
        QCOMPARE(r1->normalizedKey, QStringLiteral("photo"));

        const auto r2 = detectRoot(QStringLiteral("photo - copie (2)"));
        QVERIFY(r2.has_value());
        QCOMPARE(r2->shape, RootShape::CopySuffix);
        QCOMPARE(r2->normalizedKey, QStringLiteral("photo"));
    }

    void detectsGluedDigitsWithAtLeastThreeDigits()
    {
        const auto r = detectRoot(QStringLiteral("PXL20240714"));
        QVERIFY(r.has_value());
        QCOMPARE(r->shape, RootShape::GluedDigits);
        QCOMPARE(r->normalizedKey, QStringLiteral("pxl"));
    }

    void rejectsGluedDigitsUnderThreeDigits()
    {
        // "img1" : un seul chiffre collé, racine non distincte -> rejeté (N1 forme 4).
        const auto r = detectRoot(QStringLiteral("img1"));
        QVERIFY(!r.has_value());
    }

    void rejectsNameWithoutNumbering()
    {
        QVERIFY(!detectRoot(QStringLiteral("vacances_bali")).has_value());
        QVERIFY(!detectRoot(QStringLiteral("photo")).has_value());
    }

    // --- Casse ignorée pour la racine normalisée

    void normalizesCaseInsensitively()
    {
        const auto r1 = detectRoot(QStringLiteral("Vacances_Bali_001"));
        const auto r2 = detectRoot(QStringLiteral("vacances_bali_002"));
        QVERIFY(r1.has_value());
        QVERIFY(r2.has_value());
        QCOMPARE(r1->normalizedKey, r2->normalizedKey);
    }

    // --- Racines génériques (N2)

    void flagsKnownDeviceRootsAsGeneric_data()
    {
        QTest::addColumn<QString>("fileName");
        QTest::newRow("IMG_") << QStringLiteral("IMG_0001");
        QTest::newRow("DSC_") << QStringLiteral("DSC_0002");
        QTest::newRow("DSCN") << QStringLiteral("DSCN0003");
        QTest::newRow("PXL_") << QStringLiteral("PXL_20240714_153000");
        QTest::newRow("VID_") << QStringLiteral("VID_0004");
        QTest::newRow("MVI_") << QStringLiteral("MVI_0005");
        QTest::newRow("GOPR") << QStringLiteral("GOPR0006");
        QTest::newRow("Capture") << QStringLiteral("Capture_002");
        QTest::newRow("Screenshot") << QStringLiteral("Screenshot_003");
        QTest::newRow("Image") << QStringLiteral("Image_004");
        QTest::newRow("Photo") << QStringLiteral("Photo_005");
        QTest::newRow("Video") << QStringLiteral("Video_006");
    }

    void flagsKnownDeviceRootsAsGeneric()
    {
        QFETCH(QString, fileName);
        const auto r = detectRoot(fileName);
        QVERIFY2(r.has_value(), qPrintable(fileName));
        QVERIFY2(r->generic, qPrintable(fileName));
    }

    void flagsShortRootsAsGeneric_data()
    {
        QTest::addColumn<QString>("fileName");
        QTest::newRow("a_1") << QStringLiteral("a_1");
        QTest::newRow("01 seul") << QStringLiteral("01_02");
        QTest::newRow("img minuscule court") << QStringLiteral("im_1");
    }

    void flagsShortRootsAsGeneric()
    {
        QFETCH(QString, fileName);
        const auto r = detectRoot(fileName);
        if (r.has_value())
            QVERIFY2(r->generic, qPrintable(fileName));
    }

    void doesNotFlagSpecificRootsAsGeneric()
    {
        const auto r = detectRoot(QStringLiteral("vacances_bali_001"));
        QVERIFY(r.has_value());
        QVERIFY(!r->generic);
    }

    // --- groupByRoot() : regroupement, exclusion des génériques, confiance

    void groupsRegularSeriesTogether()
    {
        QList<NamingInput> items;
        for (int i = 1; i <= 5; ++i)
            items << NamingInput{ItemId(i), QStringLiteral("vacances_bali_%1").arg(i, 3, 10, QLatin1Char('0'))};
        const auto candidates = groupByRoot(items, false);
        QCOMPARE(candidates.size(), 1);
        QCOMPARE(candidates.first().items.size(), 5);
        QCOMPARE(candidates.first().confidence, Confidence::RegularSeries);
    }

    void excludesGenericRootsByDefault()
    {
        QList<NamingInput> items;
        for (int i = 1; i <= 5; ++i)
            items << NamingInput{ItemId(i), QStringLiteral("IMG_%1").arg(i, 4, 10, QLatin1Char('0'))};
        const auto candidates = groupByRoot(items, false);
        QVERIFY(candidates.isEmpty());
    }

    void includesGenericRootsWhenRequested()
    {
        QList<NamingInput> items;
        for (int i = 1; i <= 5; ++i)
            items << NamingInput{ItemId(i), QStringLiteral("IMG_%1").arg(i, 4, 10, QLatin1Char('0'))};
        const auto candidates = groupByRoot(items, true);
        QCOMPARE(candidates.size(), 1);
        QCOMPARE(candidates.first().items.size(), 5);
        QCOMPARE(candidates.first().confidence, Confidence::GenericRoot);
    }

    void neverProducesSingleItemGroup()
    {
        QList<NamingInput> items{NamingInput{1, QStringLiteral("vacances_bali_001")}};
        const auto candidates = groupByRoot(items, false);
        QVERIFY(candidates.isEmpty());
    }

    void irregularNumberingGetsSameRootConfidence()
    {
        // Mélange de formes (parenthèses + suffixe copie) sous une même racine spécifique :
        // regroupé, mais confiance plus prudente que "régulière".
        QList<NamingInput> items{
            NamingInput{1, QStringLiteral("anniversaire julie (1)")},
            NamingInput{2, QStringLiteral("anniversaire julie - copie")},
        };
        const auto candidates = groupByRoot(items, false);
        QCOMPARE(candidates.size(), 1);
        QCOMPARE(candidates.first().confidence, Confidence::SameRoot);
    }

    void gapsInNumberingGetSameRootConfidence()
    {
        QList<NamingInput> items{NamingInput{1, QStringLiteral("mariage_001")},
                                 NamingInput{2, QStringLiteral("mariage_050")},
                                 NamingInput{3, QStringLiteral("mariage_400")}};
        const auto candidates = groupByRoot(items, false);
        QCOMPARE(candidates.size(), 1);
        QCOMPARE(candidates.first().confidence, Confidence::SameRoot);
    }

    void trailingSeparatorsAreNotPartOfTheRoot()
    {
        QCOMPARE(detectRoot(QStringLiteral("vacances bali - 001"))->normalizedKey, QStringLiteral("vacances bali"));
        QCOMPARE(detectRoot(QStringLiteral("vacances bali__002"))->normalizedKey, QStringLiteral("vacances bali"));
    }

    void flagsDeviceVariantsAsGeneric_data()
    {
        QTest::addColumn<QString>("fileName");
        QTest::newRow("iPhone retouché") << QStringLiteral("IMG_E1234");
        QTest::newRow("WhatsApp") << QStringLiteral("IMG-20240101-WA0001");
        QTest::newRow("Capture Windows") << QStringLiteral("Capture d'écran 2024-03-12 153000");
        QTest::newRow("Fuji") << QStringLiteral("DSCF1234");
        QTest::newRow("Horodatage PXL") << QStringLiteral("PXL_20240714_153000123");
    }

    void flagsDeviceVariantsAsGeneric()
    {
        QFETCH(QString, fileName);
        const auto r = detectRoot(fileName);
        QVERIFY2(r.has_value(), qPrintable(fileName));
        QVERIFY2(r->generic, qPrintable(fileName));
    }

    void suggestsReadableNames()
    {
        QCOMPARE(suggestedName(QStringLiteral("vacances_bali")), QStringLiteral("vacances bali"));
        QCOMPARE(baseNameOf(QStringLiteral("a.b_001.jpg")), QStringLiteral("a.b_001"));
    }

    // SC-001 : au moins 95 % des séries nommées par événement, aucune série d'appareil.
    void accuracyOnMixedNames()
    {
        const QStringList series{
            "vacances_bali_%1", "Anniversaire Julie (%1)", "Mariage-%1",         "noel 2023 %1",
            "rando%1",          "Concert_%1",              "Lomé marché %1",     "chantier_maison_%1",
            "EVG Paul (%1)",    "bapteme%1",               "ski-alpes-%1",       "Kara festival_%1",
            "zoo %1",           "demenagement_%1",         "gala (%1)",          "remise diplome %1",
            "pique-nique_%1",   "plage%1",                 "foot_%1",            "jardin - %1"};
        const QStringList devices{"IMG_%1",   "DSC_%1",   "DSCN%1",        "PXL_20240714_%1",
                                  "VID_%1",   "MVI_%1",   "GOPR%1",        "Screenshot_%1",
                                  "_DSC%1",   "P100%1",   "DCIM_%1",       "IMG-20240101-WA%1"};
        QList<NamingInput> items;
        ItemId id = 0;
        for (const QString& pattern : series + devices)
            for (int i = 101; i <= 105; ++i)
                items << NamingInput{++id, pattern.arg(i)};
        const auto candidates = groupByRoot(items, false);
        int found = 0;
        for (const auto& c : candidates) {
            for (ItemId item : c.items)
                QVERIFY2(item <= ItemId(series.size() * 5), qPrintable(c.rootKey));
            found += c.items.size() == 5;
        }
        QVERIFY2(found * 100 >= series.size() * 95, qPrintable(QString::number(found)));
    }

    void groupsPhotoAndVideoOfSameRootTogether()
    {
        QList<NamingInput> items{
            NamingInput{1, QStringLiteral("vacances_bali_001")},
            NamingInput{2, QStringLiteral("vacances_bali_002")},
        };
        // Les extensions ne font pas partie de `fileName` (contrat : sans extension) ; une
        // photo et une vidéo de même racine sont donc déjà regroupées par cette seule fonction.
        const auto candidates = groupByRoot(items, false);
        QCOMPARE(candidates.size(), 1);
        QCOMPARE(candidates.first().items.size(), 2);
    }
};

QTEST_MAIN(TstNamingDetector)
#include "tst_naming_detector.moc"

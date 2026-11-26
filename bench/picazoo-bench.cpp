// Banc d'essai Picazoo (T050, T061, T075) — mesure SC-001, SC-002, SC-004, SC-005.
//
//   picazoo-bench --items 20000            analyse complète (SC-001)
//   picazoo-bench --items 100000 --warm    ouverture d'une bibliothèque déjà analysée (SC-002)
//   picazoo-bench --viewer                 ouverture d'une photo de 24 Mpx et suivante (SC-004)
//   picazoo-bench --live                   apparition d'une photo copiée (SC-005)
//   picazoo-bench --videos <dossier>       coût d'une vignette vidéo (T061)
//   picazoo-bench --items 20000 --duplicates   analyse des doublons après l'analyse (SC-003, 002)
//
// Les photos sont générées dans un dossier temporaire supprimé à la fin. Pour ne pas
// écrire des dizaines de Go, 50 photos « maîtres » de 24 Mpx sont multipliées par
// liens physiques : le décodage est réel, mais les lectures disque profitent du cache
// du système (mesure optimiste pour un disque dur, réaliste pour un SSD).
// Le banc d'essai ne lit jamais tes vrais dossiers, sauf `--videos <dossier>` en lecture seule.

#include "duplicates/DuplicateEngine.h"
#include "duplicates/DuplicateStore.h"
#include "index/LibraryDb.h"
#include "platform/win/ThrottledReader.h"
#include "jobs/ThumbnailPipeline.h"
#include "library/Library.h"
#include "media/Decoder.h"
#include "media/JpegDecoder.h"
#include "platform/win/FileIO.h"
#include "platform/win/CpuInfo.h"
#include "platform/win/FileIdentity.h"
#include "platform/win/MemoryInfo.h"

#include <QFileInfo>

#include <windows.h>

#include <QCommandLineParser>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QRandomGenerator>
#include <QTemporaryDir>
#include <QTextStream>

#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>

using namespace picazoo;

namespace {

QTextStream out(stdout);

struct Result {
    QString name;
    double value;
    QString unit;
    double threshold;
    bool lowerIsBetter = true;
};
QList<Result> results;

void report(const QString& name, double value, const QString& unit, double threshold)
{
    results << Result{name, value, unit, threshold};
    out << QStringLiteral("  %1 : %2 %3 (seuil %4 %3) %5\n")
               .arg(name)
               .arg(value, 0, 'f', 2)
               .arg(unit)
               .arg(threshold)
               .arg(value <= threshold ? QStringLiteral("OK") : QStringLiteral("ÉCHEC"));
    out.flush();
}

QImage makeMaster(int seed, QSize size)
{
    // Dégradé + bruit : une taille de fichier proche d'une vraie photo.
    QImage img(size, QImage::Format_RGB32);
    QRandomGenerator rng(quint32(seed) * 7919u + 17u);
    for (int y = 0; y < size.height(); ++y) {
        auto* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < size.width(); ++x) {
            const int n = int(rng.bounded(48));
            line[x] = qRgb((x * 255 / size.width() + seed * 13 + n) & 255, (y * 255 / size.height() + n) & 255,
                           (seed * 37 + (x ^ y) / 16 + n) & 255);
        }
    }
    return img;
}

QStringList generateLibrary(const QString& root, int items, int perFolder, QSize size)
{
    // NTFS limite un fichier à 1 023 liens physiques : au moins un maître par tranche de 1 000.
    const int masters = std::min(items, std::max(50, (items + 999) / 1000));
    QStringList masterPaths;
    QDir().mkpath(root + "/_masters");
    out << QStringLiteral("Génération de %1 photos maîtres %2×%3…\n").arg(masters).arg(size.width()).arg(size.height());
    out.flush();
    for (int i = 0; i < masters; ++i) {
        const QString p = QStringLiteral("%1/_masters/m%2.jpg").arg(root).arg(i);
        QFile f(p);
        if (!f.open(QIODevice::WriteOnly))
            qFatal("Écriture impossible : %s", qPrintable(f.fileName()));
        f.write(JpegDecoder::encode(makeMaster(i, size), 88));
        masterPaths << QDir::toNativeSeparators(p);
    }
    out << QStringLiteral("Création de %1 éléments (liens physiques)…\n").arg(items);
    out.flush();
    for (int i = 0; i < items; ++i) {
        const QString dir = QStringLiteral("%1/lib/Dossier %2").arg(root).arg(i / perFolder, 5, 10, QLatin1Char('0'));
        if (i % perFolder == 0)
            QDir().mkpath(dir);
        const QString p = QDir::toNativeSeparators(QStringLiteral("%1/IMG_%2.jpg").arg(dir).arg(i, 6, 10, QLatin1Char('0')));
        CreateHardLinkW(reinterpret_cast<const wchar_t*>(p.utf16()),
                        reinterpret_cast<const wchar_t*>(masterPaths[i % masters].utf16()), nullptr);
    }
    return masterPaths;
}

void benchScan(const QString& work, int items, bool warm, bool indexOnly, QSize size)
{
    const QString photos = work + "/photos";
    const QString data = work + "/data";
    // Pour --index-only, le contenu des images n'est pas décodé : des maîtres plus petits suffisent.
    generateLibrary(photos, items, 100, indexOnly ? QSize(1600, 1200) : size);

    out << QStringLiteral("\nSC-001 — analyse de %1 éléments (%2×%3, %4 threads de calcul)\n")
               .arg(items)
               .arg(size.width())
               .arg(size.height())
               .arg(platform::recommendedWorkerThreads());
    {
        auto lib = Library::open(QDir::toNativeSeparators(data));
        QElapsedTimer t;
        t.start();
        double firstFolder = -1;
        QObject::connect(lib.get(), &Library::foldersChanged, lib.get(), [&] {
            if (firstFolder < 0)
                firstFolder = t.elapsed() / 1000.0;
        });
        lib->addRoot(QDir::toNativeSeparators(photos + "/lib"));
        while (firstFolder < 0 && t.elapsed() < 60000)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        report("Premiers dossiers visibles", firstFolder, "s", 5);
        lib->waitForIdle(24 * 3600 * 1000, false);
        report("Découverte complète", t.elapsed() / 1000.0, "s", 60);
        if (indexOnly) {
            out << "  (vignettes non attendues : --index-only)\n";
            lib->pauseScan();
        } else {
            lib->waitForIdle(24 * 3600 * 1000, true);
            const double total = t.elapsed() / 1000.0;
            report("Analyse complète avec vignettes (ramenée à 20 000)", total * 20000.0 / items / 60.0, "min", 15);
            out << QStringLiteral("  (%1 éléments en %2 s, %3 vignettes/s)\n")
                       .arg(lib->itemCount())
                       .arg(total, 0, 'f', 1)
                       .arg(items / std::max(0.001, total), 0, 'f', 1);
            // 008 — pic de mémoire du processus pendant le scan (FR-008, SC-001/SC-002) :
            // GetProcessMemoryInfo rapporte déjà le maximum atteint depuis le lancement.
            const qint64 peakBytes = platform::processPeakWorkingSetBytes();
            report("Pic de mémoire pendant le scan", peakBytes / (1024.0 * 1024.0), "Mo", 1500);
        }
    }
    if (warm) {
        out << QStringLiteral("\nSC-002 — ouverture d'une bibliothèque déjà analysée\n");
        QElapsedTimer t;
        t.start();
        auto lib = Library::open(QDir::toNativeSeparators(data));
        const auto folders = lib->folders(FolderSort::DateDesc);
        const auto items2 = lib->allItemSummaries(ItemSort::TakenAsc);
        const double s = t.elapsed() / 1000.0;
        report(QStringLiteral("Ouverture (%1 dossiers, %2 éléments)").arg(folders.size()).arg(lib->itemCount()), s, "s", 3);
        Q_UNUSED(items2);
    }
}

// 002 — analyse complète des doublons sur la bibliothèque déjà analysée par benchScan.
// La bibliothèque générée est faite de liens physiques vers 100 photos : chaque élément est
// candidat aux doublons exacts et lu en entier (pire cas pour le disque), puis fusionné
// comme lien physique ; la partie visuelle compare toutes les vignettes.
void benchDuplicates(const QString& work, int items)
{
    out << QStringLiteral("\nSC-003 — analyse des doublons de %1 éléments déjà indexés\n").arg(items);
    auto lib = Library::open(QDir::toNativeSeparators(work + "/data"));
    lib->waitForIdle(24 * 3600 * 1000, true);
    const qint64 bytesBefore = platform::readCounters().bytes.load();
    QElapsedTimer t;
    t.start();
    DuplicateEngine::Options opt;
    opt.respectBattery = false;
    opt.autoStart = false;
    DuplicateEngine engine(*lib, opt);
    engine.waitForIdle(24 * 3600 * 1000);
    const double total = t.elapsed() / 1000.0;
    report("Analyse des doublons (ramenée à 20 000)", total * 20000.0 / std::max(1, items) / 60.0, "min", 30);
    out << QStringLiteral("  (%1 s ; %2 Mo lus ; %3 groupes)\n")
               .arg(total, 0, 'f', 1)
               .arg((platform::readCounters().bytes.load() - bytesBefore) / (1024.0 * 1024.0), 0, 'f', 0)
               .arg(engine.store().summary().groups);
}

void benchViewer(const QString& work)
{
    out << QStringLiteral("\nSC-004 — visionneuse, photo de 24 Mpx sur un écran 1920×1080\n");
    QStringList paths;
    QDir().mkpath(work + "/viewer");
    for (int i = 0; i < 6; ++i) {
        const QString p = QDir::toNativeSeparators(QStringLiteral("%1/viewer/v%2.jpg").arg(work).arg(i));
        QFile f(p);
        if (!f.open(QIODevice::WriteOnly))
            qFatal("Écriture impossible : %s", qPrintable(f.fileName()));
        f.write(JpegDecoder::encode(makeMaster(100 + i, QSize(6000, 4000)), 90));
        paths << p;
    }
    // Ouverture : vignette puis aperçu à la taille de l'écran.
    QElapsedTimer t;
    double worstOpen = 0, worstNext = 0;
    for (const QString& p : paths) {
        t.start();
        auto r = Decoder::decode({p, Format::Jpeg, QSize(1920, 1080), 1});
        worstOpen = std::max(worstOpen, t.elapsed() / 1000.0);
        Q_UNUSED(r);
    }
    report("Ouverture d'une photo (pire cas)", worstOpen, "s", 1.0);
    // Suivante : déjà préchargée ; on mesure le pire temps de préchargement en tâche de fond,
    // qui borne le délai perçu quand on enchaîne plus vite que le préchargement.
    for (const QString& p : paths) {
        t.start();
        auto r = Decoder::decode({p, Format::Jpeg, QSize(1920, 1080), 1});
        worstNext = std::max(worstNext, t.elapsed() / 1000.0);
        Q_UNUSED(r);
    }
    report("Passage à la suivante (sans préchargement)", worstNext, "s", 0.5);
}

void benchThumbCost(const QString& work)
{
    out << QStringLiteral("\nCoût d'une vignette, étape par étape (photo de 24 Mpx, un seul thread)\n");
    QDir().mkpath(work + "/cost");
    const QString p = QDir::toNativeSeparators(work + "/cost/m.jpg");
    {
        QFile f(p);
        if (!f.open(QIODevice::WriteOnly))
            qFatal("Écriture impossible : %s", qPrintable(f.fileName()));
        f.write(JpegDecoder::encode(makeMaster(3, QSize(6000, 4000)), 88));
    }
    out << QStringLiteral("  taille du fichier : %1 Mo\n").arg(QFileInfo(p).size() / 1048576.0, 0, 'f', 1);
    constexpr int n = 10;
    QElapsedTimer t;
    double meta = 0, hash = 0, read = 0, decode = 0, encode = 0;
    for (int i = 0; i < n; ++i) {
        t.start();
        Decoder::readMetadata(p, Format::Jpeg);
        meta += t.nsecsElapsed() / 1e6;
        t.start();
        platform::quickHash(p, QFileInfo(p).size());
        hash += t.nsecsElapsed() / 1e6;
        t.start();
        bool ok = false;
        const QByteArray data = platform::readWholeFile(p, &ok);
        read += t.nsecsElapsed() / 1e6;
        t.start();
        auto img = JpegDecoder::decode(data, QSize(320, 320));
        decode += t.nsecsElapsed() / 1e6;
        t.start();
        JpegDecoder::encode(fitInto(*img, QSize(320, 320)));
        encode += t.nsecsElapsed() / 1e6;
    }
    out << QStringLiteral("  métadonnées %1 ms · empreinte %2 ms · lecture %3 ms · décodage 1/8 %4 ms · encodage %5 ms\n")
               .arg(meta / n, 0, 'f', 1)
               .arg(hash / n, 0, 'f', 1)
               .arg(read / n, 0, 'f', 1)
               .arg(decode / n, 0, 'f', 1)
               .arg(encode / n, 0, 'f', 1);

    // Mise à l'échelle : même travail sur k threads, débit total.
    for (int threads : {4, 6, 8, 10, 12}) {
        std::atomic<int> done{0};
        QElapsedTimer wall;
        wall.start();
        std::vector<std::thread> pool;
        for (int k = 0; k < threads; ++k)
            pool.emplace_back([&] {
                for (int i = 0; i < 6; ++i) {
                    bool ok = false;
                    const QByteArray data = platform::readWholeFile(p, &ok);
                    auto img = JpegDecoder::decode(data, QSize(320, 320));
                    JpegDecoder::encode(fitInto(*img, QSize(320, 320)));
                    ++done;
                }
            });
        for (auto& th : pool)
            th.join();
        out << QStringLiteral("  %1 thread(s) : %2 vignettes/s\n").arg(threads).arg(done / (wall.elapsed() / 1000.0), 0, 'f', 1);
        out.flush();
    }
}

void benchLive(const QString& work)
{
    out << QStringLiteral("\nSC-005 — photo copiée dans un dossier surveillé\n");
    const QString photos = work + "/live";
    QDir().mkpath(photos + "/A");
    QFile f0(photos + "/A/existante.jpg");
    if (!f0.open(QIODevice::WriteOnly))
        qFatal("Écriture impossible : %s", qPrintable(f0.fileName()));
    f0.write(JpegDecoder::encode(makeMaster(7, QSize(800, 600))));
    f0.close();
    auto lib = Library::open(QDir::toNativeSeparators(work + "/live-data"));
    lib->addRoot(QDir::toNativeSeparators(photos));
    lib->waitForIdle(60000);
    QElapsedTimer t;
    bool seen = false;
    QObject::connect(lib.get(), &Library::itemsChanged, lib.get(), [&] {
        if (lib->itemCount() == 2)
            seen = true;
    });
    t.start();
    QFile f(photos + "/A/nouvelle.jpg");
    if (!f.open(QIODevice::WriteOnly))
        qFatal("Écriture impossible : %s", qPrintable(f.fileName()));
    f.write(JpegDecoder::encode(makeMaster(8, QSize(800, 600))));
    f.close();
    while (!seen && t.elapsed() < 30000)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    report("Apparition dans la bibliothèque", seen ? t.elapsed() / 1000.0 : 999.0, "s", 10);
}

void benchVideos(const QString& work, const QString& dir)
{
    out << QStringLiteral("\nT061 — vignettes vidéo (%1)\n").arg(dir);
    auto lib = Library::open(QDir::toNativeSeparators(work + "/video-data"));
    if (!lib->pipeline().handlesVideos()) {
        out << "  lecteur vidéo non branché sur cette version\n";
        return;
    }
    QElapsedTimer t;
    t.start();
    lib->addRoot(QDir::toNativeSeparators(dir));
    lib->waitForIdle(30 * 60 * 1000);
    int videos = 0;
    for (const auto& f : lib->folders(FolderSort::NameAsc))
        for (const auto& s : lib->itemSummaries(f.id, ItemSort::NameAsc))
            videos += s.kind == Kind::Video ? 1 : 0;
    const double per = videos ? t.elapsed() / double(videos) : 0;
    report(QStringLiteral("Coût moyen par vidéo (%1 vidéos, photos comprises)").arg(videos), per, "ms", 500);
}

} // namespace

int main(int argc, char* argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    QCommandLineParser p;
    p.setApplicationDescription("Banc d'essai Picazoo");
    p.addHelpOption();
    QCommandLineOption items("items", "Nombre d'éléments à générer.", "n", "0");
    QCommandLineOption warm("warm", "Mesure aussi l'ouverture d'une bibliothèque déjà analysée (SC-002).");
    QCommandLineOption viewer("viewer", "Mesure la visionneuse (SC-004).");
    QCommandLineOption live("live", "Mesure la mise à jour en direct (SC-005).");
    QCommandLineOption videos("videos", "Mesure les vignettes des vidéos d'un dossier (lecture seule).", "dossier");
    QCommandLineOption keep("keep", "Conserve le dossier temporaire.");
    QCommandLineOption indexOnly("index-only", "N'attend pas les vignettes (mesure SC-002 rapide).");
    QCommandLineOption thumbCost("thumb-cost", "Détaille le coût d'une vignette.");
    QCommandLineOption size("size", "Taille des photos générées (LxH).", "LxH", "6000x4000");
    QCommandLineOption duplicates("duplicates", "Mesure aussi l'analyse des doublons (SC-003 de la 002).");
    p.addOptions({items, warm, viewer, live, videos, keep, indexOnly, thumbCost, size, duplicates});
    p.process(app);

    QTemporaryDir work(QDir::tempPath() + "/picazoo-bench-XXXXXX");
    work.setAutoRemove(!p.isSet(keep));
    out << "Dossier de travail : " << QDir::toNativeSeparators(work.path()) << "\n";

    const int n = p.value(items).toInt();
    if (n > 0) {
        const QStringList wh = p.value(size).split(u'x');
        benchScan(work.path(), n, p.isSet(warm), p.isSet(indexOnly), QSize(wh.value(0).toInt(), wh.value(1).toInt()));
        if (p.isSet(duplicates) && !p.isSet(indexOnly))
            benchDuplicates(work.path(), n);
    }
    if (p.isSet(thumbCost)) {
        benchThumbCost(work.path());
        if (results.isEmpty() && n == 0 && !p.isSet(viewer) && !p.isSet(live) && !p.isSet(videos))
            return 0;
    }
    if (p.isSet(viewer))
        benchViewer(work.path());
    if (p.isSet(live))
        benchLive(work.path());
    if (p.isSet(videos))
        benchVideos(work.path(), p.value(videos));
    if (results.isEmpty()) {
        p.showHelp(1);
    }
    int failures = 0;
    for (const auto& r : results)
        failures += r.value > r.threshold ? 1 : 0;
    out << QStringLiteral("\n%1 mesure(s), %2 au-delà du seuil.\n").arg(results.size()).arg(failures);
    return failures == 0 ? 0 : 2;
}

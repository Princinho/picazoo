// picazoo-viewer.exe : visionneuse autonome (005). Ouvre une photo donnée par Windows
// dans un voile semi-transparent, sans bibliothèque (research V1 à V4).

#include "FileImageProvider.h"
#include "ViewerSession.h"
#include "ViewerSettings.h"

#include "platform/win/FileAssociations.h"
#include "platform/win/SingleInstance.h"

#include <windows.h>

#include <d3d11.h>
#include <dcomp.h>

#include <QCursor>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QGuiApplication>
#include <QHash>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPalette>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QScreen>
#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <cstring>
#include <thread>

using namespace picazoo;

namespace {

int runRegistration(const QStringList& args)
{
    const bool machine = args.contains(QStringLiteral("--machine"));
    const auto scope = machine ? platform::RegistryScope::Machine : platform::RegistryScope::User;
    QString error;
    bool ok = false;
    if (args.contains(QStringLiteral("--register")))
        ok = platform::FileAssociations::registerApp(scope, QCoreApplication::applicationFilePath(), &error);
    else
        ok = platform::FileAssociations::unregisterApp(scope, &error);
    return ok ? 0 : 1;
}

QString pathArgument(const QStringList& args)
{
    for (qsizetype i = 1; i < args.size(); ++i) {
        const QString& a = args[i];
        if (a.startsWith(QStringLiteral("--")))
            continue;
        QString p = a;
        if (p.startsWith(QStringLiteral("file:"), Qt::CaseInsensitive))
            p = QUrl(p).toLocalFile();
        return QDir::toNativeSeparators(QFileInfo(p).absoluteFilePath());
    }
    return {};
}

void applyDarkPalette(QGuiApplication& app)
{
    QPalette p;
    const QColor window(0x18, 0x1B, 0x21), base(0x1F, 0x23, 0x2A), text(0xE9, 0xEC, 0xF1);
    p.setColor(QPalette::Window, window);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, window);
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, base);
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::ToolTipBase, base);
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::Highlight, QColor(0x2F, 0x7B, 0xC8));
    p.setColor(QPalette::HighlightedText, Qt::white);
    app.setPalette(p);
}

void bringToFront(QQuickWindow* w)
{
    if (!w)
        return;
    w->raise();
    w->requestActivate();
    SetForegroundWindow(reinterpret_cast<HWND>(w->winId()));
}

// PICAZOO_VIEWER_TIMING : jalons du démarrage, en ms depuis la création du processus.
void mark(const char* step)
{
    static const bool on = qEnvironmentVariableIsSet("PICAZOO_VIEWER_TIMING");
    if (!on)
        return;
    FILETIME created, exited, kernel, user, now;
    GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user);
    GetSystemTimeAsFileTime(&now);
    auto toU = [](const FILETIME& ft) { return (quint64(ft.dwHighDateTime) << 32) | ft.dwLowDateTime; };
    fprintf(stderr, "picazoo-viewer : %-28s %5lld ms\n", step, (long long)((toU(now) - toU(created)) / 10000ULL));
}

// Le chargement du pilote graphique (Direct3D 11, DirectComposition) est le poste le
// plus lourd du démarrage. Le lancer tout de suite sur un autre thread le fait se
// recouvrir avec le chargement de Qt et du QML ; Qt retrouve ensuite un pilote chargé.
void warmUpGraphics()
{
    std::thread([] {
        ID3D11Device* device = nullptr;
        ID3D11DeviceContext* context = nullptr;
        const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
        if (SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                        levels, UINT(std::size(levels)), D3D11_SDK_VERSION, &device, nullptr,
                                        &context))) {
            IDXGIDevice* dxgi = nullptr;
            if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&dxgi)))) {
                IDCompositionDevice* dcomp = nullptr;
                if (SUCCEEDED(DCompositionCreateDevice(dxgi, IID_PPV_ARGS(&dcomp))))
                    dcomp->Release();
                dxgi->Release();
            }
            context->Release();
            device->Release();
        }
        mark("pilote graphique chargé");
    }).detach();
}

// Exécute une étape du script de développement puis programme la suivante.
void runScript(QQuickWindow* window, QStringList steps)
{
    if (steps.isEmpty())
        return;
    const QStringList step = steps.takeFirst().split(u':');
    int delay = 150;
    const QString kind = step.value(0);
    if (kind == u"wait") {
        delay = step.value(1).toInt();
    } else if (kind == u"key") {
        static const QHash<QString, int> keys{{QStringLiteral("Right"), Qt::Key_Right},
                                              {QStringLiteral("Left"), Qt::Key_Left},
                                              {QStringLiteral("Home"), Qt::Key_Home},
                                              {QStringLiteral("End"), Qt::Key_End},
                                              {QStringLiteral("F11"), Qt::Key_F11},
                                              {QStringLiteral("I"), Qt::Key_I},
                                              {QStringLiteral("Escape"), Qt::Key_Escape}};
        const int key = keys.value(step.value(1), Qt::Key_unknown);
        QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier);
        QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier);
        QCoreApplication::sendEvent(window, &press);
        QCoreApplication::sendEvent(window, &release);
        delay = 40;
    } else if (kind == u"wheel") {
        const QPointF pos(step.value(1).toDouble(), step.value(2).toDouble());
        const int delta = step.value(3).toInt();
        QWheelEvent wheel(pos, window->mapToGlobal(pos), QPoint(), QPoint(0, delta), Qt::NoButton, Qt::NoModifier,
                          Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(window, &wheel);
        delay = 60;
    } else if (kind == u"move") {
        const QPointF pos(step.value(1).toDouble(), step.value(2).toDouble());
        QMouseEvent move(QEvent::MouseMove, pos, window->mapToGlobal(pos), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(window, &move);
    }
    QTimer::singleShot(delay, window, [window, steps] { runScript(window, steps); });
}

} // namespace

int main(int argc, char* argv[])
{
    // Tant qu'aucune fenêtre n'est visible, Windows peut brider le processus (EcoQoS,
    // cœurs basse consommation). La visionneuse doit s'ouvrir vite : elle s'en exclut.
    PROCESS_POWER_THROTTLING_STATE throttling{};
    throttling.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
    throttling.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
    throttling.StateMask = 0;
    SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &throttling, sizeof(throttling));
    const bool registration = std::any_of(argv, argv + argc, [](const char* a) {
        return strcmp(a, "--register") == 0 || strcmp(a, "--unregister") == 0;
    });
    if (!registration)
        warmUpGraphics();

    // La fenêtre au premier plan est, normalement, l'Explorateur d'où vient la photo.
    const quintptr foreground = reinterpret_cast<quintptr>(GetForegroundWindow());

    // Aucune écriture disque hors viewer.ini (contracts/storage-and-registry.md).
    qputenv("QML_DISABLE_DISK_CACHE", "1");
    // Boucle de rendu sur le thread de l'interface : avec une fenêtre translucide
    // (DirectComposition), la boucle à thread de rendu bloque les animations (research V3).
    if (!qEnvironmentVariableIsSet("QSG_RENDER_LOOP"))
        qputenv("QSG_RENDER_LOOP", "basic");
    QQuickWindow::setDefaultAlphaBuffer(true);

    mark("main");
    QGuiApplication app(argc, argv);
    mark("QGuiApplication");
    QGuiApplication::setApplicationName(QStringLiteral("Picazoo"));
    QGuiApplication::setOrganizationName(QStringLiteral("Picazoo"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Picazoo"));

    const QStringList args = app.arguments();
    if (args.contains(QStringLiteral("--register")) || args.contains(QStringLiteral("--unregister")))
        return runRegistration(args);

    const QString path = pathArgument(args);
    platform::SingleInstance instance(QStringLiteral("Picazoo.Viewer"));
    if (!instance.isPrimary() && instance.sendToPrimary(path))
        return 0;

    if (path.isEmpty()) {
        MessageBoxW(nullptr, L"Ouvrez une photo depuis l'Explorateur de fichiers : double-clic, ou clic droit › "
                             L"Ouvrir avec › Picazoo.",
                    L"Picazoo", MB_OK | MB_ICONINFORMATION);
        return 0;
    }
    instance.listen();

    QQuickStyle::setStyle(QStringLiteral("Fusion"));
    applyDarkPalette(app);
    QFont font(QStringLiteral("Segoe UI"));
    font.setPixelSize(14);
    QGuiApplication::setFont(font);

    QScreen* screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen)
        screen = QGuiApplication::primaryScreen();

    auto* images = new FileImageProvider; // détenu par le moteur QML
    images->setScreenSize(screen->size() * screen->devicePixelRatio());
    ViewerSettings settings(ViewerSettings::defaultDataDir());
    ViewerSession session(*images, settings, screen);
    session.open(path, foreground); // lance le décodage avant le chargement du QML

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.addImageProvider(QStringLiteral("photo"), images);
    engine.rootContext()->setContextProperty(QStringLiteral("session"), &session);
    mark("avant QML");
    engine.loadFromModule("PicazooViewer", "Overlay");
    mark("QML chargé");

    QQuickWindow* window =
        engine.rootObjects().isEmpty() ? nullptr : qobject_cast<QQuickWindow*>(engine.rootObjects().first());

    QObject::connect(&instance, &platform::SingleInstance::messageReceived, &session, [&session](const QString& p) {
        session.openFromMessage(p, reinterpret_cast<quintptr>(GetForegroundWindow()));
    });
    QObject::connect(&session, &ViewerSession::raiseRequested, &app, [window] { bringToFront(window); });

    // Le voile couvre l'écran de la souris (research V3).
    if (window) {
        window->setScreen(screen);
        window->setGeometry(screen->geometry());
        QObject::connect(window, &QQuickWindow::frameSwapped, &app, [] {
            static bool first = true;
            if (first) {
                first = false;
                mark("première image du moteur");
            }
        });
        QObject::connect(window, &QQuickWindow::sceneGraphInitialized, &app, [] { mark("scène graphique prête"); },
                         Qt::DirectConnection);
        // Fenêtre sans bordure qui couvre l'écran : Windows la traite comme plein écran.
        // (showFullScreen() est plus lent et n'apporte rien ici.)
        window->show();
        mark("fenêtre affichée");
        bringToFront(window);
        mark("premier plan");
    }

    // PICAZOO_VIEWER_SCRIPT (développement) : événements envoyés à la fenêtre comme s'ils
    // venaient de l'utilisateur, ex. « wait:1200,key:Right,key:Right,wheel:1400:930:120 ».
    if (qEnvironmentVariableIsSet("PICAZOO_VIEWER_SCRIPT") && window)
        runScript(window, qEnvironmentVariable("PICAZOO_VIEWER_SCRIPT").split(u',', Qt::SkipEmptyParts));

    // PICAZOO_SCREENSHOT=<fichier.png>[;délai ms] (développement) : capture puis fermeture.
    if (qEnvironmentVariableIsSet("PICAZOO_SCREENSHOT") && window) {
        const QStringList spec = qEnvironmentVariable("PICAZOO_SCREENSHOT").split(u';');
        QTimer::singleShot(spec.value(1, QStringLiteral("2500")).toInt(), &app, [window, file = spec.first()] {
            window->grabWindow().save(file);
            QCoreApplication::quit();
        });
    }
    return app.exec();
}

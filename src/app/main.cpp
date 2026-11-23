#include "CrashTrace.h"
#include "controllers/AppController.h"
#include "controllers/DuplicatesController.h"
#include "controllers/OnboardingController.h"
#include "controllers/TrashController.h"
#include "duplicates/DuplicateEngine.h"
#include "fileops/TrashService.h"
#include "controllers/ViewerController.h"
#include "providers/ViewerImageProvider.h"
#include "index/Database.h"
#include "library/Library.h"
#include "models/FolderTreeModel.h"
#include "models/GridRowModel.h"
#include "models/ItemSelection.h"
#include "providers/ThumbnailImageProvider.h"
#include "platform/win/SingleInstance.h"

#include <windows.h>

#include <QAbstractNativeEventFilter>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QPalette>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QStandardPaths>

#include <memory>

namespace {

// Disque branché ou retiré : les racines indisponibles sont réessayées (FR-009).
class DeviceChangeFilter : public QAbstractNativeEventFilter {
public:
    explicit DeviceChangeFilter(picazoo::Library& lib) : lib_(lib) {}
    bool nativeEventFilter(const QByteArray& type, void* message, qintptr*) override
    {
        if (type == "windows_generic_MSG" && static_cast<MSG*>(message)->message == WM_DEVICECHANGE) {
            QMetaObject::invokeMethod(&lib_, [this] { lib_.checkRoots(); }, Qt::QueuedConnection);
        }
        return false;
    }

private:
    picazoo::Library& lib_;
};

// PICAZOO_LOG_FILE=<fichier> (développement) : messages de Qt et du QML dans un fichier.
void logToFile(QtMsgType, const QMessageLogContext& ctx, const QString& msg)
{
    static QFile file(qEnvironmentVariable("PICAZOO_LOG_FILE"));
    if (!file.isOpen() && !file.open(QIODevice::Append | QIODevice::Text))
        return;
    file.write((QString::fromUtf8(ctx.file ? ctx.file : "") + u':' + QString::number(ctx.line) + u' ' + msg +
                QStringLiteral("\n")).toUtf8());
    file.flush();
}

void applyDarkPalette(QGuiApplication& app)
{
    QPalette p;
    // Couleurs de qml/Theme.qml (« Atelier sombre ») pour les contrôles standard.
    const QColor window(0x18, 0x1B, 0x21), base(0x1F, 0x23, 0x2A), text(0xE9, 0xEC, 0xF1);
    const QColor accent(0x2F, 0x7B, 0xC8);
    p.setColor(QPalette::Window, window);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, window);
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, base);
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::ToolTipBase, base);
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::PlaceholderText, QColor(0x90, 0x90, 0x90));
    p.setColor(QPalette::Highlight, accent);
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::Disabled, QPalette::Text, QColor(0x80, 0x80, 0x80));
    p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x80, 0x80, 0x80));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x80, 0x80, 0x80));
    app.setPalette(p);
}

} // namespace

int main(int argc, char* argv[])
{
    crashtrace::install();
    if (qEnvironmentVariableIsSet("PICAZOO_LOG_FILE"))
        qInstallMessageHandler(logToFile);
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("Picazoo"));
    QGuiApplication::setOrganizationName(QStringLiteral("Picazoo"));
    QQuickStyle::setStyle(QStringLiteral("Fusion"));
    applyDarkPalette(app);

    // %LOCALAPPDATA%\Picazoo : index, vignettes, réglages. Rien n'est écrit ailleurs.
    // PICAZOO_DATA_DIR (développement) : bibliothèque séparée, pour les essais.
    const QString dataDir = qEnvironmentVariableIsSet("PICAZOO_DATA_DIR")
                                ? qEnvironmentVariable("PICAZOO_DATA_DIR")
                                : QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
                                      QStringLiteral("/Picazoo");

    // --show <chemin> : « Ouvrir dans Picazoo » depuis la visionneuse autonome (005, V12).
    // Une seule bibliothèque ouverte à la fois : une seconde instance transmet la demande
    // à la première, qui passe au premier plan.
    const QStringList args = app.arguments();
    QString showPath;
    if (const qsizetype i = args.indexOf(QStringLiteral("--show")); i >= 0 && i + 1 < args.size())
        showPath = QDir::toNativeSeparators(QFileInfo(args[i + 1]).absoluteFilePath());
    picazoo::platform::SingleInstance instance(
        QStringLiteral("Picazoo.App.%1").arg(qHash(QDir::cleanPath(dataDir).toLower()), 0, 16));
    if (!instance.isPrimary() && instance.sendToPrimary(showPath))
        return 0;
    instance.listen();

    std::unique_ptr<picazoo::Library> library;
    QString startupError;
    try {
        library = picazoo::Library::open(QDir::toNativeSeparators(dataDir));
        library->enableVideoThumbnails();
    } catch (const picazoo::DbError& e) {
        startupError = QStringLiteral("Picazoo ne peut pas ouvrir sa bibliothèque dans %1.\n%2\n"
                                      "Vos photos ne sont pas concernées.")
                           .arg(QDir::toNativeSeparators(dataDir), QString::fromUtf8(e.what()));
    }
    std::unique_ptr<DeviceChangeFilter> deviceFilter;
    if (library) {
        deviceFilter = std::make_unique<DeviceChangeFilter>(*library);
        app.installNativeEventFilter(deviceFilter.get());
    }

    std::unique_ptr<AppController> controller;
    std::unique_ptr<OnboardingController> onboarding;
    std::unique_ptr<GridRowModel> grid;
    std::unique_ptr<FolderTreeModel> folders;
    std::unique_ptr<ItemSelection> selection;
    if (library) {
        controller = std::make_unique<AppController>(*library);
        onboarding = std::make_unique<OnboardingController>(*library);
        grid = std::make_unique<GridRowModel>(*library);
        folders = std::make_unique<FolderTreeModel>(*library);
        selection = std::make_unique<ItemSelection>(*grid);
        QObject::connect(controller.get(), &AppController::sortChanged, grid.get(), &GridRowModel::reload);
        QObject::connect(controller.get(), &AppController::sortChanged, folders.get(), &FolderTreeModel::reload);
    }

    // Doublons (002) : moteur en arrière-plan, corbeille sûre. Détruits avant la bibliothèque.
    std::unique_ptr<picazoo::DuplicateEngine> dupEngine;
    std::unique_ptr<picazoo::TrashService> trashService;
    std::unique_ptr<DuplicatesController> duplicates;
    std::unique_ptr<TrashController> trash;
    if (library) {
        dupEngine = std::make_unique<picazoo::DuplicateEngine>(*library);
        trashService = std::make_unique<picazoo::TrashService>(*library, *dupEngine);
        duplicates = std::make_unique<DuplicatesController>(*library, *dupEngine);
        trash = std::make_unique<TrashController>(*library, *dupEngine, *trashService);
    }

    // Le moteur QML est détruit avant les modèles, la visionneuse et la bibliothèque qu'il
    // référence. (La visionneuse ne touche pas à son fournisseur d'images en se détruisant.)
    std::unique_ptr<ViewerController> viewer;
    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    if (library) {
        engine.addImageProvider(QStringLiteral("thumb"), new ThumbnailImageProvider(*library));
        auto* viewerImages = new ViewerImageProvider(*library);
        engine.addImageProvider(QStringLiteral("viewer"), viewerImages);
        viewer = std::make_unique<ViewerController>(*library, *grid, *viewerImages);
    }
    auto* ctx = engine.rootContext();
    ctx->setContextProperty(QStringLiteral("viewer"), viewer.get());
    ctx->setContextProperty(QStringLiteral("startupError"), startupError);
    // PICAZOO_DEV_OPEN=<n> (développement) : ouvre la visionneuse sur le n-ième élément.
    ctx->setContextProperty(QStringLiteral("devOpenIndex"),
                            qEnvironmentVariableIsSet("PICAZOO_DEV_OPEN") ? qEnvironmentVariableIntValue("PICAZOO_DEV_OPEN") : -1);
    ctx->setContextProperty(QStringLiteral("app"), controller.get());
    ctx->setContextProperty(QStringLiteral("onboarding"), onboarding.get());
    ctx->setContextProperty(QStringLiteral("gridModel"), grid.get());
    ctx->setContextProperty(QStringLiteral("folderModel"), folders.get());
    ctx->setContextProperty(QStringLiteral("selection"), selection.get());
    ctx->setContextProperty(QStringLiteral("dups"), duplicates.get());
    ctx->setContextProperty(QStringLiteral("trash"), trash.get());

    engine.loadFromModule("Picazoo", "Main");

    QQuickWindow* mainWindow =
        engine.rootObjects().isEmpty() ? nullptr : qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    auto bringToFront = [mainWindow] {
        if (!mainWindow)
            return;
        if (mainWindow->visibility() == QWindow::Minimized)
            mainWindow->showNormal();
        mainWindow->raise();
        mainWindow->requestActivate();
    };
    QObject::connect(&instance, &picazoo::platform::SingleInstance::messageReceived, &app,
                     [c = controller.get(), bringToFront](const QString& path) {
                         bringToFront();
                         if (c && !path.isEmpty())
                             c->showPath(path);
                     });
    if (controller && !showPath.isEmpty())
        QTimer::singleShot(300, controller.get(), [c = controller.get(), showPath] { c->showPath(showPath); });

    // PICAZOO_DEV_SCREEN=duplicates|summary|log|dossiers|aside|selection (développement) : ouvre cet écran au démarrage.
    if (qEnvironmentVariableIsSet("PICAZOO_DEV_SCREEN") && !engine.rootObjects().isEmpty()) {
        QObject* root = engine.rootObjects().first();
        QTimer::singleShot(1500, root, [root] {
            QMetaObject::invokeMethod(root, "devOpenScreen", Q_ARG(QVariant, qEnvironmentVariable("PICAZOO_DEV_SCREEN")));
        });
    }

    // PICAZOO_SCREENSHOT=<fichier.png>[;délai ms] (développement) : capture la fenêtre puis quitte.
    if (qEnvironmentVariableIsSet("PICAZOO_SCREENSHOT") && !engine.rootObjects().isEmpty()) {
        const QStringList spec = qEnvironmentVariable("PICAZOO_SCREENSHOT").split(u';');
        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        QTimer::singleShot(spec.value(1, "4000").toInt(), &app, [window, file = spec.first()] {
            if (window)
                window->grabWindow().save(file);
            QCoreApplication::quit();
        });
    }
    return app.exec();
}

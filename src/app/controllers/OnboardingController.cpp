#include "controllers/OnboardingController.h"

#include "controllers/AppController.h"
#include "library/Library.h"

using namespace picazoo;

OnboardingController::OnboardingController(Library& library, QObject* parent) : QObject(parent), lib_(library) {}

bool OnboardingController::done() const { return lib_.setting(QStringLiteral("onboarding_done")).toInt() != 0; }

QStringList OnboardingController::defaultFolders() const { return Library::defaultRoots(); }

void OnboardingController::start(bool useDefaults, const QList<QUrl>& folders)
{
    const QStringList paths = [&] {
        if (useDefaults)
            return Library::defaultRoots();
        QStringList out;
        for (const QUrl& u : folders)
            out << AppController::urlToPath(u);
        return out;
    }();
    if (paths.isEmpty())
        return;
    for (const QString& p : paths)
        lib_.addRoot(p);
    lib_.setSetting(QStringLiteral("onboarding_done"), 1);
    emit doneChanged();
}

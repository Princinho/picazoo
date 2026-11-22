#pragma once

// Premier lancement (T046) : « Où sont vos photos ? ».

#include <QObject>
#include <QStringList>
#include <QUrl>

namespace picazoo {
class Library;
}

class OnboardingController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool done READ done NOTIFY doneChanged)
    Q_PROPERTY(QStringList defaultFolders READ defaultFolders CONSTANT)

public:
    explicit OnboardingController(picazoo::Library& library, QObject* parent = nullptr);

    bool done() const;
    QStringList defaultFolders() const;

    // Ajoute les racines (emplacements par défaut ou dossiers choisis), lance l'analyse.
    Q_INVOKABLE void start(bool useDefaults, const QList<QUrl>& folders);

signals:
    void doneChanged();

private:
    picazoo::Library& lib_;
};

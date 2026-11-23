#pragma once

// Pont entre le service de corbeille et le QML : récapitulatif, confirmation, progression,
// journal et annulation (FR-016 à FR-022).

#include "fileops/TrashService.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

#include <optional>

namespace picazoo {
class DuplicateEngine;
class Library;
}

class TrashController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString progressText READ progressText NOTIFY progressChanged)
    Q_PROPERTY(QVariantList journal READ journal NOTIFY journalChanged)

public:
    TrashController(picazoo::Library& library, picazoo::DuplicateEngine& engine, picazoo::TrashService& service,
                    QObject* parent = nullptr);

    bool busy() const { return service_.busy(); }
    QString progressText() const { return progressText_; }
    QVariantList journal() const;

    // Récapitulatif (ne touche à rien). Liste vide : tous les groupes à revoir.
    Q_INVOKABLE QVariantMap prepare(const QVariantList& groupIds);
    Q_INVOKABLE QVariantMap prepareAllIdentical();
    // Exécute le dernier récapitulatif préparé, après confirmation explicite.
    Q_INVOKABLE bool confirm();
    Q_INVOKABLE QVariantMap operationDetails(qint64 operationId) const;
    Q_INVOKABLE bool undo(qint64 operationId);

signals:
    void busyChanged();
    void progressChanged();
    void journalChanged();
    void finished(const QString& message);

private:
    QVariantMap describe(const picazoo::TrashPlan& plan) const;

    picazoo::Library& lib_;
    picazoo::DuplicateEngine& engine_;
    picazoo::TrashService& service_;
    std::optional<picazoo::TrashPlan> lastPlan_;
    QString progressText_;
};

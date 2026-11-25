#pragma once

// Pont entre le service de corbeille et le QML : récapitulatif, confirmation, progression,
// journal et annulation (FR-016 à FR-022 de la 002). 006 : récapitulatif préparé hors du fil de
// l'interface, résumé par niveau et par dossier, niveaux à inclure, arrêt (research R7, R8).

#include "fileops/TrashService.h"

#include <QObject>
#include <QSet>
#include <QVariantList>
#include <QVariantMap>

#include <atomic>
#include <optional>
#include <thread>

namespace picazoo {
class DuplicateEngine;
class Library;
}

class TrashController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString progressText READ progressText NOTIFY progressChanged)
    Q_PROPERTY(double progressValue READ progressValue NOTIFY progressChanged) // 0–1
    Q_PROPERTY(bool preparing READ preparing NOTIFY planChanged)
    Q_PROPERTY(QVariantMap plan READ plan NOTIFY planChanged)
    Q_PROPERTY(QVariantList journal READ journal NOTIFY journalChanged)

public:
    TrashController(picazoo::Library& library, picazoo::DuplicateEngine& engine, picazoo::TrashService& service,
                    QObject* parent = nullptr);
    ~TrashController() override;

    bool busy() const { return service_.busy(); }
    QString progressText() const { return progressText_; }
    double progressValue() const { return progressValue_; }
    bool preparing() const { return preparing_; }
    QVariantMap plan() const { return described_; }
    QVariantList journal() const;

    // Récapitulatif des groupes donnés (006 : jamais « tous les groupes »), préparé sur un fil de
    // travail ; `plan` est mis à jour à la fin (`preparing` passe à faux). Ne touche à rien.
    Q_INVOKABLE void prepare(const QVariantList& groupIds);
    // Inclure ou non un niveau : identical | same_image | similar | similar_unchecked (FR-018).
    Q_INVOKABLE void setLevelIncluded(const QString& key, bool included);
    Q_INVOKABLE QVariantList folderFiles(int folderIndex) const; // détail d'un dossier déplié
    // Exécute le dernier récapitulatif préparé (niveaux inclus seulement), après confirmation.
    Q_INVOKABLE bool confirm();
    Q_INVOKABLE void cancelPlan();
    Q_INVOKABLE void stop();
    Q_INVOKABLE QVariantMap operationDetails(qint64 operationId) const;
    Q_INVOKABLE bool undo(qint64 operationId);

    // 007 : « Mettre à la corbeille… » depuis la bibliothèque ou la visionneuse intégrée. Le plan
    // est préparé hors du fil de l'interface et arrive par itemsPlanReady ; confirmItems() exécute
    // le dernier plan préparé ; itemsTrashed() annonce le résultat (message « Annuler »).
    Q_INVOKABLE void prepareItems(const QVariantList& ids);
    Q_INVOKABLE bool confirmItems(const QString& source); // "library" | "viewer"
    Q_INVOKABLE void cancelItems();

signals:
    void busyChanged();
    void progressChanged();
    void planChanged();
    void journalChanged();
    void finished(const QString& message);
    void itemsPlanReady(const QVariantMap& plan);
    void itemsTrashed(qint64 operationId, int count, const QString& message);

private:
    QVariantMap describe() const;
    picazoo::TrashPlan included() const;
    void joinPlanner();

    picazoo::Library& lib_;
    picazoo::DuplicateEngine& engine_;
    picazoo::TrashService& service_;
    std::optional<picazoo::TrashPlan> lastPlan_;
    QSet<QString> excluded_; // niveaux décochés
    QVariantMap described_;
    mutable QList<QPair<QString, QList<picazoo::TrashPlanEntry>>> folders_; // dossier -> fichiers (inclus)
    bool preparing_ = false;
    std::thread planner_;
    std::atomic<int> generation_{0};
    std::optional<picazoo::TrashPlan> itemsPlan_;
    QSet<qint64> itemOperations_; // opérations lancées par confirmItems()
    std::thread itemsPlanner_;
    std::atomic<int> itemsGeneration_{0};
    QString progressText_;
    double progressValue_ = 0;
};

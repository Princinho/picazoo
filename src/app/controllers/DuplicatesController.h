#pragma once

// Pont entre le moteur des doublons et le QML : résumé, progression, filtres, décisions
// (contracts/ui-contract.md). Ne lit aucun fichier de l'utilisateur.

#include <QObject>
#include <QString>

namespace picazoo {
class DuplicateEngine;
class Library;
}
class DuplicateGroupModel;

class DuplicatesController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* groups READ groups CONSTANT)
    Q_PROPERTY(int groupCount READ groupCount NOTIFY summaryChanged)
    Q_PROPERTY(QString summaryText READ summaryText NOTIFY summaryChanged)       // « 128 groupes · 3,4 Go »
    Q_PROPERTY(int trashCount READ trashCount NOTIFY summaryChanged)             // copies cochées « à la corbeille »
    Q_PROPERTY(QString trashText READ trashText NOTIFY summaryChanged)           // « 214 fichiers · 2,1 Go »
    Q_PROPERTY(int identicalGroupCount READ identicalGroupCount NOTIFY summaryChanged)
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)  // all | identical | same_image | similar
    Q_PROPERTY(bool active READ active NOTIFY progressChanged)                   // analyse en cours
    Q_PROPERTY(bool waiting READ waiting NOTIFY progressChanged)                 // analyse en attente
    Q_PROPERTY(bool paused READ paused NOTIFY progressChanged)
    Q_PROPERTY(bool autoSearch READ autoSearch WRITE setAutoSearch NOTIFY progressChanged)
    Q_PROPERTY(QString progressText READ progressText NOTIFY progressChanged)
    Q_PROPERTY(double progressValue READ progressValue NOTIFY progressChanged)  // 0–1, -1 si inconnue

public:
    DuplicatesController(picazoo::Library& library, picazoo::DuplicateEngine& engine, QObject* parent = nullptr);

    QObject* groups() const;
    int groupCount() const { return groupCount_; }
    QString summaryText() const;
    int trashCount() const { return trashCount_; }
    QString trashText() const;
    int identicalGroupCount() const { return identicalCount_; }
    QString filter() const { return filter_; }
    void setFilter(const QString& f);
    bool active() const;
    bool waiting() const;
    bool paused() const;
    bool autoSearch() const;
    void setAutoSearch(bool on);
    QString progressText() const;
    double progressValue() const;

    Q_INVOKABLE bool setKeep(qint64 groupId, qint64 itemId, bool keep);
    Q_INVOKABLE void keepOnly(qint64 groupId, qint64 itemId);
    Q_INVOKABLE void resetChoice(qint64 groupId);
    Q_INVOKABLE void markNotDuplicates(qint64 groupId);
    Q_INVOKABLE void removeFromGroup(qint64 groupId, qint64 itemId);
    Q_INVOKABLE void keepAll(qint64 groupId);
    Q_INVOKABLE void togglePause();
    Q_INVOKABLE void rescan();
    Q_INVOKABLE void hold();
    Q_INVOKABLE void release();
    Q_INVOKABLE qint64 groupOf(qint64 itemId) const;
    Q_INVOKABLE int duplicateCount(qint64 itemId) const;
    Q_INVOKABLE void showGroup(qint64 groupId) { emit showGroupRequested(groupId); }

signals:
    void summaryChanged();
    void filterChanged();
    void progressChanged();
    void showGroupRequested(qint64 groupId);

private:
    void refresh();

    picazoo::Library& lib_;
    picazoo::DuplicateEngine& engine_;
    DuplicateGroupModel* model_;
    QString filter_ = QStringLiteral("all");
    int groupCount_ = 0;
    qint64 reclaimable_ = 0;
    int trashCount_ = 0;
    int identicalCount_ = 0;
};

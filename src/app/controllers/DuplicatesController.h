#pragma once

// Pont entre le moteur des doublons et le QML : résumé, progression, filtres, décisions
// (contracts/ui-contract.md de la 002) ; 006 : filtres combinés, sélection et actions en lot,
// règles et paires de dossiers, « Écartés de la revue ». Ne lit aucun fichier de l'utilisateur.

#include "duplicates/DuplicateTypes.h"

#include <QObject>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QVariantList>

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
    Q_PROPERTY(QVariantMap levelCounts READ levelCounts NOTIFY summaryChanged)  // all, identical, same_image, similar
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)  // all | identical | same_image | similar
    Q_PROPERTY(bool active READ active NOTIFY progressChanged)                   // analyse en cours
    Q_PROPERTY(bool waiting READ waiting NOTIFY progressChanged)                 // analyse en attente
    Q_PROPERTY(bool paused READ paused NOTIFY progressChanged)
    Q_PROPERTY(bool autoSearch READ autoSearch WRITE setAutoSearch NOTIFY progressChanged)
    Q_PROPERTY(QString progressText READ progressText NOTIFY progressChanged)
    Q_PROPERTY(double progressValue READ progressValue NOTIFY progressChanged)  // 0–1, -1 si inconnue

    // --- 006 : filtres, tri, affichage (retenus dans les réglages, sauf dossier et paire)
    Q_PROPERTY(QString folderFilter READ folderFilter WRITE setFolderFilter NOTIFY filterChanged)
    Q_PROPERTY(QString pairA READ pairA NOTIFY filterChanged)
    Q_PROPERTY(QString pairB READ pairB NOTIFY filterChanged)
    Q_PROPERTY(QString typeFilter READ typeFilter WRITE setTypeFilter NOTIFY filterChanged)    // any | photos | videos | raw_jpeg
    Q_PROPERTY(QString stateFilter READ stateFilter WRITE setStateFilter NOTIFY filterChanged) // any | untouched | modified | flagged
    Q_PROPERTY(int minMegabytes READ minMegabytes WRITE setMinMegabytes NOTIFY filterChanged)  // 0 = aucun
    Q_PROPERTY(int yearFrom READ yearFrom NOTIFY filterChanged)                                // 0 = ouvert
    Q_PROPERTY(int yearTo READ yearTo NOTIFY filterChanged)
    Q_PROPERTY(QString sort READ sort WRITE setSort NOTIFY filterChanged)                      // reclaimable | copies | date | folder
    Q_PROPERTY(QString sortLabel READ sortLabel NOTIFY filterChanged)
    Q_PROPERTY(QString view READ view WRITE setView NOTIFY viewChanged)                        // compact | detailed
    Q_PROPERTY(QVariantList chips READ chips NOTIFY filterChanged)                             // [{key, label}]
    Q_PROPERTY(int shownCount READ shownCount NOTIFY summaryChanged)
    Q_PROPERTY(QString shownText READ shownText NOTIFY summaryChanged)                         // « 312 groupes affichés sur 1 284 »

    // --- 006 : sélection (US1)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)
    Q_PROPERTY(QString selectionText READ selectionText NOTIFY selectionChanged)               // « 37 groupes sélectionnés »
    Q_PROPERTY(QString selectionDetail READ selectionDetail NOTIFY selectionChanged)           // « 52 fichiers à la corbeille · 1,2 Go libérés »

    // --- 006 : dossiers (US2) et écartés (US5)
    Q_PROPERTY(QVariantList pairs READ pairs NOTIFY foldersChanged)
    Q_PROPERTY(QVariantList internals READ internals NOTIFY foldersChanged)
    Q_PROPERTY(QVariantList rules READ rules NOTIFY foldersChanged)
    Q_PROPERTY(bool foldersActive READ foldersActive WRITE setFoldersActive NOTIFY foldersChanged)
    Q_PROPERTY(QVariantList setAsideEntries READ setAsideEntries NOTIFY setAsideChanged)
    Q_PROPERTY(QVariantList batchList READ batchList NOTIFY setAsideChanged)
    Q_PROPERTY(int setAsideCount READ setAsideCount NOTIFY summaryChanged)

public:
    DuplicatesController(picazoo::Library& library, picazoo::DuplicateEngine& engine, QObject* parent = nullptr);

    QObject* groups() const;
    int groupCount() const { return groupCount_; }
    QString summaryText() const;
    int trashCount() const { return trashCount_; }
    QString trashText() const;
    int identicalGroupCount() const { return identicalCount_; }
    QVariantMap levelCounts() const { return levelCounts_; }
    QString filter() const { return filter_; }
    void setFilter(const QString& f);
    bool active() const;
    bool waiting() const;
    bool paused() const;
    bool autoSearch() const;
    void setAutoSearch(bool on);
    QString progressText() const;
    double progressValue() const;

    QString folderFilter() const { return folder_; }
    void setFolderFilter(const QString& path);
    QString pairA() const { return pairA_; }
    QString pairB() const { return pairB_; }
    QString typeFilter() const { return type_; }
    void setTypeFilter(const QString& t);
    QString stateFilter() const { return state_; }
    void setStateFilter(const QString& s);
    int minMegabytes() const { return minMb_; }
    void setMinMegabytes(int mb);
    int yearFrom() const { return yearFrom_; }
    int yearTo() const { return yearTo_; }
    QString sort() const { return sort_; }
    void setSort(const QString& s);
    QString sortLabel() const;
    QString view() const { return view_; }
    void setView(const QString& v);
    QVariantList chips() const;
    int shownCount() const;
    QString shownText() const;

    int selectedCount() const;
    QString selectionText() const;
    QString selectionDetail() const { return selectionDetail_; }

    QVariantList pairs() const { return pairs_; }
    QVariantList internals() const { return internals_; }
    QVariantList rules() const { return rules_; }
    bool foldersActive() const { return foldersActive_; }
    void setFoldersActive(bool on);
    QVariantList setAsideEntries() const { return setAside_; }
    QVariantList batchList() const { return batches_; }
    int setAsideCount() const { return setAsideCount_; }

    // 002
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

    // 006 : filtres
    Q_INVOKABLE void setPairFilter(const QString& a, const QString& b);
    Q_INVOKABLE void setYears(int from, int to);
    Q_INVOKABLE void clearFilter(const QString& key); // folder | pair | type | state | size | dates | all
    Q_INVOKABLE void showFolder(const QString& path);  // depuis la bibliothèque : filtre dossier, tout le reste effacé

    // 006 : sélection et actions en lot (sur les groupes sélectionnés et affichés)
    Q_INVOKABLE QVariantList selectedGroupIds() const;
    Q_INVOKABLE QVariantList selectionFolders() const; // [{path, label, count}]
    Q_INVOKABLE void batchReset();
    Q_INVOKABLE void batchKeepAll();
    Q_INVOKABLE void batchNotDuplicates();
    Q_INVOKABLE void batchKeepFolder(const QString& folder);
    Q_INVOKABLE void keepPairSide(const QString& a, const QString& b, bool keepA);
    Q_INVOKABLE bool undoBatch(qint64 batchId);

    // 006 : règles de dossier
    Q_INVOKABLE void addRule(const QString& path, const QString& kind); // prefer | avoid
    Q_INVOKABLE void changeRule(qint64 id, const QString& kind);
    Q_INVOKABLE void relinkRule(qint64 id, const QString& path);
    Q_INVOKABLE void removeRule(qint64 id);
    Q_INVOKABLE void refreshFolders();

    // 006 : écartés de la revue
    Q_INVOKABLE void refreshSetAside();
    Q_INVOKABLE void restoreSetAside(const QVariantList& indexes);
    Q_INVOKABLE QString urlToPath(const QUrl& url) const;

signals:
    void summaryChanged();
    void filterChanged();
    void viewChanged();
    void progressChanged();
    void selectionChanged();
    void foldersChanged();
    void setAsideChanged();
    void showGroupRequested(qint64 groupId);
    // Message à afficher en bas de l'écran ; batchId non nul : « Annuler » est proposé.
    void notice(const QString& message, qint64 batchId);

private:
    void refresh();
    void applyQuery();
    void afterBatch(const picazoo::BatchResult& r);
    void updateSelectionTotals();
    picazoo::GroupQuery buildQuery() const;

    picazoo::Library& lib_;
    picazoo::DuplicateEngine& engine_;
    DuplicateGroupModel* model_;
    QString filter_ = QStringLiteral("all");
    QString folder_, pairA_, pairB_;
    QString type_ = QStringLiteral("any");
    QString state_ = QStringLiteral("any");
    int minMb_ = 0;
    int yearFrom_ = 0, yearTo_ = 0;
    QString sort_ = QStringLiteral("reclaimable");
    QString view_;
    bool filtering_ = false;
    int groupCount_ = 0;
    qint64 reclaimable_ = 0;
    int trashCount_ = 0;
    qint64 trashBytes_ = 0;
    int identicalCount_ = 0;
    int setAsideCount_ = 0;
    QVariantMap levelCounts_;
    QString selectionDetail_;
    QVariantList pairs_, internals_, rules_;
    bool foldersActive_ = false;
    QTimer foldersTimer_;
    QVariantList setAside_, batches_;
    QList<picazoo::SetAsideEntry> setAsideEntries_;
};

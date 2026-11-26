#pragma once

// Pont entre le module des images liées (003) et le QML : résumé, liste des groupes,
// corrections (contracts/ui-contract.md). Aucune action ne touche un fichier (FR-017).

#include "naming/NamingTypes.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

namespace picazoo {
class Library;
class NamingEngine;
}
class NamingGroupModel;

class NamingController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* groups READ groups CONSTANT)
    Q_PROPERTY(int groupCount READ groupCount NOTIFY changed)
    Q_PROPERTY(QString summaryText READ summaryText NOTIFY changed)       // « 24 groupes »
    Q_PROPERTY(QVariantMap counts READ counts NOTIFY changed)             // all, regular_series, same_root, generic_root
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)
    Q_PROPERTY(bool includeGeneric READ includeGeneric WRITE setIncludeGeneric NOTIFY includeGenericChanged)
    Q_PROPERTY(QVariantList decisions READ decisions NOTIFY changed)      // [{key, text}]
    Q_PROPERTY(int revision READ revision NOTIFY changed)                 // fait relire groupOf()

public:
    NamingController(picazoo::Library& library, picazoo::NamingEngine& engine, QObject* parent = nullptr);

    QObject* groups() const;
    int groupCount() const { return counts_.value(QStringLiteral("all")).toInt(); }
    QString summaryText() const;
    QVariantMap counts() const { return counts_; }
    QString filter() const { return filter_; }
    void setFilter(const QString& filter);
    bool includeGeneric() const;
    void setIncludeGeneric(bool on);
    QVariantList decisions() const { return decisions_; }
    int revision() const { return revision_; }

    // Groupe d'un élément pour le panneau d'informations : {id, name, count}, vide sinon.
    Q_INVOKABLE QVariantMap groupOf(qint64 itemId) const;
    Q_INVOKABLE void showGroup(qint64 groupId) { emit showGroupRequested(groupId); }
    Q_INVOKABLE QString thumbUrl(qint64 itemId) const; // vignette actuelle d'un élément
    Q_INVOKABLE void rescan();

    // Corrections : faux si rien n'a été enregistré (refus, ou stockage occupé : `failed`).
    Q_INVOKABLE bool rename(qint64 groupId, const QString& name);
    Q_INVOKABLE bool confirmGroup(qint64 groupId);
    Q_INVOKABLE bool removeItem(qint64 groupId, qint64 itemId);
    Q_INVOKABLE bool addItems(qint64 groupId, const QVariantList& itemIds);
    Q_INVOKABLE bool merge(qint64 intoGroupId, qint64 fromGroupId, const QString& name);
    Q_INVOKABLE bool split(qint64 groupId, const QVariantList& itemIds, const QString& name);
    Q_INVOKABLE bool notAGroup(qint64 groupId);
    Q_INVOKABLE bool undoDecision(const QString& rootKey);

    // Sélecteurs : éléments à ajouter, autres groupes à fusionner.
    Q_INVOKABLE QVariantList searchItems(qint64 groupId, const QString& text) const;
    Q_INVOKABLE QVariantList otherGroups(qint64 groupId, const QString& text) const; // [{id, name, count}]

signals:
    void changed();
    void filterChanged();
    void includeGenericChanged();
    void showGroupRequested(qint64 groupId);
    void itemUpdated(qint64 itemId);
    void failed(const QString& message); // une correction n'a pas pu être enregistrée

private:
    void refresh();
    void rootsChanged();
    template <typename Fn> bool attempt(Fn fn);

    picazoo::Library& lib_;
    picazoo::NamingEngine& engine_;
    NamingGroupModel* model_;
    QString filter_ = QStringLiteral("all");
    QVariantMap counts_;
    QVariantList decisions_;
    QList<picazoo::NamingGroupRow> rows_; // toutes les lignes, relues une fois par changement
    QHash<qint64, int> rootStates_;      // disponibilité des dossiers surveillés
    int revision_ = 0;
};

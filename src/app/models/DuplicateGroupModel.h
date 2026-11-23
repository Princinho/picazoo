#pragma once

// Liste des groupes de doublons pour l'écran « Doublons » (contracts/ui-contract.md de la 002 et
// de la 006). Chaque ligne décrit un groupe et ses copies ; le détail est construit à la demande.
// 006 : la liste suit une GroupQuery (filtres combinés) et tient la sélection de groupes, limitée
// aux groupes affichés (FR-003).

#include "duplicates/DuplicateTypes.h"

#include <QAbstractListModel>
#include <QHash>
#include <QSet>
#include <QVariantMap>

namespace picazoo {
class DuplicateStore;
class Library;
}

class DuplicateGroupModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)

public:
    enum Roles {
        GroupIdRole = Qt::UserRole + 1,
        LevelRole,
        LevelLabelRole,
        ReasonLabelRole,
        ReclaimableTextRole,
        CopyCountRole,
        CopiesRole,
        SelectedRole,
        KeeperNameRole,
        KeeperFolderRole,
        OutTextRole,
        FlagTextRole,
    };

    DuplicateGroupModel(picazoo::Library& library, picazoo::DuplicateStore& store, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const { return int(ids_.size()); }

    void setFilter(picazoo::LevelFilter f);
    void setQuery(const picazoo::GroupQuery& q);
    const picazoo::GroupQuery& query() const { return query_; }
    // Relit les groupes ; garde la position si la liste n'a pas changé.
    void reload();
    // Relit une seule ligne (choix d'une copie).
    void refreshGroup(picazoo::GroupId id);
    QList<picazoo::GroupId> ids() const { return ids_; }

    Q_INVOKABLE int rowOfGroup(qint64 groupId) const;
    Q_INVOKABLE QVariantMap groupAt(int row) const;

    // --- Sélection (006, US1)
    int selectedCount() const { return int(selected_.size()); }
    QList<picazoo::GroupId> selectedIds() const; // dans l'ordre de la liste
    Q_INVOKABLE bool isSelected(qint64 groupId) const { return selected_.contains(groupId); }
    Q_INVOKABLE void toggle(qint64 groupId);
    Q_INVOKABLE void setSelected(qint64 groupId, bool on);
    Q_INVOKABLE void selectRange(int fromRow, int toRow); // ajoute la plage (bornes incluses)
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void clearSelection();

signals:
    void countChanged();
    void selectionChanged();
    // Des groupes sélectionnés ne sont plus affichés (filtre, regroupement) : ils quittent la sélection.
    void selectionDropped(int count);

private:
    QVariantMap build(picazoo::GroupId id) const;
    void intersectSelection();
    void emitSelectedRows(const QSet<picazoo::GroupId>& ids);

    picazoo::Library& lib_;
    picazoo::DuplicateStore& store_;
    picazoo::GroupQuery query_;
    QList<picazoo::GroupId> ids_;
    QHash<picazoo::GroupId, int> rowOf_;
    QSet<picazoo::GroupId> selected_;
    mutable QHash<picazoo::GroupId, QVariantMap> cache_;
};

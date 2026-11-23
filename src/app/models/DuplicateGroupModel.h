#pragma once

// Liste des groupes de doublons pour l'écran « Doublons » (contracts/ui-contract.md).
// Chaque ligne décrit un groupe et ses copies ; le détail est construit à la demande.

#include "duplicates/DuplicateTypes.h"

#include <QAbstractListModel>
#include <QHash>
#include <QVariantMap>

namespace picazoo {
class DuplicateStore;
class Library;
}

class DuplicateGroupModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        GroupIdRole = Qt::UserRole + 1,
        LevelRole,
        LevelLabelRole,
        ReasonLabelRole,
        ReclaimableTextRole,
        CopyCountRole,
        CopiesRole,
    };

    DuplicateGroupModel(picazoo::Library& library, picazoo::DuplicateStore& store, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const { return int(ids_.size()); }

    void setFilter(picazoo::LevelFilter f);
    // Relit les groupes ; garde la position si la liste n'a pas changé.
    void reload();
    // Relit une seule ligne (choix d'une copie).
    void refreshGroup(picazoo::GroupId id);
    QList<picazoo::GroupId> ids() const { return ids_; }

    Q_INVOKABLE int rowOfGroup(qint64 groupId) const;
    Q_INVOKABLE QVariantMap groupAt(int row) const;

signals:
    void countChanged();

private:
    QVariantMap build(picazoo::GroupId id) const;

    picazoo::Library& lib_;
    picazoo::DuplicateStore& store_;
    picazoo::LevelFilter filter_ = picazoo::LevelFilter::All;
    QList<picazoo::GroupId> ids_;
    mutable QHash<picazoo::GroupId, QVariantMap> cache_;
};

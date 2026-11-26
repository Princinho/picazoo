#pragma once

// Liste des groupes de l'écran « Images liées » (contracts/ui-contract.md). Le détail d'un
// groupe n'est lu qu'à l'affichage de sa ligne ; un rechargement ne touche qu'aux lignes
// ajoutées, retirées ou modifiées, pour que la liste ne saute pas pendant une analyse.

#include "naming/NamingTypes.h"

#include <QAbstractListModel>
#include <QHash>
#include <QSet>
#include <QVariantMap>

namespace picazoo {
class NamingStore;
}

class NamingGroupModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        GroupIdRole = Qt::UserRole + 1,
        NameRole,
        ConfidenceRole,
        ConfidenceLabelRole,
        ConfirmedRole,
        MemberCountRole,
        MembersRole,
    };

    NamingGroupModel(picazoo::NamingStore& store, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const { return int(rows_.size()); }

    void setFilter(picazoo::ConfidenceFilter filter);
    // Toutes les lignes, lues une fois par le contrôleur ; le filtre s'applique en mémoire.
    void reload(const QList<picazoo::NamingGroupRow>& all);
    // Vrai si l'élément figure dans une ligne déjà construite (carte affichée).
    bool showsItem(picazoo::ItemId id) const { return shown_.contains(id); }

    Q_INVOKABLE int rowOfGroup(qint64 groupId) const;

signals:
    void countChanged();

private:
    QVariantMap build(picazoo::NamingGroupId id) const;
    QList<picazoo::NamingGroupRow> filtered() const;

    picazoo::NamingStore& store_;
    picazoo::ConfidenceFilter filter_ = picazoo::ConfidenceFilter::All;
    QList<picazoo::NamingGroupRow> all_;
    QList<picazoo::NamingGroupRow> rows_;
    mutable QHash<picazoo::NamingGroupId, QVariantMap> cache_;
    mutable QSet<picazoo::ItemId> shown_;
};

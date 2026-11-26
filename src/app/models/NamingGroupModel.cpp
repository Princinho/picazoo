#include "models/NamingGroupModel.h"

#include "naming/NamingStore.h"

#include <QSet>

using namespace picazoo;

NamingGroupModel::NamingGroupModel(NamingStore& store, QObject* parent) : QAbstractListModel(parent), store_(store)
{
}

int NamingGroupModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : int(rows_.size()); }

QHash<int, QByteArray> NamingGroupModel::roleNames() const
{
    return {
        {GroupIdRole, "groupId"},
        {NameRole, "name"},
        {ConfidenceRole, "confidence"},
        {ConfidenceLabelRole, "confidenceLabel"},
        {ConfirmedRole, "confirmed"},
        {MemberCountRole, "memberCount"},
        {MembersRole, "members"},
    };
}

QVariant NamingGroupModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= rows_.size())
        return {};
    const NamingGroupId id = rows_[index.row()].id;
    if (role == GroupIdRole)
        return id;
    auto it = cache_.constFind(id);
    if (it == cache_.constEnd())
        it = cache_.insert(id, build(id));
    return it->value(QString::fromLatin1(roleNames().value(role)));
}

QVariantMap NamingGroupModel::build(NamingGroupId id) const
{
    QVariantMap out;
    const auto group = store_.group(id);
    if (!group)
        return out;
    QVariantList members;
    for (const NamingMember& m : group->members) {
        shown_.insert(m.item);
        members << QVariantMap{
            {QStringLiteral("id"), m.item},
            {QStringLiteral("name"), m.fileName},
            {QStringLiteral("folder"), m.folderPath},
            {QStringLiteral("thumb"), QStringLiteral("image://thumb/%1/%2/%3")
                                          .arg(m.item)
                                          .arg(m.version)
                                          .arg(toDb(m.thumbState))},
            {QStringLiteral("video"), m.kind == Kind::Video},
            {QStringLiteral("available"), m.available},
        };
    }
    out.insert(QStringLiteral("name"), group->displayName);
    out.insert(QStringLiteral("confidence"), toDb(group->confidence));
    out.insert(QStringLiteral("confidenceLabel"), confidenceLabel(group->confidence));
    out.insert(QStringLiteral("confirmed"), group->state == NamingGroupState::Confirmed);
    out.insert(QStringLiteral("memberCount"), int(members.size()));
    out.insert(QStringLiteral("members"), members);
    return out;
}

QList<NamingGroupRow> NamingGroupModel::filtered() const
{
    if (filter_ == ConfidenceFilter::All)
        return all_;
    const Confidence wanted = filter_ == ConfidenceFilter::RegularSeries ? Confidence::RegularSeries
                              : filter_ == ConfidenceFilter::SameRoot    ? Confidence::SameRoot
                                                                         : Confidence::GenericRoot;
    QList<NamingGroupRow> out;
    for (const auto& row : all_) {
        if (row.confidence == wanted)
            out << row;
    }
    return out;
}

void NamingGroupModel::setFilter(ConfidenceFilter filter)
{
    if (filter == filter_)
        return;
    filter_ = filter;
    beginResetModel();
    rows_ = filtered();
    cache_.clear();
    shown_.clear();
    endResetModel();
    emit countChanged();
}

void NamingGroupModel::reload(const QList<NamingGroupRow>& all)
{
    all_ = all;
    const QList<NamingGroupRow> fresh = filtered();
    const int before = int(rows_.size());
    QSet<NamingGroupId> kept;
    for (const auto& row : fresh)
        kept.insert(row.id);

    for (int i = int(rows_.size()) - 1; i >= 0; --i) {
        if (!kept.contains(rows_[i].id)) {
            beginRemoveRows({}, i, i);
            cache_.remove(rows_[i].id);
            rows_.removeAt(i);
            endRemoveRows();
        }
    }
    QSet<NamingGroupId> known;
    for (const auto& row : std::as_const(rows_))
        known.insert(row.id);

    for (int i = 0; i < fresh.size(); ++i) {
        const NamingGroupRow& row = fresh[i];
        if (!known.contains(row.id)) {
            beginInsertRows({}, i, i);
            rows_.insert(i, row);
            endInsertRows();
            continue;
        }
        if (rows_[i].id != row.id) { // renommé : la ligne change de place
            int from = i + 1;
            while (rows_[from].id != row.id)
                ++from;
            beginMoveRows({}, from, from, {}, i);
            rows_.move(from, i);
            endMoveRows();
        }
        if (rows_[i].signature != row.signature) {
            rows_[i] = row;
            cache_.remove(row.id);
            emit dataChanged(index(i), index(i));
        }
    }
    if (before != rows_.size())
        emit countChanged();
}

int NamingGroupModel::rowOfGroup(qint64 groupId) const
{
    for (int i = 0; i < rows_.size(); ++i) {
        if (rows_[i].id == groupId)
            return i;
    }
    return -1;
}

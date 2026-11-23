#include "models/DuplicateGroupModel.h"

#include "controllers/AppController.h"
#include "duplicates/DuplicateStore.h"
#include "library/Library.h"
#include "platform/win/Paths.h"

using namespace picazoo;

namespace {

QString levelKey(DupLevel l)
{
    switch (l) {
    case DupLevel::Identical: return QStringLiteral("identical");
    case DupLevel::SameImage: return QStringLiteral("same_image");
    case DupLevel::Similar: return QStringLiteral("similar");
    }
    return {};
}

} // namespace

DuplicateGroupModel::DuplicateGroupModel(Library& library, DuplicateStore& store, QObject* parent)
    : QAbstractListModel(parent), lib_(library), store_(store)
{
    reload();
}

int DuplicateGroupModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : int(ids_.size()); }

QHash<int, QByteArray> DuplicateGroupModel::roleNames() const
{
    return {
        {GroupIdRole, "groupId"},
        {LevelRole, "level"},
        {LevelLabelRole, "levelLabel"},
        {ReasonLabelRole, "reasonLabel"},
        {ReclaimableTextRole, "reclaimableText"},
        {CopyCountRole, "copyCount"},
        {CopiesRole, "copies"},
    };
}

QVariantMap DuplicateGroupModel::build(GroupId id) const
{
    auto it = cache_.constFind(id);
    if (it != cache_.constEnd())
        return *it;
    QVariantMap m;
    const auto g = store_.group(id);
    if (!g)
        return m;
    m.insert(QStringLiteral("groupId"), g->id);
    m.insert(QStringLiteral("level"), levelKey(g->level));
    m.insert(QStringLiteral("levelLabel"), levelLabel(g->level));
    m.insert(QStringLiteral("reasonLabel"), reasonLabel(g->reason));
    m.insert(QStringLiteral("reclaimableText"), AppController::formatBytes(g->reclaimableBytes));
    QVariantList copies;
    for (const DuplicateCopy& c : g->copies) {
        QVariantMap info = AppController::describe(lib_, c.item);
        if (info.isEmpty())
            continue;
        const QString path = info.value(QStringLiteral("path")).toString();
        info.insert(QStringLiteral("folder"), platform::lastSegment(platform::parentPath(path)));
        info.insert(QStringLiteral("folderPath"), platform::parentPath(path));
        info.insert(QStringLiteral("keep"), c.keep);
        info.insert(QStringLiteral("isCenter"), c.isCenter);
        info.insert(QStringLiteral("userChoice"), c.userChoice);
        info.insert(QStringLiteral("level"), levelKey(c.level));
        info.insert(QStringLiteral("levelLabel"), c.isCenter ? QString() : levelLabel(c.level));
        info.insert(QStringLiteral("rotation"), c.rotation);
        copies << info;
    }
    m.insert(QStringLiteral("copies"), copies);
    m.insert(QStringLiteral("copyCount"), int(copies.size()));
    cache_.insert(id, m);
    return m;
}

QVariant DuplicateGroupModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= ids_.size())
        return {};
    const QVariantMap m = build(ids_[index.row()]);
    switch (role) {
    case GroupIdRole: return m.value(QStringLiteral("groupId"));
    case LevelRole: return m.value(QStringLiteral("level"));
    case LevelLabelRole: return m.value(QStringLiteral("levelLabel"));
    case ReasonLabelRole: return m.value(QStringLiteral("reasonLabel"));
    case ReclaimableTextRole: return m.value(QStringLiteral("reclaimableText"));
    case CopyCountRole: return m.value(QStringLiteral("copyCount"));
    case CopiesRole: return m.value(QStringLiteral("copies"));
    default: return {};
    }
}

void DuplicateGroupModel::setFilter(LevelFilter f)
{
    if (f == filter_)
        return;
    filter_ = f;
    beginResetModel();
    ids_ = store_.groups(filter_);
    cache_.clear();
    endResetModel();
    emit countChanged();
}

void DuplicateGroupModel::reload()
{
    const QList<GroupId> fresh = store_.groups(filter_);
    cache_.clear();
    if (fresh == ids_) {
        if (!ids_.isEmpty())
            emit dataChanged(index(0), index(int(ids_.size()) - 1));
        return;
    }
    beginResetModel();
    ids_ = fresh;
    endResetModel();
    emit countChanged();
}

void DuplicateGroupModel::refreshGroup(GroupId id)
{
    cache_.remove(id);
    const int row = int(ids_.indexOf(id));
    if (row >= 0)
        emit dataChanged(index(row), index(row));
}

int DuplicateGroupModel::rowOfGroup(qint64 groupId) const { return int(ids_.indexOf(groupId)); }

QVariantMap DuplicateGroupModel::groupAt(int row) const
{
    if (row < 0 || row >= ids_.size())
        return {};
    return build(ids_[row]);
}

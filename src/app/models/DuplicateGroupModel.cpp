#include "models/DuplicateGroupModel.h"

#include "controllers/AppController.h"
#include "duplicates/DuplicateStore.h"
#include "library/Library.h"
#include "platform/win/Paths.h"

#include <QLocale>

#include <algorithm>

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

// Les deux derniers segments d'un dossier (« Sauvegarde\2019 ») : assez pour le reconnaître dans
// une ligne compacte ; le chemin complet reste dans le détail des copies.
QString shortFolder(const QString& path)
{
    const QString parent = platform::parentPath(path);
    const QString before = platform::lastSegment(parent);
    const QString last = platform::lastSegment(path);
    return before.isEmpty() || before == parent ? path : before + u'\\' + last;
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
        {SelectedRole, "selected"},
        {KeeperNameRole, "keeperName"},
        {KeeperFolderRole, "keeperFolder"},
        {OutTextRole, "outText"},
        {FlagTextRole, "flagText"},
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
    m.insert(QStringLiteral("reasonLabel"), suggestionLabel(*g));
    m.insert(QStringLiteral("reclaimableText"), AppController::formatBytes(g->reclaimableBytes));
    QString flag = flagLabel(g->flags);
    if (flag.isEmpty() && g->level == DupLevel::Similar)
        flag = QStringLiteral("À vérifier : retouche probable");
    m.insert(QStringLiteral("flagText"), flag);

    QVariantList copies;
    QString keeperName, keeperFolder;
    QList<QPair<QString, int>> out; // dossier -> copies à la corbeille, dans l'ordre d'apparition
    for (const DuplicateCopy& c : g->copies) {
        QVariantMap info = AppController::describe(lib_, c.item);
        if (info.isEmpty())
            continue;
        const QString path = info.value(QStringLiteral("path")).toString();
        const QString folderPath = platform::parentPath(path);
        info.insert(QStringLiteral("folder"), platform::lastSegment(folderPath));
        info.insert(QStringLiteral("folderPath"), folderPath);
        info.insert(QStringLiteral("keep"), c.keep);
        info.insert(QStringLiteral("isCenter"), c.isCenter);
        info.insert(QStringLiteral("isSuggested"), c.isSuggested);
        info.insert(QStringLiteral("userChoice"), c.userChoice);
        info.insert(QStringLiteral("level"), levelKey(c.level));
        info.insert(QStringLiteral("levelLabel"), c.isCenter ? QString() : levelLabel(c.level));
        info.insert(QStringLiteral("rotation"), c.rotation);
        copies << info;
        if (c.keep && (keeperName.isEmpty() || c.isSuggested)) {
            keeperName = info.value(QStringLiteral("name")).toString();
            keeperFolder = folderPath;
        }
        if (!c.keep) {
            auto f = std::find_if(out.begin(), out.end(), [&](const auto& p) { return platform::samePath(p.first, folderPath); });
            if (f == out.end())
                out << qMakePair(folderPath, 1);
            else
                ++f->second;
        }
    }
    QString outText;
    if (out.isEmpty()) {
        outText = QStringLiteral("Rien : toutes les copies sont gardées");
    } else {
        for (int i = 0; i < out.size() && i < 2; ++i) {
            if (i)
                outText += QStringLiteral(" · ");
            outText += QString::number(out[i].second) + QStringLiteral(" dans ") + shortFolder(out[i].first);
        }
        if (out.size() > 2)
            outText += QStringLiteral(" · …");
    }
    m.insert(QStringLiteral("keeperName"), keeperName);
    m.insert(QStringLiteral("keeperFolder"), shortFolder(keeperFolder));
    m.insert(QStringLiteral("outText"), outText);
    m.insert(QStringLiteral("copies"), copies);
    m.insert(QStringLiteral("copyCount"), int(copies.size()));
    cache_.insert(id, m);
    return m;
}

QVariant DuplicateGroupModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= ids_.size())
        return {};
    const GroupId id = ids_[index.row()];
    if (role == SelectedRole)
        return selected_.contains(id);
    const QVariantMap m = build(id);
    switch (role) {
    case GroupIdRole: return m.value(QStringLiteral("groupId"));
    case LevelRole: return m.value(QStringLiteral("level"));
    case LevelLabelRole: return m.value(QStringLiteral("levelLabel"));
    case ReasonLabelRole: return m.value(QStringLiteral("reasonLabel"));
    case ReclaimableTextRole: return m.value(QStringLiteral("reclaimableText"));
    case CopyCountRole: return m.value(QStringLiteral("copyCount"));
    case CopiesRole: return m.value(QStringLiteral("copies"));
    case KeeperNameRole: return m.value(QStringLiteral("keeperName"));
    case KeeperFolderRole: return m.value(QStringLiteral("keeperFolder"));
    case OutTextRole: return m.value(QStringLiteral("outText"));
    case FlagTextRole: return m.value(QStringLiteral("flagText"));
    default: return {};
    }
}

void DuplicateGroupModel::setFilter(LevelFilter f)
{
    GroupQuery q = query_;
    q.level = f;
    setQuery(q);
}

void DuplicateGroupModel::setQuery(const GroupQuery& q)
{
    query_ = q;
    beginResetModel();
    ids_ = store_.groups(query_);
    rowOf_.clear();
    for (int i = 0; i < ids_.size(); ++i)
        rowOf_.insert(ids_[i], i);
    cache_.clear();
    endResetModel();
    emit countChanged();
    intersectSelection();
}

void DuplicateGroupModel::reload()
{
    const QList<GroupId> fresh = store_.groups(query_);
    cache_.clear();
    if (fresh == ids_) {
        if (!ids_.isEmpty())
            emit dataChanged(index(0), index(int(ids_.size()) - 1));
        return;
    }
    beginResetModel();
    ids_ = fresh;
    rowOf_.clear();
    for (int i = 0; i < ids_.size(); ++i)
        rowOf_.insert(ids_[i], i);
    endResetModel();
    emit countChanged();
    intersectSelection();
}

void DuplicateGroupModel::refreshGroup(GroupId id)
{
    cache_.remove(id);
    const int row = rowOf_.value(id, -1);
    if (row >= 0)
        emit dataChanged(index(row), index(row));
}

int DuplicateGroupModel::rowOfGroup(qint64 groupId) const { return rowOf_.value(groupId, -1); }

QVariantMap DuplicateGroupModel::groupAt(int row) const
{
    if (row < 0 || row >= ids_.size())
        return {};
    return build(ids_[row]);
}

// ------------------------------------------------------------------ Sélection

QList<GroupId> DuplicateGroupModel::selectedIds() const
{
    QList<GroupId> out;
    out.reserve(selected_.size());
    for (GroupId id : ids_)
        if (selected_.contains(id))
            out << id;
    return out;
}

void DuplicateGroupModel::emitSelectedRows(const QSet<GroupId>& ids)
{
    if (ids.size() > 64) {
        if (!ids_.isEmpty())
            emit dataChanged(index(0), index(int(ids_.size()) - 1), {SelectedRole});
        return;
    }
    for (GroupId id : ids) {
        const int row = rowOf_.value(id, -1);
        if (row >= 0)
            emit dataChanged(index(row), index(row), {SelectedRole});
    }
}

void DuplicateGroupModel::toggle(qint64 groupId) { setSelected(groupId, !selected_.contains(groupId)); }

void DuplicateGroupModel::setSelected(qint64 groupId, bool on)
{
    if (!rowOf_.contains(groupId) || selected_.contains(groupId) == on)
        return;
    if (on)
        selected_.insert(groupId);
    else
        selected_.remove(groupId);
    emitSelectedRows({groupId});
    emit selectionChanged();
}

void DuplicateGroupModel::selectRange(int fromRow, int toRow)
{
    if (ids_.isEmpty())
        return;
    const int a = std::clamp(std::min(fromRow, toRow), 0, int(ids_.size()) - 1);
    const int b = std::clamp(std::max(fromRow, toRow), 0, int(ids_.size()) - 1);
    QSet<GroupId> added;
    for (int r = a; r <= b; ++r)
        if (!selected_.contains(ids_[r])) {
            selected_.insert(ids_[r]);
            added.insert(ids_[r]);
        }
    if (added.isEmpty())
        return;
    emitSelectedRows(added);
    emit selectionChanged();
}

void DuplicateGroupModel::selectAll()
{
    const QSet<GroupId> all(ids_.begin(), ids_.end());
    if (all == selected_)
        return;
    selected_ = all;
    emitSelectedRows(all);
    emit selectionChanged();
}

void DuplicateGroupModel::clearSelection()
{
    if (selected_.isEmpty())
        return;
    const QSet<GroupId> was = selected_;
    selected_.clear();
    emitSelectedRows(was);
    emit selectionChanged();
}

void DuplicateGroupModel::intersectSelection()
{
    int dropped = 0;
    for (auto it = selected_.begin(); it != selected_.end();) {
        if (rowOf_.contains(*it)) {
            ++it;
        } else {
            it = selected_.erase(it);
            ++dropped;
        }
    }
    if (dropped > 0) {
        emit selectionChanged();
        emit selectionDropped(dropped);
    }
}

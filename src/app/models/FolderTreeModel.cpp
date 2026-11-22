#include "models/FolderTreeModel.h"

#include "library/Library.h"
#include "platform/win/Paths.h"

#include <QDateTime>
#include <QLocale>
#include <QMap>
#include <QTimeZone>

#include <algorithm>
#include <functional>

using namespace picazoo;

namespace {

QString formatDate(const std::optional<qint64>& ms)
{
    if (!ms)
        return {};
    return QLocale(QLocale::French)
        .toString(QDateTime::fromMSecsSinceEpoch(*ms, QTimeZone::UTC).date(), QStringLiteral("d MMM yyyy"));
}

} // namespace

FolderTreeModel::FolderTreeModel(Library& library, QObject* parent) : QAbstractListModel(parent), lib_(library)
{
    mode_ = lib_.setting(QStringLiteral("folder_view")).toString();
    connect(&lib_, &Library::foldersChanged, this, &FolderTreeModel::reload);
    connect(&lib_, &Library::rootsChanged, this, &FolderTreeModel::reload);
    reload();
}

int FolderTreeModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : int(rows_.size()); }

QHash<int, QByteArray> FolderTreeModel::roleNames() const
{
    return {
        {RowKindRole, "rowKind"},       {FolderIdRole, "folderId"},     {NameRole, "name"},
        {PathRole, "path"},             {CountRole, "count"},           {DateTextRole, "dateText"},
        {DepthRole, "depth"},           {ExpandableRole, "expandable"}, {ExpandedRole, "expanded"},
        {UnavailableRole, "unavailable"},
    };
}

QVariant FolderTreeModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= rows_.size())
        return {};
    const Row& r = rows_[index.row()];
    switch (role) {
    case RowKindRole: return r.kind;
    case FolderIdRole: return r.folderId;
    case NameRole: return r.name;
    case PathRole: return r.path;
    case CountRole: return r.count;
    case DateTextRole: return r.dateText;
    case DepthRole: return r.depth;
    case ExpandableRole: return r.expandable;
    case ExpandedRole: return !collapsed_.contains(r.path.toLower());
    case UnavailableRole: return r.unavailable;
    default: return {};
    }
}

void FolderTreeModel::setMode(const QString& mode)
{
    const QString m = mode == u"tree" ? QStringLiteral("tree") : QStringLiteral("by_year");
    if (m == mode_)
        return;
    mode_ = m;
    lib_.setSetting(QStringLiteral("folder_view"), m);
    rebuild();
    emit modeChanged();
}

void FolderTreeModel::reload()
{
    const QString s = lib_.setting(QStringLiteral("folder_sort")).toString();
    sortByName_ = s == u"name_asc";
    const FolderSort sort = sortByName_ ? FolderSort::NameAsc : s == u"date_asc" ? FolderSort::DateAsc : FolderSort::DateDesc;
    folders_ = lib_.folders(sort);
    roots_ = lib_.roots();
    rebuild();
}

void FolderTreeModel::rebuild()
{
    QList<Row> rows;
    if (mode_ == u"tree")
        buildTree(rows);
    else
        buildByYear(rows);
    beginResetModel();
    rows_ = std::move(rows);
    endResetModel();
}

void FolderTreeModel::buildByYear(QList<Row>& out) const
{
    QSet<RootId> unavailable;
    for (const auto& r : roots_)
        if (r.state == RootState::Unavailable)
            unavailable.insert(r.id);
    int currentYear = -1;
    for (const auto& f : folders_) {
        if (!sortByName_) {
            const int year = f.date ? QDateTime::fromMSecsSinceEpoch(*f.date, QTimeZone::UTC).date().year() : 0;
            if (year != currentYear) {
                currentYear = year;
                Row y;
                y.kind = QStringLiteral("year");
                y.name = year ? QString::number(year) : QStringLiteral("Sans date");
                out << y;
            }
        }
        Row r;
        r.kind = QStringLiteral("folder");
        r.folderId = f.id;
        r.name = f.name;
        r.path = f.path;
        r.count = f.itemCount;
        r.dateText = formatDate(f.date);
        r.unavailable = unavailable.contains(f.rootId);
        out << r;
    }
}

void FolderTreeModel::buildTree(QList<Row>& out) const
{
    struct Node {
        QString path;
        QString name;
        const Folder* folder = nullptr;
        QMap<QString, QString> children; // nom minuscule -> chemin
        bool unavailable = false;
    };
    QHash<QString, Node> nodes; // clé : chemin en minuscules
    QList<QString> tops;

    // Racines effectives (une racine contenue dans une autre n'est pas une tête d'arbre).
    QList<WatchedRoot> roots = roots_;
    std::sort(roots.begin(), roots.end(), [](const WatchedRoot& a, const WatchedRoot& b) { return a.path.size() < b.path.size(); });
    QList<WatchedRoot> topRoots;
    for (const auto& r : roots) {
        const bool covered = std::any_of(topRoots.begin(), topRoots.end(), [&](const WatchedRoot& t) {
            return platform::isInside(r.path, t.path);
        });
        if (!covered)
            topRoots << r;
    }
    for (const auto& r : topRoots) {
        const QString key = r.path.toLower();
        Node& n = nodes[key];
        n.path = r.path;
        n.name = r.path;
        n.unavailable = r.state == RootState::Unavailable;
        tops << key;
    }
    std::sort(tops.begin(), tops.end());

    for (const auto& f : folders_) {
        const auto top = std::find_if(topRoots.begin(), topRoots.end(),
                                      [&](const WatchedRoot& t) { return platform::isInside(f.path, t.path); });
        if (top == topRoots.end())
            continue;
        QString child = f.path;
        nodes[child.toLower()].path = f.path;
        nodes[child.toLower()].name = platform::samePath(f.path, top->path) ? top->path : f.name;
        nodes[child.toLower()].folder = &f;
        nodes[child.toLower()].unavailable = top->state == RootState::Unavailable;
        while (!platform::samePath(child, top->path)) {
            const QString parent = platform::parentPath(child);
            if (parent.isEmpty())
                break;
            Node& p = nodes[parent.toLower()];
            if (p.path.isEmpty()) {
                p.path = parent;
                p.name = platform::lastSegment(parent);
                p.unavailable = top->state == RootState::Unavailable;
            }
            p.children.insert(platform::lastSegment(child).toLower(), child.toLower());
            child = parent;
        }
    }

    std::function<void(const QString&, int)> emitNode = [&](const QString& key, int depth) {
        const Node& n = nodes[key];
        Row r;
        r.kind = n.folder ? QStringLiteral("folder") : QStringLiteral("node");
        r.folderId = n.folder ? n.folder->id : 0;
        r.name = n.name;
        r.path = n.path;
        r.count = n.folder ? n.folder->itemCount : 0;
        r.dateText = n.folder ? formatDate(n.folder->date) : QString();
        r.depth = depth;
        r.expandable = !n.children.isEmpty();
        r.unavailable = n.unavailable;
        out << r;
        if (!collapsed_.contains(key)) {
            for (const QString& c : n.children)
                emitNode(c, depth + 1);
        }
    };
    for (const QString& t : tops) {
        if (nodes[t].folder || !nodes[t].children.isEmpty())
            emitNode(t, 0);
    }
}

int FolderTreeModel::rowOfFolder(qint64 folderId)
{
    auto find = [&] {
        for (qsizetype i = 0; i < rows_.size(); ++i)
            if (rows_[i].folderId == folderId && rows_[i].kind == u"folder")
                return int(i);
        return -1;
    };
    int row = find();
    if (row >= 0 || mode_ != u"tree")
        return row;
    // Déplier les ancêtres du dossier.
    const auto f = std::find_if(folders_.begin(), folders_.end(), [&](const Folder& x) { return x.id == folderId; });
    if (f == folders_.end())
        return -1;
    for (QString p = platform::parentPath(f->path); !p.isEmpty(); p = platform::parentPath(p))
        collapsed_.remove(p.toLower());
    rebuild();
    return find();
}

void FolderTreeModel::toggle(int row)
{
    if (row < 0 || row >= rows_.size() || !rows_[row].expandable)
        return;
    const QString key = rows_[row].path.toLower();
    if (collapsed_.contains(key))
        collapsed_.remove(key);
    else
        collapsed_.insert(key);
    rebuild();
}

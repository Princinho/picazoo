#pragma once

// Liste des dossiers à gauche (T041, T072) : « Par année » (comme Picasa) ou
// « Arborescence » (dossiers tels que rangés sur le disque, à partir des racines).

#include "library/Types.h"

#include <QAbstractListModel>
#include <QSet>

namespace picazoo {
class Library;
}

class FolderTreeModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY modeChanged) // "by_year" | "tree"

public:
    enum Roles {
        RowKindRole = Qt::UserRole + 1, // "year" | "folder" | "node" (dossier intermédiaire sans élément)
        FolderIdRole,
        NameRole,
        PathRole,
        CountRole,
        DateTextRole,
        DepthRole,
        ExpandableRole,
        ExpandedRole,
        UnavailableRole,
    };

    explicit FolderTreeModel(picazoo::Library& library, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString mode() const { return mode_; }
    void setMode(const QString& mode);

    Q_INVOKABLE void reload();
    Q_INVOKABLE int rowOfFolder(qint64 folderId);   // déplie l'arborescence si nécessaire
    Q_INVOKABLE void toggle(int row);

    struct Row {
        QString kind;
        picazoo::FolderId folderId = 0;
        QString name;
        QString path;
        int count = 0;
        QString dateText;
        int depth = 0;
        bool expandable = false;
        bool unavailable = false;
    };

signals:
    void modeChanged();

private:
    void rebuild();
    void buildByYear(QList<Row>& out) const;
    void buildTree(QList<Row>& out) const;

    picazoo::Library& lib_;
    QString mode_;
    QList<picazoo::Folder> folders_;
    QList<picazoo::WatchedRoot> roots_;
    QList<Row> rows_;
    QSet<QString> collapsed_; // chemins repliés (minuscules) ; tout est déplié par défaut
    bool sortByName_ = false;
};

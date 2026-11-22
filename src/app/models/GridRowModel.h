#pragma once

// Modèle de la grille continue (T042) : lignes d'en-tête de dossier et rangées de
// vignettes, calculées par picazoo::GridLayout. Les mises à jour pendant l'analyse
// sont appliquées par blocs (insertion / suppression / modification) pour ne pas
// recréer les délégués visibles.

#include "index/LibraryDb.h"
#include "library/GridLayout.h"

#include <QAbstractListModel>
#include <QHash>
#include <QSet>
#include <QTimer>

namespace picazoo {
class Library;
}

class GridRowModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(double viewWidth READ viewWidth WRITE setViewWidth NOTIFY layoutParamsChanged)
    Q_PROPERTY(int thumbSize READ thumbSize WRITE setThumbSize NOTIFY layoutParamsChanged)
    Q_PROPERTY(int spacing READ spacing CONSTANT)
    Q_PROPERTY(int columns READ columns NOTIFY layoutParamsChanged)
    Q_PROPERTY(int itemCount READ itemCount NOTIFY countsChanged)
    Q_PROPERTY(int folderCount READ folderCount NOTIFY countsChanged)

public:
    enum Roles {
        RowTypeRole = Qt::UserRole + 1, // "header" | "items"
        FolderIdRole,
        FolderNameRole,
        FolderPathRole,
        FolderDateRole,
        FolderCountRole,
        RowItemsRole, // liste de { id, version, kind, state, duration, name }
        RowHeightRole,
        FirstIndexRole,
        RowYRole, // position de la ligne dans la mise en page (sélection au cadre)
    };

    explicit GridRowModel(picazoo::Library& library, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    double viewWidth() const { return width_; }
    void setViewWidth(double w);
    int thumbSize() const { return thumbSize_; }
    void setThumbSize(int s);
    int spacing() const { return 8; }
    int columns() const { return layout_.columns(); }
    int itemCount() const { return int(flat_.size()); }
    int folderCount() const { return int(folders_.size()); }

    Q_INVOKABLE void reload();
    Q_INVOKABLE double yOfFolder(qint64 folderId) const;
    Q_INVOKABLE int headerRowOfFolder(qint64 folderId) const;
    Q_INVOKABLE int rowOfItem(qint64 itemId) const;
    Q_INVOKABLE double yOfItem(qint64 itemId) const;
    Q_INVOKABLE double totalHeight() const { return layout_.totalHeight(); }
    // Signale les lignes visibles : leurs vignettes puis celles des lignes proches
    // (±2 écrans) passent en tête de file.
    Q_INVOKABLE void reportRows(int firstRow, int lastRow);

    // Ordre de navigation (bibliothèque et visionneuse).
    Q_INVOKABLE int flatIndexOf(qint64 itemId) const;
    Q_INVOKABLE qint64 itemAtFlat(int flatIndex) const;
    Q_INVOKABLE qint64 neighbor(qint64 itemId, int dx, int dy) const;
    Q_INVOKABLE qint64 folderOfItem(qint64 itemId) const;
    Q_INVOKABLE QVariantList itemsOfFolder(qint64 folderId) const;
    Q_INVOKABLE qint64 firstItemOfFolder(qint64 folderId) const;
    Q_INVOKABLE QString kindOf(qint64 itemId) const;
    // Éléments dont la vignette coupe le rectangle (coordonnées du contenu de la grille).
    Q_INVOKABLE QVariantList itemsInRect(double x0, double y0, double x1, double y1) const;

    const QList<picazoo::Folder>& folders() const { return folders_; }
    const QList<picazoo::ItemId>& flatOrder() const { return flat_; }

signals:
    void layoutParamsChanged();
    void countsChanged();
    void structureChanged();

private:
    void onFoldersChanged();
    void onItemsChanged(qint64 folderId);
    void onItemUpdated(qint64 itemId);
    void scheduleRefresh();
    void refresh(bool force);
    void rebuildFlat();
    QVariantMap cell(const picazoo::ItemSummary& s) const;
    picazoo::FolderSort folderSort() const;
    picazoo::ItemSort itemSort() const;
    picazoo::GridParams params() const;

    picazoo::Library& lib_;
    double width_ = 800;
    int thumbSize_ = 160;
    QList<picazoo::Folder> folders_;
    QHash<picazoo::FolderId, qsizetype> folderIndex_;
    QHash<picazoo::FolderId, QList<picazoo::ItemSummary>> items_;
    picazoo::GridLayout layout_;
    QList<picazoo::GridRow> rows_; // lignes exposées (peut être transitoirement mixte pendant une mise à jour)

    QList<picazoo::ItemId> flat_;
    QHash<picazoo::ItemId, QPair<picazoo::FolderId, int>> where_; // élément -> (dossier, indice)

    QSet<picazoo::FolderId> dirtyFolders_;
    bool foldersDirty_ = false;
    QTimer refreshTimer_;
};

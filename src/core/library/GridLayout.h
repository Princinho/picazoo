#pragma once

// Découpage de la grille continue en lignes (research R9) : pour chaque dossier,
// une ligne d'en-tête puis des rangées de N vignettes. Hauteurs connues à l'avance,
// donc recherche d'une ligne par position en O(log n).

#include "library/Types.h"

#include <QHash>
#include <QList>
#include <QPair>

namespace picazoo {

struct GridRow {
    enum class Type { Header, Items };
    Type type = Type::Header;
    FolderId folder = 0;
    int first = 0; // premier indice (dans le dossier) des éléments de la rangée
    int count = 0; // nombre d'éléments de la rangée
    double y = 0;
    double height = 0;
};

struct GridParams {
    double width = 800;
    int thumbSize = 160;
    int spacing = 8;
    int headerHeight = 44;
    int margin = 12;
};

class GridLayout {
public:
    // `blocks` : dossiers dans l'ordre d'affichage, avec leur nombre d'éléments.
    void build(const QList<QPair<FolderId, int>>& blocks, const GridParams& params);

    static int columnsFor(const GridParams& params);
    int columns() const { return columns_; }
    const QList<GridRow>& rows() const { return rows_; }
    double totalHeight() const;
    const GridParams& params() const { return params_; }

    // Ligne qui contient la position `y` (bornée à [0, rows-1]) ; -1 si la grille est vide.
    int rowAtY(double y) const;
    int headerRow(FolderId folder) const;
    double yOfFolder(FolderId folder) const;
    // Ligne qui affiche l'élément `indexInFolder` du dossier.
    int rowOfItem(FolderId folder, int indexInFolder) const;
    // Première ligne et nombre de lignes (en-tête compris) d'un dossier.
    QPair<int, int> block(FolderId folder) const;

private:
    GridParams params_;
    int columns_ = 1;
    QList<GridRow> rows_;
    QHash<FolderId, QPair<int, int>> blocks_;
};

} // namespace picazoo

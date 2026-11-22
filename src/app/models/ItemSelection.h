#pragma once

// Sélection dans la grille (T043, T074) : élément courant + sélection multiple
// (clic, Ctrl+clic, Maj+clic, cadre, Ctrl+A).

#include <QObject>
#include <QSet>
#include <QVariantList>

class GridRowModel;

class ItemSelection : public QObject {
    Q_OBJECT
    Q_PROPERTY(qint64 current READ current WRITE setCurrent NOTIFY currentChanged)
    Q_PROPERTY(int count READ count NOTIFY selectionChanged)
    Q_PROPERTY(int revision READ revision NOTIFY selectionChanged)

public:
    explicit ItemSelection(GridRowModel& grid, QObject* parent = nullptr);

    qint64 current() const { return current_; }
    void setCurrent(qint64 id);
    int count() const { return int(selected_.size()); }
    int revision() const { return revision_; }

    Q_INVOKABLE bool isSelected(qint64 id) const { return selected_.contains(id); }
    // modifiers : 0 = simple, 1 = Ctrl, 2 = Maj
    Q_INVOKABLE void click(qint64 id, int modifiers);
    Q_INVOKABLE void selectRange(qint64 fromId, qint64 toId, bool add);
    Q_INVOKABLE void selectFolder(qint64 folderId);
    Q_INVOKABLE void clear();
    // Sélection au cadre : remplace (ou complète, avec Ctrl) la sélection.
    Q_INVOKABLE void setSelection(const QVariantList& ids, bool add);
    // Flèches : déplace l'élément courant (et la sélection) ; Maj étend.
    Q_INVOKABLE qint64 move(int dx, int dy, bool extend);
    Q_INVOKABLE QVariantList selectedIds() const;

signals:
    void currentChanged();
    void selectionChanged();

private:
    void touch();

    GridRowModel& grid_;
    qint64 current_ = 0;
    qint64 anchor_ = 0;
    QSet<qint64> selected_;
    int revision_ = 0;
};

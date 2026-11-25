#pragma once

// Visionneuse (T052) : élément courant, précédent / suivant à travers les dossiers,
// préchargement ±1 et ±2 à la taille de l'écran.

#include "library/Navigation.h"

#include <QObject>
#include <QPointF>
#include <QSize>
#include <QVariantMap>

namespace picazoo {
class Library;
}
class GridRowModel;
class ViewerImageProvider;

class ViewerController : public QObject {
    Q_OBJECT
    Q_PROPERTY(qint64 current READ current NOTIFY currentChanged)
    Q_PROPERTY(QVariantMap info READ info NOTIFY currentChanged)
    Q_PROPERTY(QString position READ position NOTIFY currentChanged)
    Q_PROPERTY(QString folderLabel READ folderLabel NOTIFY currentChanged) // « Bali · 9 sur 86 »
    Q_PROPERTY(QVariantList strip READ strip NOTIFY currentChanged)        // voisins, ±5
    Q_PROPERTY(bool hasPrevious READ hasPrevious NOTIFY currentChanged)
    Q_PROPERTY(bool hasNext READ hasNext NOTIFY currentChanged)
    Q_PROPERTY(QSize screenSize READ screenSize WRITE setScreenSize NOTIFY screenSizeChanged)
    Q_PROPERTY(bool animations READ animations CONSTANT) // « Afficher les animations dans Windows »

public:
    ViewerController(picazoo::Library& library, GridRowModel& grid, ViewerImageProvider& images,
                     QObject* parent = nullptr);

    qint64 current() const { return current_; }
    QVariantMap info() const { return info_; }
    QString position() const;
    QString folderLabel() const;
    QVariantList strip() const { return strip_; }
    bool hasPrevious() const;
    bool hasNext() const;
    QSize screenSize() const { return screen_; }
    void setScreenSize(QSize s);

    Q_INVOKABLE void open(qint64 itemId);
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void refresh();

    bool animations() const { return animations_; }

signals:
    void currentChanged();
    void screenSizeChanged();
    void emptied(); // 007 : plus aucun élément à afficher (tout a été mis à la corbeille)

private:
    void setCurrent(qint64 id);
    void prefetch();
    void buildStrip();

    picazoo::Library& lib_;
    GridRowModel& grid_;
    ViewerImageProvider& images_;
    picazoo::Navigation nav_;
    QList<picazoo::ItemId> order_; // ordre connu au dernier changement de la grille
    qint64 current_ = 0;
    QVariantMap info_;
    QVariantList strip_;
    QSize screen_{1920, 1080};
    bool animations_ = true;
};

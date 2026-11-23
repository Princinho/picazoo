import QtQuick
import QtQuick.Controls

// Bande des photos voisines (US6) : vignettes en mémoire seulement.
ListView {
    id: strip
    height: 60
    orientation: ListView.Horizontal
    spacing: 6
    interactive: false
    model: session.neighbors
    leftMargin: Math.max(0, (width - contentWidth) / 2)

    delegate: Item {
        id: tile
        required property var modelData
        width: 70
        height: 56
        Rectangle {
            anchors.fill: parent
            anchors.margins: 2
            radius: 6
            color: Theme.surface
            clip: true
            opacity: tile.modelData.current ? 1 : (tileHover.hovered ? 0.95 : 0.65)
            Image {
                anchors.fill: parent
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                cache: false
                source: tile.modelData.thumb
            }
        }
        Rectangle {
            visible: tile.modelData.current
            anchors.fill: parent
            anchors.margins: -1
            radius: 8
            color: "transparent"
            border.width: 2
            border.color: Theme.accent
        }
        HoverHandler { id: tileHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: session.goTo(tile.modelData.index) }
    }
}

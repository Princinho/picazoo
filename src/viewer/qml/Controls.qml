import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Barre de commandes de la visionneuse autonome (contracts/ui-contract.md).
Rectangle {
    id: bar
    property real zoomPercent: 100
    property bool fitActive: true
    property bool infoActive: false
    property bool veilActive: false
    property bool imageActions: true
    signal previous()
    signal next()
    signal zoomIn()
    signal zoomOut()
    signal fit()
    signal actualSize()
    signal toggleInfo()
    signal reveal()
    signal openInApp()
    signal toggleVeil()

    implicitWidth: row.implicitWidth + 24
    implicitHeight: 56
    radius: 28
    color: Theme.pill
    border.color: Theme.pillLine

    component Sep: Rectangle { width: 1; height: 24; color: Theme.pillLine; Layout.alignment: Qt.AlignVCenter }
    component TextPill: ToolButton {
        id: tp
        property bool active: false
        implicitHeight: 40
        leftPadding: 12
        rightPadding: 12
        focusPolicy: Qt.NoFocus
        contentItem: Label {
            text: tp.text
            color: tp.active ? Theme.accentText : Theme.text
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle { radius: 20; color: tp.active ? Theme.accentBg : (tp.hovered ? "#33FFFFFF" : "transparent") }
    }

    RowLayout {
        id: row
        anchors.centerIn: parent
        spacing: 4
        IconButton { glyph: Theme.iconPrev; Accessible.name: "Photo précédente (←)"; enabled: session.count > 1; onClicked: bar.previous() }
        IconButton { glyph: Theme.iconNext; Accessible.name: "Photo suivante (→)"; enabled: session.count > 1; onClicked: bar.next() }
        Sep {}
        IconButton { glyph: Theme.iconZoomOut; Accessible.name: "Zoom arrière (↓, Ctrl −)"; enabled: bar.imageActions; onClicked: bar.zoomOut() }
        Label {
            Layout.preferredWidth: 56
            horizontalAlignment: Text.AlignHCenter
            text: Math.round(bar.zoomPercent) + " %"
            color: Theme.textSoft
        }
        IconButton { glyph: Theme.iconZoomIn; Accessible.name: "Zoom avant (↑, Ctrl +)"; enabled: bar.imageActions; onClicked: bar.zoomIn() }
        TextPill { text: "Ajuster"; active: bar.fitActive; enabled: bar.imageActions; onClicked: bar.fit() }
        TextPill { text: "100 %"; enabled: bar.imageActions; onClicked: bar.actualSize() }
        Sep {}
        IconButton { glyph: Theme.iconInfo; active: bar.infoActive; Accessible.name: "Informations (I)"; onClicked: bar.toggleInfo() }
        IconButton { glyph: Theme.iconFolder; Accessible.name: "Afficher dans l'Explorateur (Ctrl+Maj+E)"; onClicked: bar.reveal() }
        IconButton { glyph: Theme.iconGrid; Accessible.name: "Ouvrir dans Picazoo"; onClicked: bar.openInApp() }
        IconButton {
            glyph: Theme.iconVeil
            active: bar.veilActive
            enabled: session.transparencyAvailable
            Accessible.name: "Voile opaque (F11)"
            onClicked: bar.toggleVeil()
        }
    }
}

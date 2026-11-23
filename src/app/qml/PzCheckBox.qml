import QtQuick
import QtQuick.Controls

// Case à cocher de l'« Atelier sombre » : décochée = cadre vide, cochée = accent avec coche
// (le style par défaut rendait une case décochée en carré plein, ambigu).
CheckBox {
    id: box
    spacing: 8
    indicator: Rectangle {
        implicitWidth: 18
        implicitHeight: 18
        x: box.leftPadding
        y: (box.height - height) / 2
        radius: 4
        color: box.checked ? (box.hovered ? Theme.accent : Theme.accentButton) : Theme.field
        border.color: box.checked ? "transparent" : (box.hovered ? Theme.accentText : "#5A6372")
        border.width: 1.5
        Label {
            anchors.centerIn: parent
            visible: box.checked
            text: "✓"
            color: "#FFFFFF"
            font.pixelSize: 13
            font.weight: Font.Bold
        }
    }
    contentItem: Label {
        leftPadding: box.indicator.width + (box.text !== "" ? box.spacing : 0)
        text: box.text
        color: box.enabled ? Theme.text : Theme.text3
        verticalAlignment: Text.AlignVCenter
    }
}

import QtQuick
import QtQuick.Controls

// Bouton texte arrondi ; `primary` pour l'action proposée.
Button {
    id: b
    property bool primary: false
    implicitHeight: 40
    leftPadding: 16
    rightPadding: 16
    focusPolicy: Qt.NoFocus
    contentItem: Label {
        text: b.text
        color: b.primary ? "#FFFFFF" : Theme.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        radius: 20
        color: b.primary ? (b.hovered ? Theme.accent : Theme.accentButton) : (b.hovered ? Theme.hover : Theme.surface)
        border.color: b.primary ? "transparent" : Theme.lineStrong
    }
}

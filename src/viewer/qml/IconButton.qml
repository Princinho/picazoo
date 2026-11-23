import QtQuick
import QtQuick.Controls

// Bouton rond à icône (police d'icônes de Windows), avec info-bulle.
ToolButton {
    id: b
    property string glyph
    property bool active: false
    implicitWidth: 44
    implicitHeight: 44
    focusPolicy: Qt.NoFocus
    ToolTip.visible: hovered && Accessible.name !== ""
    ToolTip.delay: 500
    ToolTip.text: Accessible.name
    contentItem: Label {
        text: b.glyph
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        font.family: Theme.iconFont
        font.pixelSize: 16
        color: !b.enabled ? Theme.text3 : (b.active ? Theme.accentText : Theme.text)
    }
    background: Rectangle {
        radius: 22
        color: b.active ? Theme.accentBg : (b.hovered ? "#33FFFFFF" : "transparent")
    }
}

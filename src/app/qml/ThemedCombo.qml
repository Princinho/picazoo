import QtQuick
import QtQuick.Controls

// Liste déroulante aux couleurs de l'interface ; modèle : [{ text, value }].
ComboBox {
    id: combo
    textRole: "text"
    valueRole: "value"
    implicitHeight: 34

    contentItem: Label {
        leftPadding: 12
        rightPadding: combo.indicator.width + 8
        text: combo.displayText
        color: Theme.text
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    indicator: Label {
        x: combo.width - width - 12
        y: (combo.height - height) / 2
        text: "▾"
        color: Theme.text2
    }
    background: Rectangle {
        radius: 8
        color: combo.hovered ? Theme.hover : Theme.surface
        border.color: combo.visualFocus ? Theme.accent : Theme.lineStrong
    }
    delegate: ItemDelegate {
        id: d
        required property var modelData
        required property int index
        width: combo.width
        highlighted: combo.highlightedIndex === index
        contentItem: Label {
            text: d.modelData.text
            color: d.highlighted ? Theme.text : Theme.textSoft
        }
        background: Rectangle { color: d.highlighted ? Theme.accentBg : Theme.surface }
    }
    popup.background: Rectangle {
        radius: 8
        color: Theme.surface
        border.color: Theme.lineStrong
    }
}

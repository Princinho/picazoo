import QtQuick
import QtQuick.Controls

// Champ de saisie aux couleurs de l'« Atelier sombre ».
TextField {
    id: field
    implicitHeight: 34
    leftPadding: 12
    rightPadding: 12
    color: Theme.text
    placeholderTextColor: Theme.text3
    selectionColor: Theme.accentButton
    selectedTextColor: "#FFFFFF"
    selectByMouse: true
    Accessible.name: placeholderText
    background: Rectangle {
        radius: 8
        color: Theme.field
        border.color: field.activeFocus ? Theme.accent : Theme.lineStrong
    }
}

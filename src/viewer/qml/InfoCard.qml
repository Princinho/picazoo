import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Informations de la photo (FR-024) ; les champs inconnus sont masqués.
Rectangle {
    id: card
    property var info: ({})
    signal reveal()
    width: 320
    implicitHeight: col.implicitHeight + 40
    radius: 14
    color: Theme.pill
    border.color: Theme.pillLine

    component Field: ColumnLayout {
        property string label
        property string value
        visible: value !== ""
        Layout.fillWidth: true
        spacing: 2
        Label { text: parent.label; color: Theme.text3; font.pixelSize: 12 }
        Label { Layout.fillWidth: true; text: parent.value; wrapMode: Text.WrapAnywhere; color: Theme.text }
    }

    ColumnLayout {
        id: col
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 20
        spacing: 12
        Label {
            text: "Informations"
            font.family: Theme.displayFont
            font.pixelSize: 17
            font.weight: Font.DemiBold
            color: Theme.text
        }
        Field { label: "Nom"; value: card.info.name || "" }
        Field { label: card.info.dateLabel || "Date"; value: card.info.date || "" }
        Field { label: "Dimensions"; value: card.info.dimensions || "" }
        Field { label: "Poids"; value: card.info.size || "" }
        Field { label: "Appareil"; value: card.info.camera || "" }
        Field { label: "Emplacement"; value: card.info.path || "" }
        PillButton { text: "Afficher dans l'Explorateur"; onClicked: card.reveal() }
    }
}

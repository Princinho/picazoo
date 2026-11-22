import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Panneau d'informations (FR-020) ; les champs inconnus sont masqués.
Rectangle {
    id: root
    property var info: ({})
    color: "#1a1a1a"

    component Field: ColumnLayout {
        property string label
        property string value
        visible: value !== undefined && value !== ""
        Layout.fillWidth: true
        spacing: 2
        Label {
            text: parent.label
            color: "#8a8a8a"
            font.pixelSize: 11
        }
        Label {
            Layout.fillWidth: true
            text: parent.value
            wrapMode: Text.WrapAnywhere
            color: "#e0e0e0"
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 14

        Label {
            text: "Informations"
            font.pixelSize: 16
            font.bold: true
        }
        Field { label: "Nom"; value: root.info.name || "" }
        Field { label: "Emplacement"; value: root.info.path || "" }
        Field { label: root.info.dateLabel || "Date"; value: root.info.date || "" }
        Field { label: "Dimensions"; value: root.info.dimensions || "" }
        Field { label: "Durée"; value: root.info.duration || "" }
        Field { label: "Poids"; value: root.info.size || "" }
        Field { label: "Appareil"; value: root.info.camera || "" }
        Item { Layout.fillHeight: true }
    }
}

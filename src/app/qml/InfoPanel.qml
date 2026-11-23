import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Panneau d'informations (FR-020) ; les champs inconnus sont masqués.
Rectangle {
    id: root
    property var info: ({})
    // Doublons (002) : nombre d'autres copies du même groupe.
    readonly property int duplicateCount: dups && root.info.id ? dups.duplicateCount(root.info.id) : 0
    color: Theme.viewerPanel

    Rectangle { anchors.left: parent.left; width: 1; height: parent.height; color: Theme.viewerLine }

    component Field: ColumnLayout {
        property string label
        property string value
        visible: value !== undefined && value !== ""
        Layout.fillWidth: true
        spacing: 3
        Label {
            text: parent.label
            color: Theme.text3
            font.pixelSize: 12
        }
        Label {
            Layout.fillWidth: true
            text: parent.value
            wrapMode: Text.WrapAnywhere
            color: Theme.text
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        Label {
            text: "Informations"
            font.family: Theme.displayFont
            font.pixelSize: 18
            font.weight: Font.DemiBold
            color: Theme.text
        }
        Field { label: root.info.dateLabel || "Date"; value: root.info.date || "" }
        Field { label: "Dimensions"; value: root.info.dimensions || "" }
        Field { label: "Durée"; value: root.info.duration || "" }
        Field { label: "Poids"; value: root.info.size || "" }
        Field { label: "Appareil"; value: root.info.camera || "" }
        Field { label: "Emplacement"; value: root.info.path || "" }
        RowLayout {
            visible: root.duplicateCount > 0
            Layout.fillWidth: true
            spacing: 10
            Label {
                Layout.fillWidth: true
                text: root.duplicateCount + (root.duplicateCount > 1 ? " doublons" : " doublon")
                color: Theme.amberText
            }
            Button {
                id: dupBtn
                focusPolicy: Qt.NoFocus
                implicitHeight: 32
                onClicked: dups.showGroup(dups.groupOf(root.info.id))
                contentItem: Label { text: "Voir"; color: Theme.text; verticalAlignment: Text.AlignVCenter }
                background: Rectangle {
                    radius: 8
                    color: dupBtn.hovered ? Theme.hover : "#1A1D23"
                    border.color: Theme.lineStrong
                }
            }
        }
        Button {
            id: revealBtn
            focusPolicy: Qt.NoFocus
            implicitHeight: 40
            leftPadding: 14
            rightPadding: 14
            onClicked: app.revealInExplorer(root.info.id)
            contentItem: Label { text: "Afficher dans l'Explorateur"; color: Theme.text; verticalAlignment: Text.AlignVCenter }
            background: Rectangle {
                radius: 10
                color: revealBtn.hovered ? Theme.hover : "#1A1D23"
                border.color: Theme.lineStrong
            }
        }
        Item { Layout.fillHeight: true }
    }
}

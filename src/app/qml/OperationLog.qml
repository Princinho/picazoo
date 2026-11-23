import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Journal des opérations sur les fichiers (FR-020) et annulation (FR-021).
Popup {
    id: root
    property var selectedId: 0
    property var details: ({})

    function refreshDetails() { details = selectedId ? trash.operationDetails(selectedId) : ({}) }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(900, parent ? parent.width - 64 : 900)
    height: Math.min(600, parent ? parent.height - 64 : 600)
    modal: true
    focus: true
    padding: 24
    closePolicy: Popup.CloseOnEscape
    onOpened: {
        const j = trash.journal
        selectedId = j.length > 0 ? j[0].id : 0
        refreshDetails()
    }
    background: Rectangle { radius: 14; color: Theme.panel; border.color: Theme.lineStrong }

    Connections {
        target: trash
        function onJournalChanged() { root.refreshDetails() }
        function onBusyChanged() { root.refreshDetails() }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Journal des opérations"
                font.family: Theme.displayFont
                font.pixelSize: 20
                font.weight: Font.DemiBold
                color: Theme.text
            }
            Item { Layout.fillWidth: true }
            Button { text: "Fermer"; onClicked: root.close() }
        }

        Label {
            visible: trash.journal.length === 0
            text: "Aucune opération pour l'instant. Picazoo n'a touché à aucun fichier."
            color: Theme.text2
        }

        RowLayout {
            visible: trash.journal.length > 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 16

            ListView {
                id: ops
                Layout.preferredWidth: 340
                Layout.fillHeight: true
                clip: true
                spacing: 4
                model: trash.journal
                ScrollBar.vertical: ScrollBar {}
                delegate: Rectangle {
                    required property var modelData
                    width: ListView.view.width
                    height: 62
                    radius: 8
                    color: modelData.id === root.selectedId ? Theme.accentBg : (opMouse.containsMouse ? Theme.hover : "transparent")
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 2
                        Label { Layout.fillWidth: true; text: modelData.title; color: Theme.text; elide: Text.ElideRight }
                        Label { Layout.fillWidth: true; text: modelData.date + " · " + modelData.state; color: Theme.text3; font.pixelSize: 12 }
                    }
                    MouseArea {
                        id: opMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: { root.selectedId = modelData.id; root.refreshDetails() }
                    }
                }
            }

            Rectangle { Layout.fillHeight: true; Layout.preferredWidth: 1; color: Theme.line }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 10
                Label { text: "État : " + (root.details.state || ""); color: Theme.text2 }
                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 6
                    model: root.details.entries || []
                    ScrollBar.vertical: ScrollBar {}
                    delegate: ColumnLayout {
                        required property var modelData
                        width: ListView.view.width
                        spacing: 1
                        Label { Layout.fillWidth: true; text: modelData.name + " · " + modelData.size; color: Theme.text; elide: Text.ElideMiddle }
                        Label { Layout.fillWidth: true; text: modelData.path; color: Theme.text3; font.pixelSize: 12; elide: Text.ElideMiddle }
                        Label {
                            Layout.fillWidth: true
                            text: modelData.resultText
                            color: modelData.result === "trashed" || modelData.result === "restored" ? Theme.okText : Theme.amberText
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }
                    }
                }
                Button {
                    id: undoBtn
                    readonly property var op: {
                        const j = trash.journal
                        for (let i = 0; i < j.length; ++i)
                            if (j[i].id === root.selectedId)
                                return j[i]
                        return null
                    }
                    enabled: op !== null && op.undoable && !trash.busy
                    text: "Annuler cette opération"
                    onClicked: trash.undo(root.selectedId)
                }
                // 006 (FR-019) : arrêt entre deux fichiers ; ce qui est fait reste annulable.
                Button {
                    visible: trash.busy
                    text: "■  Arrêter l'opération en cours (" + trash.progressText + ")"
                    onClicked: trash.stop()
                }
            }
        }
    }
}

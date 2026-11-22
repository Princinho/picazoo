import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// Dossiers surveillés (FR-002) : ajouter, retirer, exclure un sous-dossier.
Dialog {
    id: root
    title: "Dossiers surveillés"
    modal: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(Overlay.overlay ? Overlay.overlay.width - 80 : 800, 760)
    height: Math.min(Overlay.overlay ? Overlay.overlay.height - 80 : 600, 560)
    standardButtons: Dialog.Close

    property var pendingRoot: ({})

    function stateText(state) {
        if (state === "denied")
            return "Accès refusé — ce dossier est ignoré."
        if (state === "unavailable")
            return "Disque ou dossier introuvable. Il sera de nouveau pris en compte dès qu'il sera accessible."
        return ""
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: "#a0a0a0"
            text: "Picazoo affiche les photos et vidéos de ces dossiers et de leurs sous-dossiers. "
                  + "Retirer un dossier ne supprime aucun fichier de votre ordinateur."
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 6
            model: app.roots
            ScrollBar.vertical: ScrollBar {}

            delegate: Rectangle {
                id: rootRow
                required property var modelData
                width: ListView.view.width
                height: content.implicitHeight + 16
                color: "#262626"
                radius: 4

                ColumnLayout {
                    id: content
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 4

                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            text: rootRow.modelData.path
                            elide: Text.ElideMiddle
                            font.bold: true
                            color: rootRow.modelData.state === "available" ? "#e6e6e6" : "#8a8a8a"
                        }
                        Button {
                            text: "Exclure un sous-dossier…"
                            onClicked: {
                                root.pendingRoot = rootRow.modelData
                                excludePicker.currentFolder = "file:///" + rootRow.modelData.path.replace(/\\/g, "/")
                                excludePicker.open()
                            }
                        }
                        Button {
                            text: "Retirer"
                            onClicked: {
                                root.pendingRoot = rootRow.modelData
                                confirmRemove.open()
                            }
                        }
                    }
                    Label {
                        visible: text !== ""
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: root.stateText(rootRow.modelData.state)
                        color: "#e0a040"
                        font.pixelSize: 12
                    }
                    Repeater {
                        model: rootRow.modelData.excluded
                        delegate: RowLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.leftMargin: 16
                            Label {
                                Layout.fillWidth: true
                                text: "Exclu : " + modelData
                                elide: Text.ElideMiddle
                                color: "#a0a0a0"
                                font.pixelSize: 12
                            }
                            ToolButton {
                                text: "Inclure de nouveau"
                                font.pixelSize: 12
                                onClicked: app.includeFolder(rootRow.modelData.id, modelData)
                            }
                        }
                    }
                }
            }
        }

        Button {
            text: "Ajouter un dossier…"
            onClicked: addPicker.open()
        }
    }

    FolderDialog {
        id: addPicker
        title: "Ajouter un dossier à surveiller"
        onAccepted: app.addRoot(selectedFolder)
    }

    FolderDialog {
        id: excludePicker
        title: "Choisir le sous-dossier à exclure"
        onAccepted: app.excludeFolder(root.pendingRoot.id, selectedFolder)
    }

    Dialog {
        id: confirmRemove
        anchors.centerIn: parent
        modal: true
        title: "Retirer un dossier"
        standardButtons: Dialog.Ok | Dialog.Cancel
        Label {
            width: 420
            wrapMode: Text.WordWrap
            text: "Retirer « " + (root.pendingRoot.path || "") + " » de Picazoo ? Ses photos ne seront plus affichées. "
                  + "Aucun fichier ne sera supprimé de votre ordinateur."
        }
        onAccepted: app.removeRoot(root.pendingRoot.id)
    }
}

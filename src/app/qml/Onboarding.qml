import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// Premier lancement (FR-001) : « Où sont vos photos ? »
Rectangle {
    id: root
    color: "#1e1e1e"
    property var chosen: []

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 64, 560)
        spacing: 18

        Label {
            text: "Où sont vos photos ?"
            font.pixelSize: 28
            font.bold: true
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: "#b0b0b0"
            text: "Picazoo trouve tout seul les photos et les vidéos des dossiers que vous lui indiquez. "
                  + "Il ne modifie, ne déplace et ne supprime jamais vos fichiers."
        }

        ButtonGroup { id: choice }

        RadioButton {
            id: useDefaults
            checked: true
            ButtonGroup.group: choice
            text: "Images, Bureau et Documents (recommandé)"
        }
        Label {
            visible: useDefaults.checked
            Layout.leftMargin: 32
            Layout.fillWidth: true
            text: onboarding.defaultFolders.join("\n")
            color: "#8a8a8a"
            font.pixelSize: 12
            elide: Text.ElideMiddle
        }

        RadioButton {
            id: custom
            ButtonGroup.group: choice
            text: "Choisir des dossiers…"
        }
        ColumnLayout {
            visible: custom.checked
            Layout.leftMargin: 32
            Layout.fillWidth: true
            Repeater {
                model: root.chosen
                delegate: RowLayout {
                    required property var modelData
                    required property int index
                    Layout.fillWidth: true
                    Label {
                        Layout.fillWidth: true
                        text: app.urlToPath(modelData)
                        elide: Text.ElideMiddle
                    }
                    ToolButton {
                        text: "Retirer"
                        onClicked: {
                            const c = root.chosen.slice()
                            c.splice(index, 1)
                            root.chosen = c
                        }
                    }
                }
            }
            Button {
                text: "Ajouter un dossier…"
                onClicked: picker.open()
            }
        }

        Button {
            Layout.alignment: Qt.AlignRight
            text: "Commencer"
            highlighted: true
            enabled: useDefaults.checked || root.chosen.length > 0
            onClicked: onboarding.start(useDefaults.checked, root.chosen)
        }
    }

    FolderDialog {
        id: picker
        title: "Choisir un dossier de photos"
        onAccepted: {
            const c = root.chosen.slice()
            c.push(selectedFolder)
            root.chosen = c
        }
    }
}

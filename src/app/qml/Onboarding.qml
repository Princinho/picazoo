import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// Premier lancement (FR-001) : « Où sont vos photos ? »
Rectangle {
    id: root
    color: Theme.bg
    property var chosen: []
    property bool useDefaults: true

    component ChoiceCard: Rectangle {
        id: card
        property bool selected: false
        default property alias content: inner.data
        signal picked()
        Layout.fillWidth: true
        implicitHeight: inner.implicitHeight + 36
        radius: 14
        color: selected ? "#16202C" : Theme.panel
        border.width: selected ? 2 : 1
        border.color: selected ? Theme.accentButton : Theme.lineStrong
        RowLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 14
            RadioButton {
                id: radio
                Layout.alignment: Qt.AlignTop
                checked: card.selected
                onClicked: card.picked()
                Accessible.name: "Choisir cette option"
                indicator: Rectangle {
                    implicitWidth: 20
                    implicitHeight: 20
                    x: radio.leftPadding
                    y: radio.topPadding + (radio.availableHeight - height) / 2
                    radius: 10
                    color: "transparent"
                    border.width: 2
                    border.color: radio.checked ? Theme.accent : Theme.text3
                    Rectangle {
                        anchors.centerIn: parent
                        width: 10
                        height: 10
                        radius: 5
                        color: Theme.accent
                        visible: radio.checked
                    }
                }
            }
            ColumnLayout {
                id: inner
                Layout.fillWidth: true
                spacing: 6
            }
        }
        MouseArea {
            anchors.fill: parent
            z: -1
            cursorShape: Qt.PointingHandCursor
            onClicked: card.picked()
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 620)
        spacing: 28

        ColumnLayout {
            spacing: 18
            Image {
                source: "qrc:/brand/picazoo.svg"
                sourceSize: Qt.size(72, 72)
                Layout.preferredWidth: 72
                Layout.preferredHeight: 72
            }
            Label {
                text: "Où sont vos photos ?"
                font.family: Theme.displayFont
                font.pixelSize: 40
                font.weight: Font.DemiBold
                color: Theme.text
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                lineHeight: 1.25
                color: "#B8C0CB"
                font.pixelSize: 15
                text: "Picazoo trouve tout seul les photos et les vidéos des dossiers que vous lui indiquez, "
                      + "et les tient à jour. Il ne modifie, ne déplace et ne supprime jamais vos fichiers."
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 12

            ChoiceCard {
                selected: root.useDefaults
                onPicked: root.useDefaults = true
                RowLayout {
                    spacing: 8
                    Label { text: "Images, Bureau et Documents"; font.weight: Font.DemiBold; color: Theme.text; font.pixelSize: 15 }
                    Rectangle {
                        implicitWidth: rec.implicitWidth + 16
                        implicitHeight: rec.implicitHeight + 4
                        radius: height / 2
                        color: Theme.accentBg
                        Label { id: rec; anchors.centerIn: parent; text: "Recommandé"; color: Theme.accentText; font.pixelSize: 12 }
                    }
                }
                Label {
                    Layout.fillWidth: true
                    text: onboarding.defaultFolders.join("\n")
                    color: Theme.text2
                    font.pixelSize: 13
                    elide: Text.ElideMiddle
                }
            }

            ChoiceCard {
                selected: !root.useDefaults
                onPicked: root.useDefaults = false
                Label { text: "Choisir des dossiers…"; font.weight: Font.DemiBold; color: Theme.text; font.pixelSize: 15 }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: "Un disque externe, un dossier de sauvegarde, plusieurs emplacements…"
                    color: Theme.text2
                    font.pixelSize: 13
                }
                Repeater {
                    model: root.useDefaults ? [] : root.chosen
                    delegate: RowLayout {
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            text: app.urlToPath(modelData)
                            elide: Text.ElideMiddle
                            color: Theme.text
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
                    visible: !root.useDefaults
                    text: "Ajouter un dossier…"
                    onClicked: picker.open()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 16
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.text2
                font.pixelSize: 13
                text: "Vous pourrez ajouter ou retirer des dossiers à tout moment, depuis Outils › Dossiers surveillés."
            }
            Button {
                id: start
                enabled: root.useDefaults || root.chosen.length > 0
                implicitHeight: 48
                leftPadding: 28
                rightPadding: 28
                onClicked: onboarding.start(root.useDefaults, root.chosen)
                contentItem: Label {
                    text: "Commencer"
                    color: "#FFFFFF"
                    font.weight: Font.DemiBold
                    font.pixelSize: 15
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 12
                    color: !start.enabled ? Theme.lineStrong : (start.hovered ? "#3A88D6" : Theme.accentButton)
                }
            }
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

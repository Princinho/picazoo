import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// Dossiers surveillés (FR-002) : ajouter, retirer, exclure un sous-dossier.
Popup {
    id: root
    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(Overlay.overlay ? Overlay.overlay.width - 48 : 760, 760)
    height: Math.min(Overlay.overlay ? Overlay.overlay.height - 48 : 640, content.implicitHeight + 56)
    padding: 28
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    property var pendingRoot: ({})
    property var roots: []

    onOpened: roots = app.roots
    Connections {
        target: app
        function onRootsChanged() { if (root.opened) root.roots = app.roots }
    }

    Overlay.modal: Rectangle { color: "#B3000000" }
    background: Rectangle {
        radius: 18
        color: Theme.panel
        border.color: Theme.line
    }

    function stateText(r) {
        if (r.state === "denied")
            return "Accès refusé — ce dossier est ignoré."
        if (r.state === "unavailable")
            return "Disque ou dossier introuvable. Il sera de nouveau pris en compte dès qu'il sera accessible ; "
                   + "ses " + r.count.toLocaleString(Qt.locale("fr_FR"), "f", 0) + " éléments restent dans la bibliothèque."
        return ""
    }
    function badgeText(r) {
        if (r.state === "denied") return "Accès refusé"
        if (r.state === "unavailable") return "Introuvable"
        return "Disponible · " + r.count.toLocaleString(Qt.locale("fr_FR"), "f", 0)
               + (r.count > 1 ? " éléments" : " élément")
    }

    component SmallButton: Button {
        id: sb
        property color textColor: Theme.text
        implicitHeight: 36
        leftPadding: 12
        rightPadding: 12
        contentItem: Label { text: sb.text; color: sb.textColor; verticalAlignment: Text.AlignVCenter }
        background: Rectangle {
            radius: 8
            color: sb.hovered ? Theme.hover : Theme.panel
            border.color: Theme.lineStrong
        }
    }

    contentItem: ColumnLayout {
        id: content
        spacing: 20

        ColumnLayout {
            spacing: 6
            Label {
                text: "Dossiers surveillés"
                font.family: Theme.displayFont
                font.pixelSize: 24
                font.weight: Font.DemiBold
                color: Theme.text
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: "#B8C0CB"
                text: "Picazoo affiche les photos et les vidéos de ces dossiers et de leurs sous-dossiers. "
                      + "Retirer un dossier ne supprime aucun fichier de votre ordinateur."
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: contentHeight
            Layout.maximumHeight: 420
            clip: true
            spacing: 10
            model: root.roots
            ScrollBar.vertical: ScrollBar {}

            delegate: Rectangle {
                id: rootRow
                required property var modelData
                width: ListView.view.width
                height: rowContent.implicitHeight + 28
                radius: 12
                color: Theme.surface
                border.color: Theme.line

                ColumnLayout {
                    id: rowContent
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 14
                        Label { text: Theme.iconFolder; font.family: Theme.iconFont; font.pixelSize: 16; color: Theme.text2 }
                        Label {
                            Layout.fillWidth: true
                            text: rootRow.modelData.path
                            elide: Text.ElideMiddle
                            font.weight: Font.DemiBold
                            color: rootRow.modelData.state === "available" ? Theme.text : Theme.text2
                        }
                        Rectangle {
                            implicitWidth: badge.implicitWidth + 20
                            implicitHeight: badge.implicitHeight + 6
                            radius: height / 2
                            color: rootRow.modelData.state === "denied" ? Theme.errBg
                                 : rootRow.modelData.state === "unavailable" ? Theme.amberBg : Theme.okBg
                            Label {
                                id: badge
                                anchors.centerIn: parent
                                text: root.badgeText(rootRow.modelData)
                                font.pixelSize: 12
                                color: rootRow.modelData.state === "denied" ? Theme.errText
                                     : rootRow.modelData.state === "unavailable" ? Theme.amberText : Theme.okText
                            }
                        }
                    }
                    Label {
                        visible: text !== ""
                        Layout.fillWidth: true
                        Layout.leftMargin: 30
                        wrapMode: Text.WordWrap
                        text: root.stateText(rootRow.modelData)
                        color: Theme.amberText
                        font.pixelSize: 13
                    }
                    Repeater {
                        model: rootRow.modelData.excluded
                        delegate: RowLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.leftMargin: 30
                            Label {
                                Layout.fillWidth: true
                                text: "Exclu : " + modelData
                                elide: Text.ElideMiddle
                                color: Theme.text2
                                font.pixelSize: 13
                            }
                            ToolButton {
                                contentItem: Label { text: "Inclure de nouveau"; color: Theme.accentText; font.pixelSize: 13 }
                                onClicked: app.includeFolder(rootRow.modelData.id, modelData)
                            }
                        }
                    }
                    RowLayout {
                        Layout.leftMargin: 30
                        spacing: 8
                        SmallButton {
                            text: "Exclure un sous-dossier…"
                            onClicked: {
                                root.pendingRoot = rootRow.modelData
                                excludePicker.currentFolder = "file:///" + rootRow.modelData.path.replace(/\\/g, "/")
                                excludePicker.open()
                            }
                        }
                        SmallButton {
                            text: "Retirer"
                            textColor: Theme.errText
                            onClicked: {
                                root.pendingRoot = rootRow.modelData
                                confirmRemove.open()
                            }
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            SmallButton {
                implicitHeight: 44
                text: "+  Ajouter un dossier…"
                onClicked: addPicker.open()
            }
            Item { Layout.fillWidth: true }
            Button {
                id: closeBtn
                implicitHeight: 44
                leftPadding: 22
                rightPadding: 22
                onClicked: root.close()
                contentItem: Label {
                    text: "Fermer"
                    color: "#FFFFFF"
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle { radius: 10; color: closeBtn.hovered ? "#3A88D6" : Theme.accentButton }
            }
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
        parent: Overlay.overlay
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

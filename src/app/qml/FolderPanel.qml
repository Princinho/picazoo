import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// Panneau « Dossiers » (006, US2) : paires de dossiers qui partagent des copies, doublons
// internes, règles de dossier. Il oriente les choix et les suggestions ; il ne touche jamais aux
// fichiers (la mise à la corbeille passe toujours par le récapitulatif).
Rectangle {
    id: panel
    signal close()

    color: Theme.sidebar
    Rectangle { anchors.left: parent.left; width: 1; height: parent.height; color: Theme.line }

    component SectionTitle: Label {
        color: Theme.text3
        font.pixelSize: 11
        font.letterSpacing: 0.6
        font.capitalization: Font.AllUppercase
        topPadding: 10
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 1
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 16
            Label {
                Layout.fillWidth: true
                text: "Dossiers"
                color: Theme.text
                font.pixelSize: 17
                font.weight: Font.DemiBold
            }
            ToolButton {
                focusPolicy: Qt.NoFocus
                Accessible.name: "Fermer le panneau Dossiers"
                contentItem: Label { text: "×"; color: Theme.text2; font.pixelSize: 16; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { radius: 8; color: parent.hovered ? Theme.hover : "transparent" }
                onClicked: panel.close()
            }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.line }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                width: parent.width
                spacing: 8

                Item { height: 4 }
                SectionTitle { Layout.leftMargin: 16; text: "Paires de dossiers" }
                Label {
                    visible: dups.pairs.length === 0
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: "Aucune paire de dossiers ne partage de copies."
                    color: Theme.text2
                }
                Repeater {
                    model: dups.pairs
                    delegate: Rectangle {
                        id: pairCard
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.leftMargin: 12
                        Layout.rightMargin: 12
                        implicitHeight: pairCol.implicitHeight + 24
                        radius: 10
                        color: modelData.active ? "#1B2330" : (pairMouse.containsMouse ? Theme.hover : Theme.panel)
                        border.color: modelData.active ? Theme.accentButton : Theme.line

                        MouseArea {
                            id: pairMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: dups.setPairFilter(pairCard.modelData.a, pairCard.modelData.b)
                            ToolTip.visible: containsMouse
                            ToolTip.delay: 800
                            ToolTip.text: pairCard.modelData.a + "\n↔ " + pairCard.modelData.b
                        }

                        ColumnLayout {
                            id: pairCol
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 12
                            spacing: 4
                            Label { Layout.fillWidth: true; text: pairCard.modelData.aLabel; color: Theme.text; font.weight: Font.DemiBold; elide: Text.ElideLeft }
                            Label { Layout.fillWidth: true; text: "↔  " + pairCard.modelData.bLabel; color: Theme.textSoft; elide: Text.ElideLeft }
                            Label {
                                Layout.fillWidth: true
                                text: pairCard.modelData.stats + (pairCard.modelData.active ? " · filtre actif" : "")
                                color: pairCard.modelData.active ? Theme.accentText : Theme.text2
                                font.pixelSize: 12
                            }
                            RowLayout {
                                visible: pairCard.modelData.active
                                Layout.fillWidth: true
                                Layout.topMargin: 6
                                spacing: 0
                                Button {
                                    id: keepA
                                    Layout.fillWidth: true
                                    implicitHeight: 38
                                    focusPolicy: Qt.NoFocus
                                    contentItem: Label {
                                        text: "Garder celles de " + pairCard.modelData.aLabel
                                        color: "#FFFFFF"
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideLeft
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    background: Rectangle { radius: 10; color: keepA.hovered ? Theme.accent : Theme.accentButton }
                                    onClicked: dups.keepPairSide(pairCard.modelData.a, pairCard.modelData.b, true)
                                }
                                Button {
                                    id: more
                                    implicitWidth: 38
                                    implicitHeight: 38
                                    focusPolicy: Qt.NoFocus
                                    Accessible.name: "Autres choix pour cette paire"
                                    contentItem: Label { text: "▾"; color: "#FFFFFF"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { radius: 10; color: more.hovered ? Theme.accent : Theme.accentButton }
                                    onClicked: pairMenu.popup(more, 0, more.height + 4)
                                }
                            }
                        }

                        Menu {
                            id: pairMenu
                            Action {
                                text: "Garder celles de " + pairCard.modelData.bLabel
                                onTriggered: dups.keepPairSide(pairCard.modelData.a, pairCard.modelData.b, false)
                            }
                            MenuSeparator {}
                            Action {
                                text: "Toujours préférer « " + pairCard.modelData.aLabel + " » (règle)"
                                onTriggered: dups.addRule(pairCard.modelData.a, "prefer")
                            }
                            Action {
                                text: "Toujours écarter « " + pairCard.modelData.bLabel + " » (règle)"
                                onTriggered: dups.addRule(pairCard.modelData.b, "avoid")
                            }
                        }
                    }
                }

                SectionTitle { Layout.leftMargin: 16; text: "Doublons internes"; visible: dups.internals.length > 0 }
                Repeater {
                    model: dups.internals
                    delegate: Rectangle {
                        id: internalCard
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.leftMargin: 12
                        Layout.rightMargin: 12
                        implicitHeight: 54
                        radius: 10
                        color: internalMouse.containsMouse ? Theme.hover : Theme.panel
                        border.color: Theme.line
                        MouseArea {
                            id: internalMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: dups.folderFilter = internalCard.modelData.path
                        }
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 2
                            Label { Layout.fillWidth: true; text: internalCard.modelData.label; color: Theme.text; elide: Text.ElideLeft }
                            Label { text: internalCard.modelData.stats; color: Theme.text2; font.pixelSize: 12 }
                        }
                    }
                }

                SectionTitle { Layout.leftMargin: 16; text: "Règles" }
                Repeater {
                    model: dups.rules
                    delegate: Rectangle {
                        id: ruleCard
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.leftMargin: 12
                        Layout.rightMargin: 12
                        implicitHeight: 56
                        radius: 10
                        color: Theme.panel
                        border.color: ruleCard.modelData.found ? Theme.line : Theme.amberLine
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 6
                            spacing: 10
                            Rectangle {
                                implicitWidth: kindLabel.implicitWidth + 16
                                implicitHeight: 22
                                radius: 11
                                color: ruleCard.modelData.kind === "prefer" ? Theme.accentBg : Theme.amberBg
                                Label {
                                    id: kindLabel
                                    anchors.centerIn: parent
                                    text: ruleCard.modelData.kindLabel
                                    color: ruleCard.modelData.kind === "prefer" ? Theme.accentText : Theme.amberText
                                    font.pixelSize: 12
                                    font.weight: Font.DemiBold
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Label { Layout.fillWidth: true; text: ruleCard.modelData.path; color: Theme.text; elide: Text.ElideLeft }
                                Label {
                                    Layout.fillWidth: true
                                    text: ruleCard.modelData.note
                                    color: ruleCard.modelData.found ? Theme.text2 : Theme.amberText
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                }
                            }
                            ToolButton {
                                id: ruleMore
                                focusPolicy: Qt.NoFocus
                                Accessible.name: "Actions de la règle"
                                contentItem: Label { text: Theme.iconMore; font.family: Theme.iconFont; color: Theme.text2; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                background: Rectangle { radius: 8; color: ruleMore.hovered ? Theme.hover : "transparent" }
                                onClicked: ruleMenu.popup(ruleMore, 0, ruleMore.height)
                            }
                        }
                        Menu {
                            id: ruleMenu
                            Action {
                                text: ruleCard.modelData.kind === "prefer" ? "Changer en « Écarter »" : "Changer en « Préférer »"
                                onTriggered: dups.changeRule(ruleCard.modelData.id, ruleCard.modelData.kind === "prefer" ? "avoid" : "prefer")
                            }
                            Action {
                                text: "Rattacher à un autre dossier…"
                                onTriggered: { folderDialog.relinkId = ruleCard.modelData.id; folderDialog.open() }
                            }
                            MenuSeparator {}
                            Action { text: "Supprimer"; onTriggered: dups.removeRule(ruleCard.modelData.id) }
                        }
                    }
                }
                Button {
                    id: addRule
                    Layout.leftMargin: 12
                    implicitHeight: 36
                    focusPolicy: Qt.NoFocus
                    contentItem: Label { text: "+  Ajouter une règle…"; color: Theme.textSoft; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { radius: 8; color: addRule.hovered ? Theme.hover : "transparent"; border.color: Theme.lineStrong }
                    onClicked: { folderDialog.relinkId = 0; folderDialog.open() }
                }
                Label {
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    Layout.bottomMargin: 16
                    wrapMode: Text.WordWrap
                    text: "Les règles changent seulement la copie suggérée. Un choix fait à la main reste prioritaire."
                    color: Theme.text3
                    font.pixelSize: 12
                }
            }
        }
    }

    FolderDialog {
        id: folderDialog
        property var relinkId: 0
        title: relinkId ? "Rattacher la règle à un dossier" : "Dossier de la règle"
        onAccepted: {
            const path = dups.urlToPath(selectedFolder)
            if (relinkId)
                dups.relinkRule(relinkId, path)
            else {
                kindDialog.path = path
                kindDialog.open()
            }
        }
    }

    Dialog {
        id: kindDialog
        property string path
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        title: "Que faire des copies de ce dossier ?"
        width: Math.min(520, parent ? parent.width - 64 : 520)
        ColumnLayout {
            width: kindDialog.availableWidth
            spacing: 12
            Label { Layout.fillWidth: true; text: kindDialog.path; color: Theme.text2; elide: Text.ElideMiddle }
            RadioButton { id: preferRadio; text: "Toujours les préférer (les garder)"; checked: true }
            RadioButton { text: "Toujours les écarter (les proposer pour la corbeille)" }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: "Rien n'est mis à la corbeille sans votre confirmation."
                color: Theme.text3
                font.pixelSize: 12
            }
        }
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: dups.addRule(path, preferRadio.checked ? "prefer" : "avoid")
    }
}

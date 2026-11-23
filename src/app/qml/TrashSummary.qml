import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Récapitulatif avant toute mise à la corbeille (FR-016) : la liste exacte de ce qui part,
// la copie gardée de chaque groupe, ce qui ne sera pas touché et pourquoi. « Annuler » est le
// bouton par défaut : Entrée ne met rien à la corbeille.
Popup {
    id: root
    property var plan: ({})
    readonly property int count: plan.count || 0

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(720, parent ? parent.width - 64 : 720)
    height: Math.min(640, parent ? parent.height - 64 : 640)
    modal: true
    focus: true
    padding: 24
    closePolicy: Popup.CloseOnEscape
    onOpened: cancelBtn.forceActiveFocus()

    background: Rectangle { radius: 14; color: Theme.panel; border.color: Theme.lineStrong }

    ColumnLayout {
        anchors.fill: parent
        spacing: 14

        Label {
            Layout.fillWidth: true
            text: root.count > 0 ? "Mettre " + root.plan.countText + " à la corbeille ?" : "Rien à mettre à la corbeille"
            font.family: Theme.displayFont
            font.pixelSize: 20
            font.weight: Font.DemiBold
            color: Theme.text
            wrapMode: Text.WordWrap
        }
        Label {
            visible: root.count > 0
            text: root.plan.bytesText + " libérés"
            color: Theme.text2
        }
        Label {
            visible: (root.plan.refusedCount || 0) > 0
            Layout.fillWidth: true
            text: "Gardez au moins une copie de chaque photo. " + root.plan.refusedCount
                  + (root.plan.refusedCount > 1 ? " groupes ne seront pas traités." : " groupe ne sera pas traité.")
            color: Theme.errText
            wrapMode: Text.WordWrap
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                width: parent.width
                spacing: 12

                Repeater {
                    model: root.plan.groups || []
                    delegate: ColumnLayout {
                        id: g
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 4
                        Label {
                            Layout.fillWidth: true
                            text: Theme.iconStar + "  Gardée : " + g.modelData.keeperName
                            font.family: Theme.iconFont + ", " + Theme.font
                            color: Theme.accentText
                            elide: Text.ElideMiddle
                        }
                        Label {
                            Layout.fillWidth: true
                            text: g.modelData.keeperPath
                            color: Theme.text3
                            font.pixelSize: 12
                            elide: Text.ElideMiddle
                        }
                        Repeater {
                            model: g.modelData.files
                            delegate: Label {
                                required property var modelData
                                Layout.fillWidth: true
                                Layout.leftMargin: 18
                                text: Theme.iconDelete + "  " + modelData.name + " — " + modelData.folder + " · " + modelData.size
                                font.family: Theme.iconFont + ", " + Theme.font
                                color: Theme.text
                                elide: Text.ElideMiddle
                            }
                        }
                    }
                }

                Label {
                    visible: (root.plan.blocked || []).length > 0
                    Layout.topMargin: 8
                    text: "Ne seront pas touchés"
                    font.weight: Font.DemiBold
                    color: Theme.text
                }
                Repeater {
                    model: root.plan.blocked || []
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 10
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1
                            Label { Layout.fillWidth: true; text: modelData.name + " — " + modelData.reason; color: Theme.amberText; elide: Text.ElideMiddle }
                            Label { Layout.fillWidth: true; text: modelData.path; color: Theme.text3; font.pixelSize: 12; elide: Text.ElideMiddle }
                        }
                        Button {
                            text: "Afficher dans l'Explorateur"
                            focusPolicy: Qt.NoFocus
                            onClicked: app.revealInExplorer(modelData.item)
                        }
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            text: "Les fichiers iront dans la corbeille de Windows. Vous pourrez les restaurer depuis le journal des opérations."
            color: Theme.text2
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Item { Layout.fillWidth: true }
            Button {
                id: cancelBtn
                text: "Annuler"
                implicitHeight: 40
                leftPadding: 18
                rightPadding: 18
                onClicked: root.close()
                Keys.onReturnPressed: root.close()
                Keys.onEnterPressed: root.close()
            }
            Button {
                id: confirmBtn
                visible: root.count > 0
                implicitHeight: 40
                leftPadding: 18
                rightPadding: 18
                focusPolicy: Qt.TabFocus
                onClicked: {
                    trash.confirm()
                    root.close()
                }
                contentItem: Label { text: "Mettre à la corbeille"; color: "#FFFFFF"; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { radius: 8; color: confirmBtn.hovered ? Theme.accent : Theme.accentButton }
            }
        }
    }
}

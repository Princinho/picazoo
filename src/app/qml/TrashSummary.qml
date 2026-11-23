import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Récapitulatif avant toute mise à la corbeille (FR-016 de la 002 ; 006 : FR-016 à FR-020) :
// totaux par niveau (les « Très ressemblante » non vérifiées à part, non incluses par défaut),
// fichiers par dossier d'origine avec la copie gardée, ce qui ne sera pas touché et pourquoi.
// Préparé hors du fil de l'interface. « Annuler » est le bouton par défaut : Entrée ne met rien à
// la corbeille.
Popup {
    id: root
    readonly property var plan: trash.plan
    readonly property int count: plan.count || 0
    property var expanded: ({})      // index de dossier -> fichiers chargés
    property bool blockedOpen: false

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(820, parent ? parent.width - 64 : 820)
    height: Math.min(720, parent ? parent.height - 64 : 720)
    modal: true
    focus: true
    padding: 24
    closePolicy: Popup.CloseOnEscape
    onOpened: { expanded = ({}); blockedOpen = false; cancelBtn.forceActiveFocus() }
    onClosed: if (!trash.busy) trash.cancelPlan()

    Connections {
        target: trash
        function onPlanChanged() { root.expanded = ({}) }
    }

    background: Rectangle { radius: 14; color: Theme.panel; border.color: Theme.lineStrong }

    ColumnLayout {
        anchors.fill: parent
        spacing: 14

        Label {
            Layout.fillWidth: true
            text: trash.preparing ? "Préparation du récapitulatif…"
                                  : (root.count > 0 ? "Mettre " + root.plan.countText + " à la corbeille ?" : "Rien à mettre à la corbeille")
            font.family: Theme.displayFont
            font.pixelSize: 20
            font.weight: Font.DemiBold
            color: Theme.text
            wrapMode: Text.WordWrap
        }
        Label {
            visible: !trash.preparing && root.count > 0
            text: root.plan.bytesText + " libérés · " + root.plan.groupsText
            color: Theme.text2
        }
        BusyIndicator {
            visible: trash.preparing
            running: trash.preparing
            Layout.alignment: Qt.AlignHCenter
        }

        // Niveaux à inclure (FR-018)
        Rectangle {
            visible: !trash.preparing && (root.plan.levels || []).length > 0
            Layout.fillWidth: true
            implicitHeight: levelsCol.implicitHeight
            radius: 10
            color: "transparent"
            border.color: Theme.line
            clip: true
            ColumnLayout {
                id: levelsCol
                width: parent.width
                spacing: 0
                Repeater {
                    model: root.plan.levels || []
                    delegate: Rectangle {
                        id: lvl
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true
                        implicitHeight: 44
                        color: lvl.modelData.included ? Theme.panel : Theme.sidebar
                        Rectangle { visible: lvl.index > 0; width: parent.width; height: 1; color: Theme.line }
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 6
                            anchors.rightMargin: 14
                            spacing: 10
                            PzCheckBox {
                                checked: lvl.modelData.included
                                focusPolicy: Qt.TabFocus
                                Accessible.name: "Inclure : " + lvl.modelData.label
                                onToggled: trash.setLevelIncluded(lvl.modelData.key, checked)
                            }
                            Rectangle {
                                implicitWidth: lvlBadge.implicitWidth + 18
                                implicitHeight: 24
                                radius: 12
                                color: lvl.modelData.badge === "identical" ? Theme.okBg : (lvl.modelData.badge === "same_image" ? Theme.accentBg : Theme.amberBg)
                                Label {
                                    id: lvlBadge
                                    anchors.centerIn: parent
                                    text: lvl.modelData.label
                                    font.pixelSize: 12
                                    font.weight: Font.DemiBold
                                    color: lvl.modelData.badge === "identical" ? Theme.okText : (lvl.modelData.badge === "same_image" ? Theme.accentText : Theme.amberText)
                                }
                            }
                            Label {
                                Layout.fillWidth: true
                                text: lvl.modelData.stats
                                color: lvl.modelData.included ? Theme.text : Theme.text3
                                elide: Text.ElideRight
                            }
                            Label {
                                text: lvl.modelData.note
                                color: lvl.modelData.badge === "similar" ? Theme.amberText : Theme.text2
                                font.pixelSize: 12
                            }
                        }
                    }
                }
            }
        }

        Label {
            visible: !trash.preparing && (root.plan.refusedCount || 0) > 0
            Layout.fillWidth: true
            text: "Gardez au moins une copie de chaque photo. " + root.plan.refusedCount
                  + (root.plan.refusedCount > 1 ? " groupes ne seront pas traités." : " groupe ne sera pas traité.")
            color: Theme.errText
            wrapMode: Text.WordWrap
        }

        Label {
            visible: !trash.preparing && root.count > 0
            text: "PAR DOSSIER D'ORIGINE"
            color: Theme.text3
            font.pixelSize: 11
            font.letterSpacing: 0.6
        }

        ScrollView {
            visible: !trash.preparing
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                width: parent.width
                spacing: 2

                Repeater {
                    model: root.plan.folders || []
                    delegate: ColumnLayout {
                        id: folder
                        required property var modelData
                        readonly property var files: root.expanded[modelData.index]
                        Layout.fillWidth: true
                        spacing: 2
                        Button {
                            id: folderBtn
                            Layout.fillWidth: true
                            implicitHeight: 38
                            focusPolicy: Qt.TabFocus
                            Accessible.name: folder.modelData.path
                            onClicked: {
                                const next = Object.assign({}, root.expanded)
                                if (next[folder.modelData.index]) delete next[folder.modelData.index]
                                else next[folder.modelData.index] = trash.folderFiles(folder.modelData.index)
                                root.expanded = next
                            }
                            contentItem: RowLayout {
                                spacing: 10
                                Label { text: folder.files ? "▾" : "▸"; color: Theme.text2 }
                                Label { text: Theme.iconFolder; font.family: Theme.iconFont; color: Theme.text3 }
                                Label { Layout.fillWidth: true; text: folder.modelData.path; color: Theme.text; font.weight: Font.DemiBold; elide: Text.ElideLeft }
                                Label { text: folder.modelData.stats; color: Theme.text2 }
                            }
                            background: Rectangle { radius: 8; color: folderBtn.hovered ? Theme.hover : "transparent" }
                        }
                        Repeater {
                            model: folder.files || []
                            delegate: RowLayout {
                                required property var modelData
                                Layout.fillWidth: true
                                Layout.leftMargin: 40
                                spacing: 12
                                Label { Layout.preferredWidth: 200; text: modelData.name; color: Theme.text; elide: Text.ElideMiddle }
                                Label { Layout.preferredWidth: 70; text: modelData.size; color: Theme.text2; horizontalAlignment: Text.AlignRight }
                                Label {
                                    Layout.fillWidth: true
                                    text: Theme.iconStar + "  gardée : " + modelData.keeper
                                    font.family: Theme.iconFont + ", " + Theme.font
                                    color: Theme.text2
                                    elide: Text.ElideMiddle
                                }
                            }
                        }
                        Label {
                            visible: !!folder.files && folder.modelData.moreText !== ""
                            Layout.leftMargin: 40
                            text: folder.modelData.moreText
                            color: Theme.text3
                            font.pixelSize: 12
                        }
                    }
                }

                // Ne seront pas touchés
                Button {
                    id: blockedBtn
                    visible: (root.plan.blocked || []).length > 0
                    Layout.fillWidth: true
                    Layout.topMargin: 8
                    implicitHeight: 36
                    focusPolicy: Qt.TabFocus
                    onClicked: root.blockedOpen = !root.blockedOpen
                    contentItem: RowLayout {
                        spacing: 10
                        Label { text: root.blockedOpen ? "▾" : "▸"; color: Theme.text2 }
                        Label { Layout.fillWidth: true; text: "Ne seront pas touchés (" + (root.plan.blocked || []).length + ")"; color: Theme.text; font.weight: Font.DemiBold }
                    }
                    background: Rectangle { radius: 8; color: blockedBtn.hovered ? Theme.hover : "transparent" }
                }
                Repeater {
                    model: root.blockedOpen ? (root.plan.blocked || []) : []
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.leftMargin: 34
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

        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.line }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Label {
                Layout.fillWidth: true
                text: "Les fichiers iront dans la corbeille de Windows. Vous pourrez les restaurer depuis le journal des opérations."
                color: Theme.text2
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
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
                visible: root.count > 0 && !trash.preparing
                implicitHeight: 40
                leftPadding: 18
                rightPadding: 18
                focusPolicy: Qt.TabFocus
                onClicked: {
                    trash.confirm()
                    root.close()
                }
                contentItem: Label { text: "Mettre " + root.plan.countText + " à la corbeille"; color: "#FFFFFF"; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { radius: 8; color: confirmBtn.hovered ? Theme.accent : Theme.accentButton }
            }
        }
    }
}

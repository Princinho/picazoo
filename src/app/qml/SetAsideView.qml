import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// « Écartés de la revue » (006, US5) : les actions en lot (annulables d'un coup) et les groupes
// sortis de la revue (« Garder toutes », « Ce ne sont pas des doublons »), à remettre en revue.
Item {
    id: view
    property var checked: ({})   // index -> true
    readonly property int checkedCount: Object.keys(checked).filter(k => checked[k]).length

    Connections {
        target: dups
        function onSetAsideChanged() { view.checked = ({}) }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                width: parent.width
                spacing: 8

                Label {
                    Layout.leftMargin: 20
                    Layout.topMargin: 8
                    text: "ACTIONS EN LOT"
                    color: Theme.text3
                    font.pixelSize: 11
                    font.letterSpacing: 0.6
                }
                Label {
                    visible: dups.batchList.length === 0
                    Layout.leftMargin: 20
                    text: "Aucune action en lot pour l'instant."
                    color: Theme.text2
                }
                Repeater {
                    model: dups.batchList
                    delegate: Rectangle {
                        id: batchCard
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.leftMargin: 20
                        Layout.rightMargin: 20
                        implicitHeight: 56
                        radius: 10
                        color: Theme.panel
                        border.color: Theme.line
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 10
                            spacing: 12
                            Label {
                                Layout.fillWidth: true
                                text: batchCard.modelData.title
                                color: batchCard.modelData.undone ? Theme.text3 : Theme.text
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }
                            Label {
                                visible: batchCard.modelData.undone
                                text: "Annulée"
                                color: Theme.okText
                            }
                            Button {
                                id: undoBtn
                                visible: !batchCard.modelData.undone
                                implicitHeight: 36
                                focusPolicy: Qt.NoFocus
                                contentItem: Label {
                                    text: Theme.iconUndo + "  Annuler cette action"
                                    font.family: Theme.iconFont + ", " + Theme.font
                                    color: Theme.text
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle { radius: 8; color: undoBtn.hovered ? Theme.hover : Theme.surface; border.color: Theme.lineStrong }
                                onClicked: dups.undoBatch(batchCard.modelData.id)
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.leftMargin: 20
                    Layout.topMargin: 14
                    spacing: 8
                    PzCheckBox {
                        focusPolicy: Qt.NoFocus
                        enabled: dups.setAsideEntries.length > 0
                        checked: dups.setAsideEntries.length > 0 && view.checkedCount === dups.setAsideEntries.length
                        Accessible.name: "Tout sélectionner"
                        onToggled: {
                            const all = view.checkedCount === dups.setAsideEntries.length
                            const next = {}
                            if (!all)
                                for (let i = 0; i < dups.setAsideEntries.length; ++i) next[i] = true
                            view.checked = next
                        }
                    }
                    Label { text: "GROUPES ÉCARTÉS"; color: Theme.text3; font.pixelSize: 11; font.letterSpacing: 0.6 }
                }
                Label {
                    visible: dups.setAsideEntries.length === 0
                    Layout.leftMargin: 20
                    text: "Aucun groupe n'a été écarté de la revue."
                    color: Theme.text2
                }
                Repeater {
                    model: dups.setAsideEntries
                    delegate: Rectangle {
                        id: entry
                        required property var modelData
                        readonly property bool on: !!view.checked[modelData.index]
                        Layout.fillWidth: true
                        Layout.leftMargin: 20
                        Layout.rightMargin: 20
                        implicitHeight: 60
                        radius: 10
                        color: entry.on ? "#1B2330" : Theme.sidebar
                        border.color: entry.on ? Theme.accentButton : Theme.line
                        RowLayout {
                            anchors.fill: parent
                            anchors.rightMargin: 14
                            spacing: 14
                            Item {
                                Layout.preferredWidth: 44
                                Layout.fillHeight: true
                                PzCheckBox {
                                    anchors.centerIn: parent
                                    focusPolicy: Qt.NoFocus
                                    checked: entry.on
                                    Accessible.name: "Sélectionner " + entry.modelData.name
                                    onToggled: {
                                        const next = Object.assign({}, view.checked)
                                        next[entry.modelData.index] = !entry.on
                                        view.checked = next
                                    }
                                }
                            }
                            Row {
                                spacing: 6
                                Repeater {
                                    model: entry.modelData.thumbs
                                    delegate: Image {
                                        required property var modelData
                                        width: 40
                                        height: 40
                                        opacity: 0.8
                                        fillMode: Image.PreserveAspectCrop
                                        asynchronous: true
                                        source: "image://thumb/" + modelData.id + "/" + modelData.version + "/done"
                                        sourceSize: Qt.size(96, 96)
                                    }
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Label { Layout.fillWidth: true; text: entry.modelData.name; color: Theme.textSoft; font.weight: Font.DemiBold; elide: Text.ElideMiddle }
                                Label { Layout.fillWidth: true; text: entry.modelData.folders; color: Theme.text3; font.pixelSize: 12; elide: Text.ElideLeft }
                            }
                            Rectangle {
                                implicitWidth: decision.implicitWidth + 18
                                implicitHeight: 24
                                radius: 12
                                color: Theme.surface
                                border.color: Theme.lineStrong
                                Label { id: decision; anchors.centerIn: parent; text: entry.modelData.decision; color: Theme.textSoft; font.pixelSize: 12 }
                            }
                            Label { Layout.preferredWidth: 160; text: entry.modelData.date; color: Theme.text3; font.pixelSize: 12 }
                        }
                    }
                }
                Item { height: 16 }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            color: Theme.panel
            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Theme.line }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 24
                anchors.rightMargin: 24
                spacing: 12
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1
                    Label {
                        text: view.checkedCount > 0 ? view.checkedCount + (view.checkedCount > 1 ? " groupes sélectionnés" : " groupe sélectionné") : "Aucun groupe sélectionné"
                        color: Theme.text
                        font.weight: Font.DemiBold
                    }
                    Label {
                        text: "Remettre dans la revue annule la décision : les groupes reviennent après la mise à jour des groupes."
                        color: Theme.text2
                        font.pixelSize: 12
                    }
                }
                Button {
                    id: restoreBtn
                    enabled: view.checkedCount > 0
                    implicitHeight: 40
                    leftPadding: 18
                    rightPadding: 18
                    focusPolicy: Qt.NoFocus
                    contentItem: Label {
                        text: Theme.iconUndo + "   Remettre dans la revue"
                        font.family: Theme.iconFont + ", " + Theme.font
                        color: restoreBtn.enabled ? "#FFFFFF" : Theme.text3
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle { radius: 10; color: restoreBtn.enabled ? (restoreBtn.hovered ? Theme.accent : Theme.accentButton) : Theme.surface }
                    onClicked: dups.restoreSetAside(Object.keys(view.checked).filter(k => view.checked[k]).map(k => Number(k)))
                }
            }
        }
    }
}

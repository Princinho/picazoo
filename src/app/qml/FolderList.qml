import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Liste des dossiers : « Par année » (comme Picasa) ou « Arborescence ».
Rectangle {
    id: root
    color: Theme.sidebar
    property var currentFolderId: 0
    property bool duplicatesActive: false
    signal folderClicked(var id)
    signal duplicatesClicked()

    onCurrentFolderIdChanged: {
        const row = folderModel.rowOfFolder(currentFolderId)
        if (row >= 0)
            list.positionViewAtIndex(row, ListView.Contain)
    }

    component Segment: Button {
        id: seg
        property bool selected: false
        Layout.fillWidth: true
        implicitHeight: 34
        Accessible.checked: selected
        contentItem: Label {
            text: seg.text
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            color: seg.selected ? "#FFFFFF" : Theme.text2
            font.weight: seg.selected ? Font.DemiBold : Font.Normal
        }
        background: Rectangle {
            radius: 8
            color: seg.selected ? "#2A3240" : (seg.hovered ? Theme.hover : "transparent")
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 40
            radius: 10
            color: Theme.field
            RowLayout {
                anchors.fill: parent
                anchors.margins: 3
                spacing: 3
                Segment {
                    text: "Par année"
                    selected: folderModel.mode !== "tree"
                    onClicked: folderModel.mode = "by_year"
                }
                Segment {
                    text: "Arborescence"
                    selected: folderModel.mode === "tree"
                    onClicked: folderModel.mode = "tree"
                }
            }
        }

        // Entrée « Doublons » (002) : visible dès qu'un groupe existe ou pendant l'analyse.
        Rectangle {
            id: dupEntry
            visible: dups !== null && (dups.groupCount > 0 || dups.active)
            Layout.fillWidth: true
            implicitHeight: 52
            radius: 8
            color: root.duplicatesActive ? Theme.accentBg : (dupMouse.containsMouse ? Theme.hover : "transparent")
            Accessible.role: Accessible.Button
            Accessible.name: "Doublons — " + (dups ? dups.summaryText : "")
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 10
                Rectangle {
                    Layout.preferredWidth: 34
                    Layout.preferredHeight: 34
                    radius: 7
                    color: root.duplicatesActive ? "#24456A" : "#252B35"
                    Label {
                        anchors.centerIn: parent
                        text: Theme.iconDuplicates
                        font.family: Theme.iconFont
                        color: root.duplicatesActive ? Theme.accentText : Theme.text3
                        font.pixelSize: 15
                        Accessible.ignored: true
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1
                    Label { text: "Doublons"; color: Theme.text; font.weight: Font.DemiBold }
                    Label {
                        Layout.fillWidth: true
                        text: dups ? (dups.groupCount > 0 ? dups.summaryText : "Recherche en cours…") : ""
                        elide: Text.ElideRight
                        font.pixelSize: 12
                        color: Theme.text3
                    }
                }
                Rectangle {
                    visible: dups !== null && dups.active
                    Layout.preferredWidth: 8
                    Layout.preferredHeight: 8
                    radius: 4
                    color: Theme.amber
                }
            }
            MouseArea {
                id: dupMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.duplicatesClicked()
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: folderModel
            spacing: 2
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            delegate: Item {
                id: row
                required property int index
                required property string rowKind
                required property var folderId
                required property string name
                required property string path
                required property int count
                required property string dateText
                required property int depth
                required property bool expandable
                required property bool expanded
                required property bool unavailable

                readonly property bool active: rowKind === "folder" && folderId !== 0 && folderId === root.currentFolderId

                width: ListView.view.width
                height: rowKind === "year" ? 40 : 52

                Label {
                    visible: row.rowKind === "year"
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 6
                    text: row.name
                    font.family: Theme.displayFont
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    color: Theme.text
                }

                Rectangle {
                    visible: row.rowKind !== "year"
                    anchors.fill: parent
                    radius: 8
                    color: row.active ? Theme.accentBg : (mouse.containsMouse ? Theme.hover : "transparent")

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8 + row.depth * 16
                        anchors.rightMargin: 8
                        spacing: 10

                        Label {
                            visible: folderModel.mode === "tree"
                            text: row.expandable ? (row.expanded ? "▾" : "▸") : ""
                            Layout.preferredWidth: 12
                            color: Theme.text2
                            MouseArea {
                                anchors.fill: parent
                                anchors.margins: -8
                                onClicked: folderModel.toggle(row.index)
                            }
                        }
                        // Pastille de dossier
                        Rectangle {
                            Layout.preferredWidth: 34
                            Layout.preferredHeight: 34
                            radius: 7
                            color: row.rowKind === "node" ? Theme.surface : (row.active ? "#24456A" : "#252B35")
                            Label {
                                anchors.centerIn: parent
                                text: Theme.iconFolder
                                font.family: Theme.iconFont
                                color: row.active ? Theme.accentText : Theme.text3
                                font.pixelSize: 15
                                Accessible.ignored: true
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1
                            Label {
                                Layout.fillWidth: true
                                text: row.name + (row.unavailable ? " (indisponible)" : "")
                                elide: Text.ElideRight
                                color: row.unavailable ? Theme.text3 : (row.rowKind === "node" ? Theme.text2 : Theme.text)
                            }
                            Label {
                                visible: row.rowKind === "folder"
                                Layout.fillWidth: true
                                text: row.count + (row.count > 1 ? " éléments" : " élément")
                                      + (row.dateText !== "" ? " · " + row.dateText : "")
                                elide: Text.ElideRight
                                font.pixelSize: 12
                                color: Theme.text3
                            }
                        }
                    }
                }

                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    anchors.leftMargin: folderModel.mode === "tree" ? 28 + row.depth * 16 : 0
                    hoverEnabled: true
                    enabled: row.rowKind !== "year"
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (row.rowKind === "folder")
                            root.folderClicked(row.folderId)
                        else
                            folderModel.toggle(row.index)
                    }
                    ToolTip.visible: containsMouse && row.path !== ""
                    ToolTip.delay: 800
                    ToolTip.text: row.path
                }
            }
        }
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Liste des dossiers : « Par année » (comme Picasa) ou « Arborescence ».
Rectangle {
    id: root
    color: "#232323"
    property var currentFolderId: 0
    signal folderClicked(var id)

    onCurrentFolderIdChanged: {
        const row = folderModel.rowOfFolder(currentFolderId)
        if (row >= 0)
            list.positionViewAtIndex(row, ListView.Contain)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TabBar {
            id: modeBar
            Layout.fillWidth: true
            currentIndex: folderModel.mode === "tree" ? 1 : 0
            TabButton {
                text: "Par année"
                onClicked: folderModel.mode = "by_year"
            }
            TabButton {
                text: "Arborescence"
                onClicked: folderModel.mode = "tree"
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: folderModel
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            delegate: Rectangle {
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

                width: ListView.view.width
                height: rowKind === "year" ? 34 : 44
                color: rowKind !== "year" && folderId === root.currentFolderId && folderId !== 0 ? "#34495e"
                     : mouse.containsMouse && rowKind !== "year" ? "#2d2d2d" : "transparent"

                Label {
                    visible: row.rowKind === "year"
                    anchors.left: parent.left
                    anchors.leftMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    text: row.name
                    font.pixelSize: 15
                    font.bold: true
                    color: "#d0d0d0"
                }

                RowLayout {
                    visible: row.rowKind !== "year"
                    anchors.fill: parent
                    anchors.leftMargin: 12 + row.depth * 16
                    anchors.rightMargin: 8
                    spacing: 6

                    Label {
                        text: row.expandable ? (row.expanded ? "▾" : "▸") : ""
                        Layout.preferredWidth: folderModel.mode === "tree" ? 12 : 0
                        color: "#a0a0a0"
                        MouseArea {
                            anchors.fill: parent
                            anchors.margins: -6
                            onClicked: folderModel.toggle(row.index)
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1
                        Label {
                            Layout.fillWidth: true
                            text: row.name + (row.unavailable ? " (indisponible)" : "")
                            elide: Text.ElideRight
                            color: row.unavailable ? "#777777" : (row.rowKind === "node" ? "#a0a0a0" : "#e6e6e6")
                        }
                        Label {
                            visible: row.rowKind === "folder"
                            Layout.fillWidth: true
                            text: row.count + (row.count > 1 ? " éléments" : " élément")
                                  + (row.dateText !== "" ? " · " + row.dateText : "")
                            elide: Text.ElideRight
                            font.pixelSize: 11
                            color: "#8a8a8a"
                        }
                    }
                }

                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    anchors.leftMargin: folderModel.mode === "tree" ? 24 + row.depth * 16 : 0
                    hoverEnabled: true
                    enabled: row.rowKind !== "year"
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

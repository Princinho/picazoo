import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Écran Bibliothèque : dossiers à gauche, grille continue à droite, barre du bas.
FocusScope {
    id: root
    signal openItem(var id)

    function revealItem(id) { grid.revealItem(id) }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        ScanIndicator {
            Layout.fillWidth: true
        }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            FolderList {
                id: folderList
                SplitView.preferredWidth: 300
                SplitView.minimumWidth: 180
                currentFolderId: grid.topFolderId
                onFolderClicked: id => grid.jumpToFolder(id)
            }

            ThumbGrid {
                id: grid
                SplitView.fillWidth: true
                focus: true
                onOpenItem: id => root.openItem(id)
            }
        }

        // Barre du bas : taille des vignettes, sélection, tris.
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            color: "#2b2b2b"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 16

                Label {
                    text: gridModel.itemCount === 0 ? "Aucune photo"
                          : gridModel.itemCount.toLocaleString(Qt.locale("fr_FR"), "f", 0) + " éléments dans "
                            + gridModel.folderCount.toLocaleString(Qt.locale("fr_FR"), "f", 0) + " dossiers"
                    color: "#b0b0b0"
                }
                Label {
                    visible: selection.count > 1
                    text: selection.count + " éléments sélectionnés"
                }
                Item { Layout.fillWidth: true }

                Label { text: "Dossiers :"; color: "#b0b0b0" }
                ComboBox {
                    id: folderSortBox
                    implicitWidth: 200
                    textRole: "text"
                    valueRole: "value"
                    model: [
                        { text: "Date (récents d'abord)", value: "date_desc" },
                        { text: "Date (anciens d'abord)", value: "date_asc" },
                        { text: "Nom", value: "name_asc" }
                    ]
                    Component.onCompleted: currentIndex = indexOfValue(app.folderSort)
                    onActivated: app.folderSort = currentValue
                }
                Label { text: "Photos :"; color: "#b0b0b0" }
                ComboBox {
                    implicitWidth: 190
                    textRole: "text"
                    valueRole: "value"
                    model: [
                        { text: "Date de prise de vue", value: "taken_asc" },
                        { text: "Nom", value: "name_asc" }
                    ]
                    Component.onCompleted: currentIndex = indexOfValue(app.itemSort)
                    onActivated: app.itemSort = currentValue
                }
                Label { text: "Taille :"; color: "#b0b0b0" }
                Slider {
                    implicitWidth: 160
                    from: 64
                    to: 320
                    stepSize: 8
                    value: gridModel.thumbSize
                    onMoved: gridModel.thumbSize = value
                }
            }
        }
    }
}

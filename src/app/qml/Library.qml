import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Écran Bibliothèque : barre du haut, dossiers à gauche, grille continue, barre du bas.
FocusScope {
    id: root
    signal openItem(var id)
    signal addFolder()
    signal watchedFolders()
    signal operationLog()

    // Écran « Doublons » (002) à la place de la grille.
    property bool showDuplicates: false

    function revealItem(id) {
        if (!root.showDuplicates)
            grid.revealItem(id)
    }
    // « Ouvrir dans Picazoo » (005) : revient à la grille depuis l'écran « Doublons ».
    function showItemInGrid(id) {
        root.showDuplicates = false
        grid.revealItem(id)
        grid.forceActiveFocus()
    }
    function openTrashSummary() { duplicatesView.openSummary(false) }
    function showGroup(groupId) {
        root.showDuplicates = true
        duplicatesView.focusGroup(groupId)
        duplicatesView.forceActiveFocus()
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.bg
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        AppHeader {
            Layout.fillWidth: true
            onAddFolder: root.addFolder()
            onWatchedFolders: root.watchedFolders()
            onOperationLog: root.operationLog()
            onShowDuplicates: root.showGroup(0)
        }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal
            handle: Rectangle {
                implicitWidth: 1
                color: SplitHandle.hovered || SplitHandle.pressed ? Theme.accent : Theme.line
            }

            FolderList {
                id: folderList
                SplitView.preferredWidth: 288
                SplitView.minimumWidth: 200
                currentFolderId: root.showDuplicates ? 0 : grid.topFolderId
                duplicatesActive: root.showDuplicates
                onFolderClicked: id => {
                    root.showDuplicates = false
                    grid.jumpToFolder(id)
                    grid.forceActiveFocus()
                }
                onDuplicatesClicked: root.showGroup(0)
            }

            Item {
                SplitView.fillWidth: true

                ThumbGrid {
                    id: grid
                    anchors.fill: parent
                    visible: !root.showDuplicates
                    focus: !root.showDuplicates
                    onOpenItem: id => root.openItem(id)
                }

                Duplicates {
                    id: duplicatesView
                    anchors.fill: parent
                    visible: root.showDuplicates
                    focus: root.showDuplicates
                    onOpenItem: id => root.openItem(id)
                    onBack: {
                        root.showDuplicates = false
                        grid.forceActiveFocus()
                    }
                }
            }
        }

        // Barre du bas : compteur, sélection, tris, taille des vignettes.
        Rectangle {
            visible: !root.showDuplicates
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            color: Theme.panel
            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Theme.line }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 24
                anchors.rightMargin: 24
                spacing: 20

                Label {
                    text: gridModel.itemCount === 0 ? "Aucune photo"
                          : gridModel.itemCount.toLocaleString(Qt.locale("fr_FR"), "f", 0) + " éléments dans "
                            + gridModel.folderCount.toLocaleString(Qt.locale("fr_FR"), "f", 0) + " dossiers"
                    color: Theme.text2
                }
                Rectangle {
                    visible: selection.count > 1
                    Layout.preferredHeight: 26
                    Layout.preferredWidth: selLabel.implicitWidth + 20
                    radius: 13
                    color: Theme.accentBg
                    Label {
                        id: selLabel
                        anchors.centerIn: parent
                        text: selection.count + " sélectionnés"
                        color: Theme.accentText
                    }
                }
                Item { Layout.fillWidth: true }

                Label { text: "Dossiers"; color: Theme.text2 }
                ThemedCombo {
                    implicitWidth: 180
                    model: [
                        { text: "Récents d'abord", value: "date_desc" },
                        { text: "Anciens d'abord", value: "date_asc" },
                        { text: "Nom", value: "name_asc" }
                    ]
                    Component.onCompleted: currentIndex = indexOfValue(app.folderSort)
                    onActivated: app.folderSort = currentValue
                }
                Label { text: "Photos"; color: Theme.text2 }
                ThemedCombo {
                    implicitWidth: 200
                    model: [
                        { text: "Date de prise de vue", value: "taken_asc" },
                        { text: "Nom", value: "name_asc" }
                    ]
                    Component.onCompleted: currentIndex = indexOfValue(app.itemSort)
                    onActivated: app.itemSort = currentValue
                }
                RowLayout {
                    spacing: 8
                    Label { text: Theme.iconGrid; font.family: Theme.iconFont; color: Theme.text2; font.pixelSize: 15; Accessible.ignored: true }
                    Label { text: "Taille"; color: Theme.text2 }
                    Slider {
                        id: sizeSlider
                        implicitWidth: 150
                        from: 64
                        to: 320
                        stepSize: 8
                        value: gridModel.thumbSize
                        onMoved: gridModel.thumbSize = value
                        Accessible.name: "Taille des vignettes"
                        background: Rectangle {
                            x: sizeSlider.leftPadding
                            y: sizeSlider.topPadding + sizeSlider.availableHeight / 2 - height / 2
                            width: sizeSlider.availableWidth
                            height: 4
                            radius: 2
                            color: Theme.lineStrong
                            Rectangle {
                                width: sizeSlider.visualPosition * parent.width
                                height: parent.height
                                radius: 2
                                color: Theme.accent
                            }
                        }
                        handle: Rectangle {
                            x: sizeSlider.leftPadding + sizeSlider.visualPosition * (sizeSlider.availableWidth - width)
                            y: sizeSlider.topPadding + sizeSlider.availableHeight / 2 - height / 2
                            width: 16
                            height: 16
                            radius: 8
                            color: sizeSlider.pressed ? Theme.accentText : Theme.text
                        }
                    }
                }
            }
        }
    }
}

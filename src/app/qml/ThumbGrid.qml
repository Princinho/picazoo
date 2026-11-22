import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Grille continue (R9) : un en-tête par dossier puis ses rangées de vignettes.
FocusScope {
    id: root
    signal openItem(var id)
    property var topFolderId: 0
    property var menuItem: 0

    function jumpToFolder(folderId) {
        const row = gridModel.headerRowOfFolder(folderId)
        if (row < 0)
            return
        list.positionViewAtIndex(row, ListView.Beginning)
        const first = gridModel.firstItemOfFolder(folderId)
        if (first)
            selection.current = first
        app.lastFolderId = folderId
        list.forceActiveFocus()
    }

    function revealItem(id) {
        const row = gridModel.rowOfItem(id)
        if (row >= 0)
            list.positionViewAtIndex(row, ListView.Contain)
    }

    function reportViewport() {
        if (list.count === 0)
            return
        let first = list.indexAt(20, list.contentY + 1)
        let last = list.indexAt(20, list.contentY + list.height - 1)
        if (first < 0)
            first = 0
        if (last < 0)
            last = Math.min(list.count - 1, first + Math.ceil(list.height / 60))
        gridModel.reportRows(first, last)
        const top = list.itemAt(20, list.contentY + 1)
        if (top)
            root.topFolderId = top.folderId
    }

    Timer {
        id: viewportTimer
        interval: 60
        onTriggered: root.reportViewport()
    }

    Connections {
        target: gridModel
        function onStructureChanged() { viewportTimer.restart() }
    }

    ListView {
        id: list
        anchors.fill: parent
        clip: true
        focus: true
        model: gridModel
        reuseItems: true
        cacheBuffer: Math.max(0, height)
        boundsBehavior: Flickable.StopAtBounds
        maximumFlickVelocity: 6000
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOn }

        onWidthChanged: gridModel.viewWidth = width
        Component.onCompleted: gridModel.viewWidth = width
        onContentYChanged: viewportTimer.restart()
        onHeightChanged: viewportTimer.restart()

        WheelHandler {
            acceptedModifiers: Qt.ControlModifier
            onWheel: event => gridModel.thumbSize = gridModel.thumbSize + (event.angleDelta.y > 0 ? 16 : -16)
        }

        Keys.onPressed: event => {
            const extend = (event.modifiers & Qt.ShiftModifier) !== 0
            let moved = 0
            switch (event.key) {
            case Qt.Key_Left: moved = selection.move(-1, 0, extend); break
            case Qt.Key_Right: moved = selection.move(1, 0, extend); break
            case Qt.Key_Up: moved = selection.move(0, -1, extend); break
            case Qt.Key_Down: moved = selection.move(0, 1, extend); break
            case Qt.Key_Return:
            case Qt.Key_Enter:
                if (selection.current)
                    root.openItem(selection.current)
                event.accepted = true
                return
            case Qt.Key_Escape:
                selection.clear()
                event.accepted = true
                return
            case Qt.Key_A:
                if (event.modifiers & Qt.ControlModifier) {
                    const f = selection.current ? gridModel.folderOfItem(selection.current) : root.topFolderId
                    if (f)
                        selection.selectFolder(f)
                    event.accepted = true
                }
                return
            case Qt.Key_E:
                if ((event.modifiers & Qt.ControlModifier) && (event.modifiers & Qt.ShiftModifier) && selection.current) {
                    app.revealInExplorer(selection.current)
                    event.accepted = true
                }
                return
            default:
                return
            }
            if (moved)
                root.revealItem(moved)
            event.accepted = true
        }

        delegate: Item {
            id: rowItem
            required property string rowType
            required property var folderId
            required property string folderName
            required property string folderPath
            required property string folderDate
            required property int folderCount
            required property var rowItems
            required property real rowHeight
            required property real rowY

            width: ListView.view.width
            height: rowHeight

            // Sélection au cadre (FR-015) : glisser depuis un espace vide de la grille.
            MouseArea {
                anchors.fill: parent
                z: -1
                preventStealing: true
                property real startX: 0
                property real startY: 0
                property bool additive: false
                onPressed: mouse => {
                    startX = mouse.x
                    startY = rowItem.rowY + mouse.y
                    additive = (mouse.modifiers & Qt.ControlModifier) !== 0
                    const p = mapToItem(list.contentItem, mouse.x, mouse.y)
                    band.originX = p.x
                    band.originY = p.y
                    band.x = p.x
                    band.y = p.y
                    band.width = 0
                    band.height = 0
                    band.visible = true
                    list.forceActiveFocus()
                }
                onPositionChanged: mouse => {
                    const p = mapToItem(list.contentItem, mouse.x, mouse.y)
                    band.x = Math.min(band.originX, p.x)
                    band.y = Math.min(band.originY, p.y)
                    band.width = Math.abs(p.x - band.originX)
                    band.height = Math.abs(p.y - band.originY)
                    selection.setSelection(gridModel.itemsInRect(startX, startY, mouse.x, rowItem.rowY + mouse.y),
                                           additive)
                }
                onReleased: band.visible = false
                onCanceled: band.visible = false
                onClicked: mouse => {
                    const dragged = band.width > 4 || band.height > 4
                    if (!dragged && !(mouse.modifiers & Qt.ControlModifier))
                        selection.clear()
                }
            }

            // En-tête de dossier
            Item {
                visible: rowItem.rowType === "header"
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 4
                    height: 1
                    color: "#3a3a3a"
                }
                RowLayout {
                    anchors.fill: parent
                    anchors.bottomMargin: 6
                    spacing: 12
                    Label {
                        text: rowItem.folderName
                        font.pixelSize: 16
                        font.bold: true
                        elide: Text.ElideRight
                        Layout.maximumWidth: parent.width * 0.6
                        HoverHandler { id: headerHover }
                        ToolTip.visible: headerHover.hovered
                        ToolTip.delay: 600
                        ToolTip.text: rowItem.folderPath
                    }
                    Label {
                        text: rowItem.folderDate
                        color: "#9a9a9a"
                    }
                    Label {
                        text: rowItem.folderCount + (rowItem.folderCount > 1 ? " éléments" : " élément")
                        color: "#7a7a7a"
                    }
                    Item { Layout.fillWidth: true }
                }
            }

            // Rangée de vignettes
            Row {
                visible: rowItem.rowType === "items"
                x: 12
                spacing: gridModel.spacing
                Repeater {
                    model: rowItem.rowType === "items" ? rowItem.rowItems.length : 0
                    delegate: ThumbCell {
                        required property int index
                        cell: rowItem.rowItems[index] || ({})
                        size: gridModel.thumbSize
                        onActivated: id => root.openItem(id)
                        onContextMenu: id => {
                            root.menuItem = id
                            contextMenu.popup()
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        id: band
        parent: list.contentItem
        property real originX: 0
        property real originY: 0
        visible: false
        z: 1000
        color: "#303d8ee0"
        border.color: "#3d8ee0"
        border.width: 1
    }

    Menu {
        id: contextMenu
        MenuItem {
            text: "Ouvrir"
            onTriggered: root.openItem(root.menuItem)
        }
        MenuItem {
            text: "Afficher dans l'Explorateur"
            onTriggered: app.revealInExplorer(root.menuItem)
        }
        MenuItem {
            text: "Ouvrir avec l'application par défaut"
            onTriggered: app.openWithDefaultApp(root.menuItem)
        }
    }

    Label {
        anchors.centerIn: parent
        visible: list.count === 0
        text: app.scanActive ? "Recherche de vos photos…" : "Aucune photo trouvée dans les dossiers surveillés."
        color: "#8a8a8a"
        font.pixelSize: 16
    }
}

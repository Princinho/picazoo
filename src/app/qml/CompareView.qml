import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Comparaison de deux copies côte à côte (FR-012) : zoom et déplacement synchronisés.
Popup {
    id: root
    property var groupId: 0
    property var left: ({})
    property var right: ({})
    property real zoom: 1
    property real panX: 0
    property real panY: 0

    function show(gid, a, b) {
        groupId = gid
        left = a
        right = b
        zoom = 1
        panX = 0
        panY = 0
        open()
    }
    function zoomBy(f) { zoom = Math.max(1, Math.min(12, zoom * f)); if (zoom === 1) { panX = 0; panY = 0 } }

    parent: Overlay.overlay
    x: 0
    y: 0
    width: parent ? parent.width : 1200
    height: parent ? parent.height : 800
    modal: true
    focus: true
    padding: 0
    closePolicy: Popup.CloseOnEscape
    background: Rectangle { color: Theme.viewerBg }

    contentItem: FocusScope {
        focus: true
        Keys.onPressed: event => {
            if (event.key === Qt.Key_0) { root.zoom = 1; root.panX = 0; root.panY = 0 }
            else if (event.key === Qt.Key_Plus || event.key === Qt.Key_Equal) root.zoomBy(1.25)
            else if (event.key === Qt.Key_Minus) root.zoomBy(0.8)
            else return
            event.accepted = true
        }

        component CopyPane: ColumnLayout {
            id: pane
            property var copy: ({})
            spacing: 10
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                Image {
                    id: img
                    width: parent.width
                    height: parent.height
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    source: pane.copy.id ? "image://viewer/" + pane.copy.id + "/" + pane.copy.version + "/screen" : ""
                    sourceSize: Qt.size(2048, 2048)
                    transform: [
                        Scale { origin.x: img.width / 2; origin.y: img.height / 2; xScale: root.zoom; yScale: root.zoom },
                        Translate { x: root.panX; y: root.panY }
                    ]
                }
                MouseArea {
                    anchors.fill: parent
                    property real lastX: 0
                    property real lastY: 0
                    cursorShape: root.zoom > 1 ? (pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor) : Qt.ArrowCursor
                    onPressed: mouse => { lastX = mouse.x; lastY = mouse.y }
                    onPositionChanged: mouse => {
                        if (!pressed || root.zoom <= 1)
                            return
                        root.panX += mouse.x - lastX
                        root.panY += mouse.y - lastY
                        lastX = mouse.x
                        lastY = mouse.y
                    }
                    onWheel: wheel => root.zoomBy(wheel.angleDelta.y > 0 ? 1.15 : 1 / 1.15)
                }
            }
            Label { Layout.fillWidth: true; text: pane.copy.name || ""; color: Theme.text; font.weight: Font.DemiBold; elide: Text.ElideMiddle; horizontalAlignment: Text.AlignHCenter }
            Label {
                Layout.fillWidth: true
                text: (pane.copy.dimensions || "") + " · " + (pane.copy.size || "") + " · " + String(pane.copy.format || "").toUpperCase()
                color: Theme.text2
                horizontalAlignment: Text.AlignHCenter
            }
            Label { Layout.fillWidth: true; text: pane.copy.folderPath || ""; color: Theme.text3; font.pixelSize: 12; elide: Text.ElideMiddle; horizontalAlignment: Text.AlignHCenter }
            Button {
                Layout.alignment: Qt.AlignHCenter
                text: pane.copy.keep ? "Gardée" : "Garder celle-ci"
                enabled: !pane.copy.keep
                onClicked: {
                    dups.keepOnly(root.groupId, pane.copy.id)
                    root.close()
                }
            }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 12
            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: "Comparer deux copies"
                    font.family: Theme.displayFont
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    color: Theme.text
                }
                Label { text: "Molette : zoom des deux images · glisser : déplacer · 0 : ajuster · Échap : fermer"; color: Theme.text3; Layout.leftMargin: 16 }
                Item { Layout.fillWidth: true }
                Button { text: "Fermer"; onClicked: root.close() }
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 16
                CopyPane { Layout.fillWidth: true; Layout.fillHeight: true; copy: root.left }
                Rectangle { Layout.fillHeight: true; Layout.preferredWidth: 1; color: Theme.viewerLine }
                CopyPane { Layout.fillWidth: true; Layout.fillHeight: true; copy: root.right }
            }
        }
    }
}

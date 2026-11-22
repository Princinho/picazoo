import QtQuick
import QtQuick.Controls

// Une vignette de la grille : image, ou remplacement (illisible, aperçu indisponible,
// vidéo en attente), symbole ▶ et durée pour les vidéos, cadre de sélection.
Item {
    id: root
    property var cell: ({})
    property int size: 160
    signal activated(var id)
    signal contextMenu(var id)

    readonly property var itemId: cell.id || 0
    readonly property string state_: cell.state || "pending"
    readonly property bool isVideo: cell.kind === "video"
    readonly property bool selected: selection.revision >= 0 && selection.isSelected(itemId)
    readonly property bool isCurrent: selection.current === itemId

    width: size
    height: size

    Rectangle {
        anchors.fill: parent
        color: "#2a2a2a"
        radius: 2
    }

    Image {
        id: img
        anchors.fill: parent
        anchors.margins: 3
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        cache: false
        retainWhileLoading: true
        smooth: true
        mipmap: root.size < 140
        source: (root.state_ === "done" || root.state_ === "provisional") && root.itemId
                ? "image://thumb/" + root.itemId + "/" + root.cell.version + "/" + root.state_ : ""
    }

    Label {
        visible: root.state_ === "unreadable" || root.state_ === "no_preview"
        anchors.centerIn: parent
        width: parent.width - 12
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        text: root.state_ === "unreadable" ? "image illisible" : "aperçu indisponible"
        color: "#8a8a8a"
        font.pixelSize: 12
    }

    // Vidéo : symbole et durée (FR-013)
    Rectangle {
        visible: root.isVideo
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 6
        radius: 3
        color: "#b0000000"
        width: videoLabel.implicitWidth + 10
        height: videoLabel.implicitHeight + 4
        Label {
            id: videoLabel
            anchors.centerIn: parent
            text: "▶" + (root.cell.duration !== undefined && root.cell.duration !== null
                         ? " " + app.formatDuration(root.cell.duration) : "")
            font.pixelSize: 11
            color: "white"
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.width: root.selected ? 3 : (root.isCurrent ? 1 : 0)
        border.color: root.selected ? "#3d8ee0" : "#6a6a6a"
        radius: 2
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: mouse => {
            if (mouse.button === Qt.RightButton) {
                if (!selection.isSelected(root.itemId))
                    selection.click(root.itemId, 0)
                root.contextMenu(root.itemId)
                return
            }
            const mods = (mouse.modifiers & Qt.ControlModifier) ? 1 : (mouse.modifiers & Qt.ShiftModifier) ? 2 : 0
            selection.click(root.itemId, mods)
        }
        onDoubleClicked: mouse => {
            if (mouse.button === Qt.LeftButton)
                root.activated(root.itemId)
        }
    }
}

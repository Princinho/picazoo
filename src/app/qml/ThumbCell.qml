import QtQuick
import QtQuick.Controls

// Une vignette de la grille : image, ou remplacement (illisible, aperçu indisponible,
// vidéo en attente), badges ▶ durée et RAW, cadre de sélection.
Item {
    id: root
    property var cell: ({})
    property int size: 160
    signal activated(var id)
    signal contextMenu(var id)

    readonly property var itemId: cell.id || 0
    readonly property string state_: cell.state || "pending"
    readonly property bool isVideo: cell.kind === "video"
    readonly property bool isRaw: cell.kind === "raw"
    readonly property bool selected: selection.revision >= 0 && selection.isSelected(itemId)
    readonly property bool isCurrent: selection.current === itemId

    width: size
    height: size

    Rectangle {
        anchors.fill: parent
        radius: 6
        color: Theme.surface
    }

    Image {
        id: img
        anchors.fill: parent
        anchors.margins: 2
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
        width: parent.width - 16
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        text: root.state_ === "unreadable" ? "Image illisible" : "Aperçu indisponible"
        color: Theme.text3
        font.pixelSize: 12
    }

    // Vidéo : symbole et durée (FR-013)
    Rectangle {
        visible: root.isVideo
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 8
        radius: 6
        color: "#C70A0C0F"
        width: videoLabel.implicitWidth + 14
        height: videoLabel.implicitHeight + 6
        Label {
            id: videoLabel
            anchors.centerIn: parent
            text: "▶" + (root.cell.duration !== undefined && root.cell.duration !== null
                         ? "  " + app.formatDuration(root.cell.duration) : "")
            font.pixelSize: 12
            color: "#FFFFFF"
        }
    }

    // RAW
    Rectangle {
        visible: root.isRaw
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 8
        radius: 5
        color: "#C70A0C0F"
        width: rawLabel.implicitWidth + 12
        height: rawLabel.implicitHeight + 4
        Label {
            id: rawLabel
            anchors.centerIn: parent
            text: "RAW"
            font.pixelSize: 11
            font.weight: Font.DemiBold
            font.letterSpacing: 0.5
            color: Theme.amberText
        }
    }

    // Sélection : contour bleu décalé ; élément courant : contour fin.
    Rectangle {
        anchors.fill: parent
        anchors.margins: root.selected ? -4 : 0
        radius: root.selected ? 9 : 6
        color: "transparent"
        border.width: root.selected ? 3 : (root.isCurrent ? 1 : 0)
        border.color: root.selected ? Theme.accent : Theme.text3
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        cursorShape: Qt.PointingHandCursor
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

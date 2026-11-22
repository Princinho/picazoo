import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

// Visionneuse (US2, T054) : ajustement à la fenêtre, zoom, déplacement, précédent /
// suivant à travers les dossiers, plein écran, informations ; vidéos (US3).
FocusScope {
    id: root
    property var startItem: 0
    signal closed(var lastItem)

    readonly property var info: viewer.info
    readonly property var itemId: viewer.current
    readonly property bool isVideo: info.kind === "video"
    readonly property bool isGif: info.format === "gif"
    property bool showInfo: false
    property real zoom: 1          // 1 = 100 % de la pleine résolution
    property bool fitMode: true
    property int prevVisibility: Window.Windowed

    readonly property real fullW: info.width > 0 ? info.width : Math.max(1, preview.implicitWidth)
    readonly property real fullH: info.height > 0 ? info.height : Math.max(1, preview.implicitHeight)
    readonly property real fitZoom: Math.min(1, flick.width / fullW, flick.height / fullH)
    readonly property bool needFull: !isVideo && !isGif && preview.status === Image.Ready
                                     && zoom * fullW > preview.implicitWidth * 1.01
    readonly property bool unsupported: !isVideo && !isGif && preview.status === Image.Error

    function fit() { fitMode = true; zoom = fitZoom }
    function setZoom(z, px, py) {
        const nz = Math.max(Math.min(fitZoom, 0.05), Math.min(8, z))
        const r = nz / zoom
        const cx = flick.contentX + (px === undefined ? flick.width / 2 : px)
        const cy = flick.contentY + (py === undefined ? flick.height / 2 : py)
        fitMode = false
        zoom = nz
        flick.contentX = Math.max(0, cx * r - (px === undefined ? flick.width / 2 : px))
        flick.contentY = Math.max(0, cy * r - (py === undefined ? flick.height / 2 : py))
    }
    function goNext() { viewer.next() }
    function goPrevious() { viewer.previous() }
    function toggleFullScreen() {
        const w = Window.window
        if (w.visibility === Window.FullScreen) {
            w.visibility = root.prevVisibility
        } else {
            root.prevVisibility = w.visibility
            w.visibility = Window.FullScreen
        }
    }
    function close() {
        const w = Window.window
        if (w.visibility === Window.FullScreen)
            w.visibility = root.prevVisibility
        root.closed(viewer.current)
    }

    onFitZoomChanged: if (fitMode) zoom = fitZoom
    onItemIdChanged: fit()
    Component.onCompleted: {
        viewer.open(startItem)
        fit()
    }
    onWidthChanged: viewer.screenSize = Qt.size(Math.round(width * Screen.devicePixelRatio), Math.round(height * Screen.devicePixelRatio))
    onHeightChanged: viewer.screenSize = Qt.size(Math.round(width * Screen.devicePixelRatio), Math.round(height * Screen.devicePixelRatio))

    Keys.onPressed: event => {
        const ctrl = (event.modifiers & Qt.ControlModifier) !== 0
        switch (event.key) {
        case Qt.Key_Left: root.goPrevious(); break
        case Qt.Key_Right: root.goNext(); break
        case Qt.Key_Escape: root.close(); break
        case Qt.Key_0: root.fit(); break
        case Qt.Key_1: root.setZoom(1); break
        case Qt.Key_F11:
        case Qt.Key_F: root.toggleFullScreen(); break
        case Qt.Key_I: root.showInfo = !root.showInfo; break
        case Qt.Key_Plus:
        case Qt.Key_Equal: if (ctrl) root.setZoom(root.zoom * 1.25); else return; break
        case Qt.Key_Minus: if (ctrl) root.setZoom(root.zoom / 1.25); else return; break
        case Qt.Key_E:
            if (ctrl && (event.modifiers & Qt.ShiftModifier)) app.revealInExplorer(viewer.current)
            else return
            break
        case Qt.Key_Space:
            if (videoLoader.item) videoLoader.item.togglePlay()
            break
        default: return
        }
        event.accepted = true
    }

    Rectangle {
        anchors.fill: parent
        color: "#101010"
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Flickable {
                id: flick
                anchors.fill: parent
                anchors.bottomMargin: toolbar.height
                visible: !root.isVideo
                clip: true
                contentWidth: Math.max(canvas.width, width)
                contentHeight: Math.max(canvas.height, height)
                boundsBehavior: Flickable.StopAtBounds
                interactive: canvas.width > width || canvas.height > height

                Item {
                    id: canvas
                    width: root.fullW * root.zoom
                    height: root.fullH * root.zoom
                    x: (flick.contentWidth - width) / 2
                    y: (flick.contentHeight - height) / 2

                    // Vignette immédiate, en attendant l'aperçu (pas d'écran noir).
                    Image {
                        anchors.fill: parent
                        visible: preview.status !== Image.Ready && !root.isGif
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                        cache: false
                        source: root.itemId ? "image://thumb/" + root.itemId + "/" + root.info.version + "/done" : ""
                    }
                    Image {
                        id: preview
                        anchors.fill: parent
                        visible: !root.isGif && !(full.status === Image.Ready && full.visible)
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                        cache: false
                        smooth: true
                        mipmap: true
                        sourceSize: viewer.screenSize
                        source: root.itemId && !root.isVideo && !root.isGif
                                ? "image://viewer/" + root.itemId + "/" + root.info.version + "/screen" : ""
                    }
                    Image {
                        id: full
                        anchors.fill: parent
                        visible: status === Image.Ready && root.needFull
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                        cache: false
                        smooth: true
                        source: root.needFull ? "image://viewer/" + root.itemId + "/" + root.info.version + "/full" : ""
                    }
                    AnimatedImage {
                        anchors.fill: parent
                        visible: root.isGif
                        fillMode: Image.PreserveAspectFit
                        source: root.isGif ? root.info.url : ""
                        playing: visible
                    }
                }

                WheelHandler {
                    onWheel: event => root.setZoom(root.zoom * (event.angleDelta.y > 0 ? 1.2 : 1 / 1.2),
                                                   point.position.x, point.position.y)
                }
            }

            BusyIndicator {
                anchors.centerIn: flick
                running: !root.isVideo && !root.isGif && preview.status === Image.Loading
            }

            // Élément non affichable (FR-022)
            ColumnLayout {
                anchors.centerIn: flick
                visible: root.unsupported
                spacing: 12
                Label {
                    text: "Picazoo ne sait pas afficher ce fichier."
                    font.pixelSize: 18
                    Layout.alignment: Qt.AlignHCenter
                }
                Button {
                    text: "Ouvrir avec l'application par défaut"
                    Layout.alignment: Qt.AlignHCenter
                    onClicked: app.openWithDefaultApp(viewer.current)
                }
            }

            Loader {
                id: videoLoader
                anchors.fill: parent
                anchors.bottomMargin: toolbar.height
                active: root.isVideo
                sourceComponent: VideoPlayer {
                    source: root.info.url
                    itemId: root.itemId
                }
            }

            // Boutons précédent / suivant à l'écran
            RoundButton {
                anchors.left: parent.left
                anchors.leftMargin: 12
                anchors.verticalCenter: flick.verticalCenter
                visible: viewer.hasPrevious
                text: "‹"
                font.pixelSize: 26
                opacity: hovered ? 0.95 : 0.45
                onClicked: root.goPrevious()
            }
            RoundButton {
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.verticalCenter: flick.verticalCenter
                visible: viewer.hasNext
                text: "›"
                font.pixelSize: 26
                opacity: hovered ? 0.95 : 0.45
                onClicked: root.goNext()
            }

            // Barre d'outils
            Rectangle {
                id: toolbar
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 48
                color: "#1c1c1c"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 4

                    Button {
                        text: "Retour à la bibliothèque"
                        onClicked: root.close()
                    }
                    ToolButton { text: "◀"; enabled: viewer.hasPrevious; onClicked: root.goPrevious(); ToolTip.visible: hovered; ToolTip.text: "Précédente (←)" }
                    ToolButton { text: "▶"; enabled: viewer.hasNext; onClicked: root.goNext(); ToolTip.visible: hovered; ToolTip.text: "Suivante (→)" }
                    Label {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideMiddle
                        text: (root.info.name || "") + "    " + viewer.position
                        color: "#c0c0c0"
                    }
                    ToolButton { text: "−"; visible: !root.isVideo; onClicked: root.setZoom(root.zoom / 1.25); ToolTip.visible: hovered; ToolTip.text: "Zoom arrière (Ctrl −)" }
                    Label {
                        visible: !root.isVideo
                        text: Math.round(root.zoom * 100) + " %"
                        color: "#a0a0a0"
                        Layout.preferredWidth: 52
                        horizontalAlignment: Text.AlignHCenter
                    }
                    ToolButton { text: "+"; visible: !root.isVideo; onClicked: root.setZoom(root.zoom * 1.25); ToolTip.visible: hovered; ToolTip.text: "Zoom avant (Ctrl +)" }
                    ToolButton { text: "Ajuster"; visible: !root.isVideo; onClicked: root.fit(); ToolTip.visible: hovered; ToolTip.text: "Ajuster à la fenêtre (0)" }
                    ToolButton { text: "100 %"; visible: !root.isVideo; onClicked: root.setZoom(1); ToolTip.visible: hovered; ToolTip.text: "Taille réelle (1)" }
                    ToolButton { text: "Plein écran"; onClicked: root.toggleFullScreen(); ToolTip.visible: hovered; ToolTip.text: "Plein écran (F11)" }
                    ToolButton { text: "Infos"; checkable: true; checked: root.showInfo; onClicked: root.showInfo = !root.showInfo; ToolTip.visible: hovered; ToolTip.text: "Informations (I)" }
                    ToolButton { text: "Explorateur"; onClicked: app.revealInExplorer(viewer.current); ToolTip.visible: hovered; ToolTip.text: "Afficher dans l'Explorateur (Ctrl+Maj+E)" }
                }
            }
        }

        InfoPanel {
            visible: root.showInfo
            Layout.preferredWidth: 300
            Layout.fillHeight: true
            info: root.info
        }
    }
}

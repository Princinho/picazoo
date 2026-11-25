import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import Picazoo.Zoom

// Visionneuse (US2) : ajustement à la fenêtre, zoom, déplacement, précédent / suivant à
// travers les dossiers, plein écran, informations, bande des voisines ; vidéos (US3).
FocusScope {
    id: root
    property var startItem: 0
    signal closed(var lastItem)

    readonly property var info: viewer.info
    readonly property var itemId: viewer.current
    readonly property bool isVideo: info.kind === "video"
    readonly property bool isGif: info.format === "gif"
    property bool showInfo: true
    // 007 : bouton « Quitter le plein écran » affiché (mouvement de la souris) ; menu ouvert.
    property bool exitShown: false
    property bool menuOpen: false
    property int prevVisibility: Window.Windowed
    readonly property bool fullScreen: Window.window && Window.window.visibility === Window.FullScreen

    readonly property real fullW: info.width > 0 ? info.width : Math.max(1, preview.implicitWidth)
    readonly property real fullH: info.height > 0 ? info.height : Math.max(1, preview.implicitHeight)
    readonly property real stageMargin: fullScreen ? 0 : 24
    // Zoom (007, core/browse/ZoomMotion) : 1 = 100 % de la pleine résolution.
    readonly property real zoom: zc.scale
    readonly property bool fitMode: zc.fitMode
    readonly property bool canPan: zc.canPan
    // Décodage de la pleine résolution dès que la cible la demande (pas au milieu du mouvement).
    readonly property bool needFull: !isVideo && !isGif && preview.status === Image.Ready
                                     && Math.max(zc.scale, zc.target) * fullW > preview.implicitWidth * 1.01
    readonly property bool unsupported: !isVideo && !isGif && preview.status === Image.Error

    ZoomController {
        id: zc
        imageWidth: root.fullW
        imageHeight: root.fullH
        viewWidth: stage.width
        viewHeight: stage.height
        margin: root.stageMargin
        immediate: !viewer.animations
    }
    FrameAnimation {
        running: zc.moving
        onTriggered: zc.advance(frameTime)
    }

    // Retour immédiat à l'ajustement (changement d'élément).
    function resetView() { zc.reset() }
    // Pas de zoom au centre (clavier : `autoRepeat` fait d'un appui maintenu un seul geste).
    function zoomStep(direction, source, autoRepeat) {
        if (isVideo)
            return
        zc.stepCentered(direction, source, autoRepeat === true)
    }
    function fit() { zc.fit() }
    // 007 : menu de l'image ; (x, y) < 0 : au centre (touche Menu, Maj+F10).
    function openContextMenu(x, y) {
        zc.stop()
        if (x < 0 || y < 0)
            imageMenu.openAt(stageArea, stageArea.width / 2, stageArea.height / 2)
        else
            imageMenu.openAt(stageArea, x, y)
    }
    function setZoom(z) { zc.setTarget(z, stage.width / 2, stage.height / 2) }
    function goNext() { viewer.next() }
    function goPrevious() { viewer.previous() }
    function toggleFullScreen() {
        const w = Window.window
        if (w.visibility === Window.FullScreen) {
            w.visibility = root.prevVisibility
        } else {
            root.prevVisibility = w.visibility
            w.visibility = Window.FullScreen
            // Au passage en plein écran, montrer brièvement comment en sortir.
            root.exitShown = true
            exitTimer.restart()
        }
    }
    function close() {
        const w = Window.window
        if (w.visibility === Window.FullScreen)
            w.visibility = root.prevVisibility
        root.closed(viewer.current)
    }

    onItemIdChanged: resetView()
    Connections {
        target: viewer
        // 007 : la dernière photo a été mise à la corbeille.
        function onEmptied() { root.close() }
    }
    ImageContextMenu {
        id: imageMenu
        viewerRoot: root
        onAboutToShow: root.menuOpen = true
        onAboutToHide: root.menuOpen = false
    }
    Component.onCompleted: {
        viewer.open(startItem)
        resetView()
    }

    onWidthChanged: viewer.screenSize = Qt.size(Math.round(width * Screen.devicePixelRatio), Math.round(height * Screen.devicePixelRatio))
    onHeightChanged: viewer.screenSize = Qt.size(Math.round(width * Screen.devicePixelRatio), Math.round(height * Screen.devicePixelRatio))

    Keys.onPressed: event => {
        const ctrl = (event.modifiers & Qt.ControlModifier) !== 0
        switch (event.key) {
        case Qt.Key_Left: root.goPrevious(); break
        case Qt.Key_Right: root.goNext(); break
        case Qt.Key_Up: root.zoomStep(+1, ZoomController.Key, event.isAutoRepeat); break
        case Qt.Key_Down: root.zoomStep(-1, ZoomController.Key, event.isAutoRepeat); break
        case Qt.Key_Escape: root.close(); break
        case Qt.Key_0: root.fit(); break
        case Qt.Key_1: root.setZoom(1); break
        case Qt.Key_F11:
        case Qt.Key_F: root.toggleFullScreen(); break
        case Qt.Key_I: root.showInfo = !root.showInfo; break
        case Qt.Key_Plus:
        case Qt.Key_Equal: if (ctrl) root.zoomStep(+1, ZoomController.Key, event.isAutoRepeat); else return; break
        case Qt.Key_Minus: if (ctrl) root.zoomStep(-1, ZoomController.Key, event.isAutoRepeat); else return; break
        case Qt.Key_E:
            if (ctrl && (event.modifiers & Qt.ShiftModifier)) app.revealInExplorer(viewer.current)
            else return
            break
        case Qt.Key_Space:
            if (videoLoader.item) videoLoader.item.togglePlay()
            break
        // 007 : menus contextuels et leurs raccourcis (contracts/ui-contract.md).
        case Qt.Key_Menu: root.openContextMenu(-1, -1); break
        case Qt.Key_F10:
            if (event.modifiers & Qt.ShiftModifier) root.openContextMenu(-1, -1)
            else return
            break
        case Qt.Key_Delete: trash.prepareItems([viewer.current]); break
        case Qt.Key_C:
            if (!ctrl) return
            if (event.modifiers & Qt.ShiftModifier) app.copyPaths([viewer.current])
            else app.copyItems([viewer.current])
            break
        case Qt.Key_Return:
        case Qt.Key_Enter:
            if (event.modifiers & Qt.AltModifier) app.showProperties([viewer.current])
            else return
            break
        default: return
        }
        event.accepted = true
    }

    // Boutons de la visionneuse
    component IconButton: ToolButton {
        id: ib
        property string glyph
        property bool active: false
        implicitWidth: 44
        implicitHeight: 44
        focusPolicy: Qt.NoFocus
        ToolTip.visible: hovered
        ToolTip.delay: 500
        ToolTip.text: Accessible.name
        contentItem: Label {
            text: ib.glyph
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            font.family: Theme.iconFont
            font.pixelSize: 17
            color: ib.active ? Theme.accentText : Theme.textSoft
        }
        background: Rectangle {
            radius: 10
            color: ib.active ? Theme.accentBg : (ib.hovered ? Theme.hover : "transparent")
        }
    }
    component TextButton: ToolButton {
        id: tb
        property bool active: false
        implicitHeight: 40
        leftPadding: 14
        rightPadding: 14
        focusPolicy: Qt.NoFocus
        contentItem: Label {
            text: tb.text
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            color: tb.active ? Theme.accentText : Theme.text
            font.pixelSize: tb.font.pixelSize
        }
        background: Rectangle {
            radius: 8
            color: tb.active ? Theme.accentBg : (tb.hovered ? Theme.hover : "#1A1D23")
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.viewerBg
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // En-tête
        Rectangle {
            visible: !root.fullScreen
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            color: Theme.viewerPanel
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.viewerLine }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 16
                Button {
                    id: backBtn
                    focusPolicy: Qt.NoFocus
                    implicitHeight: 42
                    leftPadding: 14
                    rightPadding: 16
                    onClicked: root.close()
                    contentItem: Row { spacing: 10; Label { text: Theme.iconBack; font.family: Theme.iconFont; font.pixelSize: 14; color: Theme.text; anchors.verticalCenter: parent.verticalCenter } Label { text: "Bibliothèque"; color: Theme.text; anchors.verticalCenter: parent.verticalCenter } }
                    background: Rectangle {
                        radius: 10
                        color: backBtn.hovered ? Theme.hover : "#1A1D23"
                        border.color: Theme.lineStrong
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: "Retour à la bibliothèque (Échap)"
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1
                    Label {
                        Layout.fillWidth: true
                        text: root.info.name || ""
                        font.weight: Font.DemiBold
                        color: Theme.text
                        elide: Text.ElideMiddle
                    }
                    Label {
                        text: viewer.folderLabel
                        font.pixelSize: 12
                        color: Theme.text2
                    }
                }
                IconButton { glyph: Theme.iconFolder; Accessible.name: "Afficher dans l'Explorateur (Ctrl+Maj+E)"; onClicked: app.revealInExplorer(viewer.current) }
                IconButton { glyph: Theme.iconFullScreen; Accessible.name: "Plein écran (F11)"; onClicked: root.toggleFullScreen() }
                IconButton { glyph: Theme.iconInfo; active: root.showInfo; Accessible.name: "Informations (I)"; onClicked: root.showInfo = !root.showInfo }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Item {
                id: stageArea
                Layout.fillWidth: true
                Layout.fillHeight: true

                // 007 : clic droit = menu de l'image (photo ou vidéo) ; ne zoome ni ne déplace.
                TapHandler {
                    acceptedButtons: Qt.RightButton
                    onTapped: eventPoint => root.openContextMenu(eventPoint.position.x, eventPoint.position.y)
                }

                // Scène : l'image y est placée en (ox, oy) à l'échelle `zoom` ; molette, pincement
                // et double-clic zooment autour du curseur, le glisser déplace l'image agrandie.
                Item {
                    id: stage
                    anchors.fill: parent
                    visible: !root.isVideo
                    clip: true

                    Item {
                        id: canvas
                        x: zc.offsetX
                        y: zc.offsetY
                        width: root.fullW * zc.scale
                        height: root.fullH * zc.scale

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
                        onWheel: event => {
                            if (!root.isVideo)
                                zc.wheel(event.angleDelta.y, point.position.x, point.position.y)
                        }
                    }

                    PinchHandler {
                        target: null
                        onActiveChanged: if (active) zc.pinchBegin(); else zc.pinchEnd()
                        onActiveScaleChanged: if (active)
                            zc.pinchUpdate(activeScale, centroid.position.x, centroid.position.y)
                    }

                    DragHandler {
                        target: null
                        enabled: root.canPan
                        acceptedButtons: Qt.LeftButton
                        cursorShape: active ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                        property real startX: 0
                        property real startY: 0
                        // Sans élan : l'image s'arrête net quand on la lâche (FR-047).
                        onActiveChanged: if (active) {
                            zc.stop()
                            startX = zc.offsetX
                            startY = zc.offsetY
                        }
                        onTranslationChanged: if (active) zc.panTo(startX + translation.x, startY + translation.y)
                    }

                    HoverHandler {
                        cursorShape: root.canPan ? Qt.OpenHandCursor : Qt.ArrowCursor
                    }

                    TapHandler {
                        acceptedButtons: Qt.LeftButton
                        // Un clic arrête l'élan du zoom (FR-042).
                        onPressedChanged: if (pressed && zc.moving) zc.stop()
                        onDoubleTapped: (eventPoint, button) => zc.toggleAt(eventPoint.position.x, eventPoint.position.y)
                    }
                }

                BusyIndicator {
                    anchors.centerIn: stage
                    running: !root.isVideo && !root.isGif && preview.status === Image.Loading
                }

                // Élément non affichable (FR-022)
                ColumnLayout {
                    anchors.centerIn: stage
                    visible: root.unsupported
                    spacing: 14
                    Label {
                        text: "Picazoo ne sait pas afficher ce fichier."
                        font.pixelSize: 18
                        color: Theme.text
                        Layout.alignment: Qt.AlignHCenter
                    }
                    TextButton {
                        text: "Ouvrir avec l'application par défaut"
                        Layout.alignment: Qt.AlignHCenter
                        onClicked: app.openWithDefaultApp(viewer.current)
                    }
                }

                Loader {
                    id: videoLoader
                    anchors.fill: parent
                    anchors.margins: root.fullScreen ? 0 : 24
                    active: root.isVideo
                    sourceComponent: VideoPlayer {
                        source: root.info.url
                        itemId: root.itemId
                    }
                }

                // Précédente / suivante
                RoundButton {
                    id: prevBtn
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    visible: viewer.hasPrevious
                    width: 48
                    height: 48
                    focusPolicy: Qt.NoFocus
                    Accessible.name: "Photo précédente (←)"
                    onClicked: root.goPrevious()
                    contentItem: Label { text: Theme.iconPrev; font.family: Theme.iconFont; font.pixelSize: 18; color: Theme.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { radius: 24; color: "#D9121418"; border.color: Theme.lineStrong; opacity: prevBtn.hovered ? 1 : 0.8 }
                }
                RoundButton {
                    id: nextBtn
                    anchors.right: parent.right
                    anchors.rightMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    visible: viewer.hasNext
                    width: 48
                    height: 48
                    focusPolicy: Qt.NoFocus
                    Accessible.name: "Photo suivante (→)"
                    onClicked: root.goNext()
                    contentItem: Label { text: Theme.iconNext; font.family: Theme.iconFont; font.pixelSize: 18; color: Theme.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { radius: 24; color: "#D9121418"; border.color: Theme.lineStrong; opacity: nextBtn.hovered ? 1 : 0.8 }
                }

                // 007 (US1) : en plein écran, rien d'autre ne permet d'en sortir à la souris. Le
                // bouton apparaît au moindre mouvement et s'efface après 2 s d'immobilité.
                HoverHandler {
                    id: fullScreenHover
                    enabled: root.fullScreen
                    onPointChanged: if (root.fullScreen) { root.exitShown = true; exitTimer.restart() }
                }
                Timer {
                    id: exitTimer
                    interval: 2000
                    onTriggered: {
                        if (exitBtn.hovered || root.menuOpen)
                            restart()
                        else
                            root.exitShown = false
                    }
                }
                RoundButton {
                    id: exitBtn
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.margins: 16
                    z: 10
                    width: 48
                    height: 48
                    opacity: root.fullScreen && root.exitShown ? 1 : 0
                    visible: opacity > 0
                    Behavior on opacity { NumberAnimation { duration: 250 } }
                    focusPolicy: Qt.NoFocus
                    Accessible.name: "Quitter le plein écran (F11)"
                    ToolTip.visible: hovered
                    ToolTip.delay: 500
                    ToolTip.text: Accessible.name
                    onClicked: {
                        root.exitShown = false
                        root.toggleFullScreen()
                    }
                    contentItem: Label { text: Theme.iconExitFullScreen; font.family: Theme.iconFont; font.pixelSize: 18; color: Theme.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { radius: 24; color: "#D9121418"; border.color: Theme.lineStrong; opacity: exitBtn.hovered ? 1 : 0.85 }
                }
            }

            InfoPanel {
                visible: root.showInfo && !root.fullScreen
                Layout.preferredWidth: 320
                Layout.fillHeight: true
                info: root.info
            }
        }

        // Pied : zoom et bande des voisines
        Rectangle {
            visible: !root.fullScreen
            Layout.fillWidth: true
            Layout.preferredHeight: 136
            color: Theme.viewerPanel
            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Theme.viewerLine }

            ColumnLayout {
                anchors.fill: parent
                anchors.topMargin: 10
                anchors.bottomMargin: 12
                spacing: 10

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 6
                    visible: !root.isVideo
                    TextButton { text: "−"; font.pixelSize: 18; implicitWidth: 44; Accessible.name: "Zoom arrière (↓, Ctrl −)"; ToolTip.visible: hovered; ToolTip.delay: 500; ToolTip.text: Accessible.name; onClicked: root.zoomStep(-1, ZoomController.Button, false) }
                    Label {
                        Layout.preferredWidth: 64
                        horizontalAlignment: Text.AlignHCenter
                        text: Math.round(zc.percent) + " %"
                        color: Theme.textSoft
                    }
                    TextButton { text: "+"; font.pixelSize: 18; implicitWidth: 44; Accessible.name: "Zoom avant (↑, Ctrl +)"; ToolTip.visible: hovered; ToolTip.delay: 500; ToolTip.text: Accessible.name; onClicked: root.zoomStep(+1, ZoomController.Button, false) }
                    TextButton { text: "Ajuster"; active: root.fitMode; onClicked: root.fit() }
                    TextButton { text: "100 %"; onClicked: root.setZoom(1) }
                }

                ListView {
                    id: strip
                    Layout.fillWidth: true
                    Layout.preferredHeight: 62
                    orientation: ListView.Horizontal
                    spacing: 6
                    interactive: false
                    model: viewer.strip
                    // centrée dans la largeur disponible
                    leftMargin: Math.max(0, (width - contentWidth) / 2)
                    delegate: Item {
                        id: tile
                        required property var modelData
                        width: 76
                        height: 58
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: 2
                            radius: 6
                            color: Theme.surface
                            opacity: tile.modelData.current ? 1 : (tileMouse.containsMouse ? 0.95 : 0.7)
                            clip: true
                            Image {
                                anchors.fill: parent
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                                cache: false
                                source: tile.modelData.thumb
                                sourceSize: Qt.size(144, 108)
                            }
                            Label {
                                visible: tile.modelData.video
                                anchors.centerIn: parent
                                text: "▶"
                                color: "#FFFFFF"
                            }
                        }
                        Rectangle {
                            visible: tile.modelData.current
                            anchors.fill: parent
                            anchors.margins: -2
                            radius: 8
                            color: "transparent"
                            border.width: 3
                            border.color: Theme.accent
                        }
                        MouseArea {
                            id: tileMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: viewer.open(tile.modelData.id)
                        }
                    }
                }
            }
        }
    }
}

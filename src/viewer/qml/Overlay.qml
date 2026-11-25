import QtQuick
import QtQuick.Window
import Picazoo.Zoom

// Visionneuse autonome (005) : voile semi-transparent posé sur le bureau, comme la
// Visionneuse de photos Picasa (contracts/ui-contract.md).
Window {
    id: win
    visible: false
    flags: Qt.Window | Qt.FramelessWindowHint
    color: "transparent"
    // Écran, géométrie et affichage plein écran : fixés par main.cpp (écran de la souris).
    title: session.currentName ? session.currentName + " – Picazoo" : "Picazoo"

    readonly property bool opaque: session.opaque || !session.transparencyAvailable
    property bool controlsShown: true
    property bool closing: false
    property bool showInfo: false
    readonly property bool keepControls: session.proposalVisible
                                         || (chromeLoader.item !== null && (chromeLoader.item.popupOpen || chromeLoader.item.hovering))

    function wake() {
        controlsShown = true
        hideTimer.restart()
    }
    function closeViewer() {
        if (closing)
            return
        closing = true
        fadeOut.start()
    }
    function toast(text) {
        if (chromeLoader.item)
            chromeLoader.item.toast(text)
    }
    // 007 : menu de l'image et mise à la corbeille (contracts/ui-contract.md).
    function openContextMenu(x, y) {
        if (chromeLoader.item) {
            wake()
            chromeLoader.item.openImageMenu(x, y)
        }
    }
    function loadChrome() {
        if (chromeLoader.status === Loader.Null)
            chromeLoader.setSource("Chrome.qml", { win: win, stage: stage })
    }

    Component.onCompleted: {
        fadeIn.start()
        root.forceActiveFocus()
        stage.show(session.currentPath, 0)
        hideTimer.start()
    }

    Connections {
        target: session
        function onCurrentChanged() { stage.show(session.currentPath, session.direction) }
        function onWrapped(forward) { win.toast(forward ? "Retour à la première photo" : "Dernière photo du dossier") }
        function onCurrentRemoved() { win.toast("Cette photo a été supprimée ou déplacée.") }
        function onMessage(text) { win.toast(text) }
        function onQuitRequested() { win.closeViewer() }
    }

    NumberAnimation {
        id: fadeIn
        target: root
        property: "opacity"
        from: 0
        to: 1
        duration: session.animations ? 150 : 0
    }
    SequentialAnimation {
        id: fadeOut
        NumberAnimation { target: root; property: "opacity"; to: 0; duration: session.animations ? 150 : 0 }
        ScriptAction { script: Qt.quit() }
    }
    Timer {
        id: hideTimer
        interval: 2500
        onTriggered: if (!win.keepControls) win.controlsShown = false; else restart()
    }

    Item {
        id: root
        anchors.fill: parent
        opacity: 0
        focus: true

        Keys.onPressed: event => {
            const ctrl = (event.modifiers & Qt.ControlModifier) !== 0
            const shift = (event.modifiers & Qt.ShiftModifier) !== 0
            switch (event.key) {
            case Qt.Key_Right: session.next(); break
            case Qt.Key_Left: session.previous(); break
            case Qt.Key_Home: session.first(); break
            case Qt.Key_End: session.last(); break
            case Qt.Key_Escape:
                if (chromeLoader.item && chromeLoader.item.closePopups()) {}
                else if (session.proposalVisible) session.answerProposal("later")
                else win.closeViewer()
                break
            case Qt.Key_F11: session.opaque = !session.opaque; break
            case Qt.Key_I: win.showInfo = !win.showInfo; break
            case Qt.Key_Plus:
            case Qt.Key_Equal: if (ctrl) stage.zoomStep(+1, ZoomController.Key, event.isAutoRepeat); else return; break
            case Qt.Key_Minus: if (ctrl) stage.zoomStep(-1, ZoomController.Key, event.isAutoRepeat); else return; break
            // 007 : flèches haut / bas = zoom d'un cran au centre (sans effet hors photo).
            case Qt.Key_Up: stage.zoomStep(+1, ZoomController.Key, event.isAutoRepeat); break
            case Qt.Key_Down: stage.zoomStep(-1, ZoomController.Key, event.isAutoRepeat); break
            case Qt.Key_Menu: win.openContextMenu(-1, -1); break
            case Qt.Key_F10: if (shift) win.openContextMenu(-1, -1); else return; break
            case Qt.Key_Delete: if (chromeLoader.item) chromeLoader.item.askTrash(); break
            case Qt.Key_C:
                if (!ctrl) return
                if (shift) session.copyPath(); else session.copyImage()
                break
            case Qt.Key_Z: if (ctrl && chromeLoader.item && chromeLoader.item.undoToast()) {} else return; break
            case Qt.Key_Return:
            case Qt.Key_Enter:
                if (event.modifiers & Qt.AltModifier) session.showProperties(); else return
                break
            case Qt.Key_0: if (ctrl) stage.fit(); else return; break
            case Qt.Key_E: if (ctrl && shift) session.revealInExplorer(); else return; break
            default: return
            }
            win.wake()
            event.accepted = true
        }

        HoverHandler {
            onPointChanged: win.wake()
        }

        // Le voile
        Rectangle {
            anchors.fill: parent
            color: "#000000"
            opacity: win.opaque ? 1 : Theme.veilOpacity
            Behavior on opacity { NumberAnimation { duration: session.animations ? 200 : 0 } }
        }

        Stage {
            id: stage
            anchors.fill: parent
            margin: win.opaque ? 0 : 48
            onContextMenuRequested: (x, y) => win.openContextMenu(x, y)
            Behavior on margin { NumberAnimation { duration: session.animations ? 200 : 0; easing.type: Easing.OutCubic } }
            onFirstShown: {
                session.firstImageShown()
                win.loadChrome()
            }
        }

        Loader {
            id: chromeLoader
            anchors.fill: parent
            asynchronous: true
        }
        Timer {
            // Photo illisible ou lente : les commandes arrivent quand même.
            interval: 600
            running: true
            onTriggered: win.loadChrome()
        }
    }
}

import QtQuick

// Scène de la visionneuse autonome : quatre emplacements d'image recyclés pour des
// transitions nettes (research V9) et le zoom ancré sur le curseur (research V10).
Item {
    id: stage

    property real margin: 48
    readonly property bool animations: session.animations
    property int currentSlot: -1
    property int outgoingSlot: -1
    property int pendingSlot: -1
    property int pendingDir: 0
    property int useCounter: 0
    readonly property var current: currentSlot >= 0 ? slots.itemAt(currentSlot) : null
    readonly property bool currentReady: current !== null && current.ready
    readonly property bool currentFailed: current !== null && current.failed
    readonly property bool zoomed: current !== null && !current.fitMode
    readonly property bool canPan: current !== null && (current.fw * current.s > width + 0.5 || current.fh * current.s > height + 0.5)
    readonly property real zoomPercent: current ? current.s * 100 : 100
    signal firstShown()
    property bool shownOnce: false

    function slotHolding(path) {
        for (let i = 0; i < slots.count; ++i)
            if (slots.itemAt(i).path === path)
                return i
        return -1
    }
    function freeSlot() {
        let best = -1, bestUse = Number.MAX_VALUE
        for (let i = 0; i < slots.count; ++i) {
            if (i === currentSlot || i === outgoingSlot || i === pendingSlot)
                continue
            const s = slots.itemAt(i)
            if (s.lastUse < bestUse) { best = i; bestUse = s.lastUse }
        }
        return best
    }

    // Affiche `path` ; `dir` : +1 suivante, -1 précédente, 0 sans glissement.
    function show(path, dir) {
        if (!path)
            return
        finishTransition()
        let idx = slotHolding(path)
        if (idx >= 0 && idx === currentSlot) {
            pendingSlot = -1
            waitTimer.stop()
            return
        }
        if (idx < 0) {
            idx = freeSlot()
            if (idx < 0)
                return
            slots.itemAt(idx).path = path
        }
        const s = slots.itemAt(idx)
        s.lastUse = ++useCounter
        s.resetView()
        pendingSlot = idx
        pendingDir = dir
        if (s.ready || s.failed)
            startTransition()
        else if (!waitTimer.running)
            waitTimer.start()
    }

    function startTransition() {
        waitTimer.stop()
        if (pendingSlot < 0)
            return
        const inc = slots.itemAt(pendingSlot)
        const out = currentSlot >= 0 ? slots.itemAt(currentSlot) : null
        outgoingSlot = currentSlot
        currentSlot = pendingSlot
        pendingSlot = -1
        inc.z = 2
        if (out)
            out.z = 1
        transition.stop()
        if (stage.animations) {
            inFade.target = inc
            inShift.target = inc
            inc.opacity = 0
            inc.shift = out ? pendingDir * 40 : 0
            inFade.duration = out ? 250 : 150
            inShift.duration = inFade.duration
            outFade.target = out ? out : dummy
            outShift.target = out ? out : dummy
            outShift.to = -pendingDir * 40
            transition.start()
        } else {
            inc.opacity = 1
            inc.shift = 0
            endTransition()
        }
    }
    function finishTransition() {
        if (transition.running)
            transition.complete()
    }
    function endTransition() {
        if (outgoingSlot >= 0) {
            const o = slots.itemAt(outgoingSlot)
            o.opacity = 0
            o.shift = 0
            o.z = 0
        }
        outgoingSlot = -1
        if (current) {
            current.opacity = 1
            current.shift = 0
        }
        prefetch()
    }
    // Voisines préparées dans les emplacements libres (texture prête pour la transition).
    function prefetch() {
        const wanted = [session.pathAt(1), session.pathAt(-1)]
        for (const p of wanted) {
            if (!p || slotHolding(p) >= 0)
                continue
            const i = freeSlot()
            if (i < 0)
                continue
            const s = slots.itemAt(i)
            s.path = p
            s.lastUse = ++useCounter
            s.resetView()
        }
    }
    function noteShown() {
        if (!shownOnce && currentReady) {
            shownOnce = true
            firstShown()
        }
    }

    // Commandes de zoom (clavier, boutons)
    function zoomBy(factor) {
        if (current && current.ready)
            current.zoomTo((current.zoomAnim.running ? current.zoomAnim.to : current.s) * factor, width / 2, height / 2, true)
    }
    function fit() { if (current) current.zoomTo(current.fitS, width / 2, height / 2, true) }
    function actualSize() { if (current) current.zoomTo(1, width / 2, height / 2, true) }

    onCurrentReadyChanged: noteShown()

    QtObject { id: dummy; property real opacity: 0; property real shift: 0 }

    Timer {
        id: waitTimer
        interval: 120
        onTriggered: stage.startTransition()
    }

    ParallelAnimation {
        id: transition
        NumberAnimation { id: inFade; property: "opacity"; to: 1; duration: 250; easing.type: Easing.OutCubic }
        NumberAnimation { id: inShift; property: "shift"; to: 0; duration: 250; easing.type: Easing.OutCubic }
        NumberAnimation { id: outFade; property: "opacity"; to: 0; duration: 250; easing.type: Easing.OutCubic }
        NumberAnimation { id: outShift; property: "shift"; duration: 250; easing.type: Easing.OutCubic }
        onRunningChanged: if (!running) stage.endTransition()
    }

    Repeater {
        id: slots
        model: 4
        delegate: Item {
            id: slot
            required property int index
            property string path: ""
            property int lastUse: 0
            property real shift: 0
            width: stage.width
            height: stage.height
            opacity: 0
            visible: opacity > 0.001

            readonly property string kind: path ? session.kindOf(path) : ""
            readonly property bool isGif: kind === "gif"
            readonly property bool ready: isGif ? gif.status === AnimatedImage.Ready : preview.status === Image.Ready
            readonly property bool failed: kind === "unsupported" || kind === "video"
                                           || (isGif ? gif.status === AnimatedImage.Error : preview.status === Image.Error)
            property size fullSize: Qt.size(0, 0)
            readonly property real fw: fullSize.width > 0 ? fullSize.width
                                                          : Math.max(1, isGif ? gif.implicitWidth : preview.implicitWidth)
            readonly property real fh: fullSize.height > 0 ? fullSize.height
                                                           : Math.max(1, isGif ? gif.implicitHeight : preview.implicitHeight)
            readonly property real fitS: session.fitScale(fw, fh, Math.max(1, width - 2 * stage.margin),
                                                          Math.max(1, height - 2 * stage.margin))
            property real s: 1
            property real ox: 0
            property real oy: 0
            property bool fitMode: true
            // Zoom ancré : le point image (px, py) reste sous le point écran (ax, ay).
            property bool anchoring: false
            property real ax: 0
            property real ay: 0
            property real px: 0
            property real py: 0
            property alias zoomAnim: zoomAnim

            function clampNow() {
                const p = session.clampOffset(ox, oy, s, fw, fh, width, height)
                ox = p.x
                oy = p.y
            }
            function resetView() {
                zoomAnim.stop()
                anchoring = false
                fitMode = true
                s = fitS
                clampNow()
            }
            function zoomTo(target, x, y, animate) {
                const ns = session.clampScale(target, fw, fh, Math.max(1, width - 2 * stage.margin),
                                              Math.max(1, height - 2 * stage.margin))
                zoomAnim.stop()
                ax = x
                ay = y
                px = (x - ox) / s
                py = (y - oy) / s
                fitMode = ns <= fitS * 1.0001
                anchoring = true
                if (animate && stage.animations) {
                    zoomAnim.to = ns
                    zoomAnim.start()
                } else {
                    s = ns
                    anchoring = false
                }
            }

            onSChanged: if (anchoring) {
                ox = ax - s * px
                oy = ay - s * py
                clampNow()
            }
            onFitSChanged: if (fitMode) { s = fitS; clampNow() }
            onWidthChanged: clampNow()
            onHeightChanged: clampNow()
            onPathChanged: { fullSize = Qt.size(0, 0); fitMode = true }
            onReadyChanged: {
                if (ready) {
                    const f = session.fullSize(path)
                    if (f.width > 0)
                        fullSize = f
                    if (fitMode) { s = fitS; clampNow() }
                }
                if ((ready || failed) && stage.pendingSlot === index)
                    stage.startTransition()
            }
            onFailedChanged: if (failed && stage.pendingSlot === index) stage.startTransition()

            NumberAnimation {
                id: zoomAnim
                target: slot
                property: "s"
                duration: 120
                easing.type: Easing.OutQuad
                onRunningChanged: if (!running) slot.anchoring = false
            }

            readonly property bool needFull: slot === stage.current && kind === "image" && ready
                                             && s * fw > preview.implicitWidth * 1.01

            Image {
                id: preview
                x: slot.ox + slot.shift
                y: slot.oy
                width: slot.fw * slot.s
                height: slot.fh * slot.s
                visible: !slot.isGif && !(full.status === Image.Ready && full.visible)
                source: slot.kind === "image" ? session.source("screen", slot.path) : ""
                asynchronous: true
                cache: false
                smooth: true
                mipmap: true
            }
            Image {
                id: full
                x: preview.x
                y: preview.y
                width: preview.width
                height: preview.height
                visible: status === Image.Ready && slot.needFull
                source: slot.needFull ? session.source("full", slot.path) : ""
                asynchronous: true
                cache: false
                smooth: true
            }
            AnimatedImage {
                id: gif
                x: slot.ox + slot.shift
                y: slot.oy
                width: slot.fw * slot.s
                height: slot.fh * slot.s
                visible: slot.isGif
                source: slot.isGif ? "file:///" + slot.path.replace(/\\/g, "/") : ""
                playing: slot.visible
                cache: false
                smooth: true
            }
        }
    }

    // --- Entrées

    WheelHandler {
        onWheel: event => {
            const c = stage.current
            if (!c || !c.ready)
                return
            const base = c.zoomAnim.running ? c.zoomAnim.to : c.s
            c.zoomTo(base * session.wheelFactor(event.angleDelta.y), point.position.x, point.position.y, true)
        }
    }

    PinchHandler {
        id: pinch
        target: null
        property real startScale: 1
        onActiveChanged: if (active && stage.current) startScale = stage.current.s
        onActiveScaleChanged: if (active && stage.current && stage.current.ready)
            stage.current.zoomTo(startScale * activeScale, centroid.position.x, centroid.position.y, false)
    }

    DragHandler {
        id: drag
        target: null
        enabled: stage.canPan
        acceptedButtons: Qt.LeftButton
        cursorShape: active ? Qt.ClosedHandCursor : Qt.OpenHandCursor
        property real startX: 0
        property real startY: 0
        onActiveChanged: if (active && stage.current) {
            stage.current.zoomAnim.stop()
            stage.current.anchoring = false
            startX = stage.current.ox
            startY = stage.current.oy
        }
        onTranslationChanged: if (active && stage.current) {
            stage.current.ox = startX + translation.x
            stage.current.oy = startY + translation.y
            stage.current.clampNow()
        }
    }

    HoverHandler {
        cursorShape: stage.canPan ? Qt.OpenHandCursor : Qt.ArrowCursor
    }

    TapHandler {
        acceptedButtons: Qt.LeftButton
        onDoubleTapped: (eventPoint, button) => {
            const c = stage.current
            if (!c || !c.ready)
                return
            if (c.fitMode)
                c.zoomTo(1, eventPoint.position.x, eventPoint.position.y, true)
            else
                c.zoomTo(c.fitS, eventPoint.position.x, eventPoint.position.y, true)
        }
    }
}

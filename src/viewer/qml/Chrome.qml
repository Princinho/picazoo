import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Picazoo.Zoom

// Commandes, messages et cartes de la visionneuse autonome. Chargé après le premier
// affichage de la photo : Qt Quick Controls ne retarde pas l'ouverture (research V2).
Item {
    id: chrome
    required property var win
    required property var stage
    anchors.fill: parent

    readonly property bool popupOpen: moreMenu.visible || statusPopup.visible || imageMenu.visible || trashConfirm.visible
    readonly property bool hovering: controlsHover.hovered || topHover.hovered || stripHover.hovered

    function toast(text) {
        toastBox.action = null
        toastLabel.text = text
        toastTimer.interval = 1800
        toastBox.opacity = 1
        toastTimer.restart()
    }
    // 007 : message avec « Annuler » (corbeille, fond d'écran), 8 s.
    function toastWithUndo(text, action) {
        toastBox.action = action
        toastLabel.text = text
        toastTimer.interval = 8000
        toastBox.opacity = 1
        toastTimer.restart()
    }
    function undoToast() {
        if (toastBox.opacity > 0 && toastBox.action) {
            const a = toastBox.action
            toastBox.action = null
            toastBox.opacity = 0
            a()
            return true
        }
        return false
    }
    // 007 : menu de l'image ; (x, y) < 0 : au centre (touche Menu, Maj+F10).
    function openImageMenu(x, y) {
        stage.stopZoom()
        if (x < 0 || y < 0)
            imageMenu.openAt(chrome.width / 2, chrome.height / 2)
        else
            imageMenu.openAt(x, y)
    }
    function askTrash() {
        if (session.currentPath === "")
            return
        session.requestRecycleInfo()
        if (session.recycleState === 0) {
            toast("Ce disque n'a pas de corbeille : Picazoo ne supprime pas cette photo.")
            return
        }
        trashConfirm.ask({ count: 1, name: session.currentName, size: session.info.size || "",
                           noRecycleBin: 0, missing: 0 })
    }
    // Ferme un menu ou une fenêtre ouverte ; vrai s'il y en avait une.
    function closePopups() {
        if (imageMenu.visible) {
            imageMenu.close()
            return true
        }
        if (trashConfirm.visible) {
            trashConfirm.reject()
            return true
        }
        if (moreMenu.visible) {
            moreMenu.close()
            return true
        }
        if (statusPopup.visible) {
            statusPopup.close()
            return true
        }
        return false
    }

    // Zones des bords : précédente / suivante (FR-008)
    component EdgeZone: Item {
        id: edge
        property bool forward: true
        width: parent.width * 0.15
        visible: session.count > 1 && !stage.zoomed
        HoverHandler { id: edgeHover }
        TapHandler {
            onTapped: forward ? session.next() : session.previous()
        }
        RoundButton {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: edge.forward ? undefined : parent.left
            anchors.right: edge.forward ? parent.right : undefined
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            width: 52
            height: 52
            focusPolicy: Qt.NoFocus
            opacity: edgeHover.hovered ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: 150 } }
            Accessible.name: edge.forward ? "Photo suivante (→)" : "Photo précédente (←)"
            onClicked: edge.forward ? session.next() : session.previous()
            contentItem: Label {
                text: edge.forward ? Theme.iconNext : Theme.iconPrev
                font.family: Theme.iconFont
                font.pixelSize: 18
                color: Theme.text
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle { radius: 26; color: Theme.pill; border.color: Theme.pillLine }
        }
    }
    EdgeZone { forward: false; anchors.left: parent.left; anchors.top: topBar.bottom; anchors.bottom: bottom.top }
    EdgeZone { forward: true; anchors.right: parent.right; anchors.top: topBar.bottom; anchors.bottom: bottom.top }

    // Photo non affichable (FR-025, contracts/ui-contract.md « Messages »)
    ColumnLayout {
        anchors.centerIn: parent
        visible: stage.currentFailed || session.currentKind === "none"
        spacing: 16
        readonly property string err: stage.currentFailed ? session.decodeError(session.currentPath) : ""
        Label {
            Layout.alignment: Qt.AlignHCenter
            font.pixelSize: 18
            color: Theme.text
            text: session.currentKind === "video" ? "Les vidéos se regardent dans Picazoo."
                : session.currentKind === "unsupported" ? "Picazoo ne sait pas afficher ce type de fichier."
                : session.currentKind === "none" ? "Aucune photo à afficher dans ce dossier."
                : parent.err === "noPreview" ? "Aperçu indisponible pour ce fichier RAW."
                : "Cette image est illisible."
        }
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 10
            visible: session.currentKind !== "none"
            PillButton {
                visible: session.currentKind === "video"
                text: "Ouvrir dans Picazoo"
                onClicked: session.openInPicazoo()
            }
            PillButton {
                text: session.currentKind === "video" ? "Ouvrir avec le lecteur par défaut" : "Ouvrir avec une autre application"
                onClicked: session.currentKind === "video" ? session.openWithDefault() : session.openWithOther()
            }
        }
    }

    BusyIndicator {
        anchors.centerIn: parent
        running: !stage.currentReady && !stage.currentFailed && session.currentKind !== "none" && busyDelay.elapsed
        visible: running
        Timer {
            id: busyDelay
            property bool elapsed: false
            interval: 300
            running: !stage.currentReady
            onRunningChanged: if (running) elapsed = false
            onTriggered: elapsed = true
        }
    }

    // En-tête : nom, position, menu, fermer
    Item {
        id: topBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 88
        opacity: win.controlsShown || win.keepControls ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 250 } }
        HoverHandler { id: topHover }

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0; color: "#99000000" }
                GradientStop { position: 1; color: "#00000000" }
            }
        }
        ColumnLayout {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.leftMargin: 28
            anchors.topMargin: 20
            anchors.right: topButtons.left
            anchors.rightMargin: 24
            spacing: 2
            Label {
                Layout.fillWidth: true
                text: session.currentName
                font.family: Theme.displayFont
                font.pixelSize: 17
                font.weight: Font.DemiBold
                color: Theme.text
                elide: Text.ElideMiddle
            }
            Label {
                text: session.positionText
                font.pixelSize: 13
                color: Theme.text2
            }
        }
        Row {
            id: topButtons
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.rightMargin: 20
            anchors.topMargin: 16
            spacing: 6
            IconButton {
                id: moreButton
                glyph: Theme.iconMore
                Accessible.name: "Plus d'options"
                onClicked: moreMenu.open()
                Menu {
                    id: moreMenu
                    y: moreButton.height + 6
                    x: moreButton.width - width
                    MenuItem {
                        text: "Ouvrir vos photos avec Picazoo…"
                        onTriggered: session.showProposal()
                    }
                    MenuItem {
                        text: "Associations actuelles…"
                        onTriggered: { statusPopup.rows = session.associationStatus(); statusPopup.open() }
                    }
                    MenuItem {
                        text: "Inscrire Picazoo dans « Ouvrir avec »"
                        visible: !session.registered
                        height: visible ? implicitHeight : 0
                        onTriggered: if (session.registerNow()) win.toast("Picazoo figure maintenant dans « Ouvrir avec ».")
                    }
                    MenuSeparator {}
                    MenuItem {
                        text: "Afficher la bande de vignettes"
                        checkable: true
                        checked: session.stripVisible
                        onTriggered: session.stripVisible = checked
                    }
                    MenuItem {
                        text: "Voile opaque (F11)"
                        checkable: true
                        checked: session.opaque
                        enabled: session.transparencyAvailable
                        onTriggered: session.opaque = checked
                    }
                }
            }
            IconButton {
                glyph: Theme.iconClose
                Accessible.name: "Fermer (Échap)"
                onClicked: win.closeViewer()
            }
        }
    }

    // Bande des voisines (US6) et commandes (FR-018, FR-022)
    Column {
        id: bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 20
        spacing: 12
        opacity: win.controlsShown || win.keepControls ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 250 } }

        Strip {
            id: strip
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(chrome.width - 64, 17 * 76)
            visible: session.stripVisible && session.count > 1
            HoverHandler { id: stripHover }
        }
        Controls {
            id: controls
            anchors.horizontalCenter: parent.horizontalCenter
            zoomPercent: stage.zoomPercent
            fitActive: !stage.zoomed
            infoActive: win.showInfo
            veilActive: session.opaque
            imageActions: stage.currentReady
            onPrevious: session.previous()
            onNext: session.next()
            onZoomIn: stage.zoomStep(+1, ZoomController.Button, false)
            onZoomOut: stage.zoomStep(-1, ZoomController.Button, false)
            onFit: stage.fit()
            onActualSize: stage.actualSize()
            onToggleInfo: win.showInfo = !win.showInfo
            onReveal: session.revealInExplorer()
            onOpenInApp: session.openInPicazoo()
            onToggleVeil: session.opaque = !session.opaque
            HoverHandler { id: controlsHover }
        }
    }

    InfoCard {
        anchors.right: parent.right
        anchors.top: topBar.bottom
        anchors.rightMargin: 24
        visible: win.showInfo
        info: session.info
        onReveal: session.revealInExplorer()
    }

    AssocCard {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: bottom.top
        anchors.bottomMargin: 16
        visible: session.proposalVisible
    }

    ImageContextMenu {
        id: imageMenu
        win: chrome.win
        stage: chrome.stage
        onTrashRequested: chrome.askTrash()
    }
    TrashConfirm {
        id: trashConfirm
        onConfirmed: session.trashCurrent()
    }
    Connections {
        target: session
        function onTrashed(operationId, name) {
            chrome.toastWithUndo("« " + name + " » mis à la corbeille", () => session.undoTrash(operationId))
        }
        function onWallpaperChanged(ok, restorable) {
            if (!ok)
                chrome.toast("Windows a refusé de changer le fond d'écran.")
            else if (restorable)
                chrome.toastWithUndo("Fond d'écran changé", () => session.undoWallpaper())
            else
                chrome.toast("Fond d'écran changé. L'ancien fond ne pourra pas être rétabli automatiquement.")
        }
        function onWallpaperRestored(ok) {
            chrome.toast(ok ? "Ancien fond d'écran rétabli." : "L'ancien fond d'écran n'a pas pu être rétabli.")
        }
    }

    // Bandeau bref
    Rectangle {
        id: toastBox
        property var action: null
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 96
        width: toastRow.implicitWidth + 40
        height: 44
        radius: 22
        color: Theme.pill
        border.color: Theme.pillLine
        opacity: 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 200 } }
        Row {
            id: toastRow
            anchors.centerIn: parent
            spacing: 14
            Label {
                id: toastLabel
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.text
            }
            Label {
                visible: toastBox.action !== null
                anchors.verticalCenter: parent.verticalCenter
                text: "Annuler"
                color: Theme.accentText
                font.weight: Font.DemiBold
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -8
                    cursorShape: Qt.PointingHandCursor
                    onClicked: chrome.undoToast()
                }
            }
        }
        Timer {
            id: toastTimer
            interval: 1800
            onTriggered: toastBox.opacity = 0
        }
    }

    Popup {
        id: statusPopup
        property var rows: []
        anchors.centerIn: parent
        modal: true
        padding: 24
        background: Rectangle { radius: 14; color: Theme.panel; border.color: Theme.lineStrong }
        contentItem: ColumnLayout {
            spacing: 12
            Label {
                text: "Applications qui ouvrent vos photos"
                font.family: Theme.displayFont
                font.pixelSize: 17
                font.weight: Font.DemiBold
                color: Theme.text
            }
            GridLayout {
                columns: 4
                columnSpacing: 24
                rowSpacing: 6
                Repeater {
                    model: statusPopup.rows
                    delegate: RowLayout {
                        required property var modelData
                        spacing: 8
                        Label { text: modelData.extension; color: Theme.text; Layout.preferredWidth: 52 }
                        Label { text: modelData.picazoo ? "Picazoo" : "autre application"; color: modelData.picazoo ? Theme.okText : Theme.text3 }
                    }
                }
            }
            Label {
                Layout.maximumWidth: 560
                wrapMode: Text.WordWrap
                color: Theme.text2
                text: "Ce choix appartient à Windows : Picazoo ne le modifie jamais de lui-même."
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                PillButton { text: "Modifier dans Windows…"; onClicked: { statusPopup.close(); session.answerProposal("accepted", []) } }
                PillButton { text: "Fermer"; onClicked: statusPopup.close() }
            }
        }
    }
}

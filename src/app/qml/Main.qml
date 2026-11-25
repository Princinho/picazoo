import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1440
    height: 900
    visible: true
    title: app ? "Picazoo " + app.version : "Picazoo"
    color: Theme.bg
    font.family: Theme.font
    font.pixelSize: 14

    readonly property bool ready: startupError === "" && app !== null
    property bool viewerOpen: false
    property var viewerItem: 0

    // La recherche des doublons laisse la priorité à la visionneuse (research D5-5).
    onViewerOpenChanged: {
        if (dups)
            viewerOpen ? dups.hold() : dups.release()
    }

    function openViewer(itemId) {
        if (!itemId)
            return
        viewerItem = itemId
        viewerOpen = true
    }

    function closeViewer(lastItemId) {
        viewerOpen = false
        if (lastItemId) {
            selection.current = lastItemId
            libraryView.revealItem(lastItemId)
        }
        libraryView.forceActiveFocus()
    }

    // Erreur au démarrage : stockage de Picazoo inaccessible.
    Label {
        visible: startupError !== ""
        anchors.centerIn: parent
        width: Math.min(parent.width - 64, 640)
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
        text: startupError
        color: Theme.text
        font.pixelSize: 16
    }

    Loader {
        id: onboardingLoader
        anchors.fill: parent
        active: win.ready && !onboarding.done
        sourceComponent: Onboarding {}
    }

    Library {
        id: libraryView
        anchors.fill: parent
        visible: win.ready && onboarding.done && !win.viewerOpen
        enabled: visible
        focus: visible
        onOpenItem: id => win.openViewer(id)
        onAddFolder: addFolderDialog.open()
        onWatchedFolders: watchedFoldersPopup.open()
        onOperationLog: operationLogPopup.open()
    }

    Loader {
        id: viewerLoader
        anchors.fill: parent
        active: win.ready && win.viewerOpen
        focus: active
        sourceComponent: Viewer {
            startItem: win.viewerItem
            onClosed: lastItem => win.closeViewer(lastItem)
        }
        onLoaded: item.forceActiveFocus()
    }

    WatchedFolders {
        id: watchedFoldersPopup
    }

    OperationLog {
        id: operationLogPopup
    }

    // Développement uniquement (PICAZOO_DEV_SCREEN), appelé depuis main.cpp.
    function devOpenScreen(name) {
        if (!win.ready)
            return
        if (name === "log") {
            operationLogPopup.open()
            return
        }
        if (name === "grid-menu") {
            libraryView.devItemMenu()
            return
        }
        // 007 : visionneuse sur le premier élément ; « viewer-fullscreen » : en plein écran avec le
        // bouton de sortie affiché ; « viewer-menu » : menu de l'image ouvert.
        if (name.startsWith("viewer")) {
            win.openViewer(gridModel.itemAtFlat(0))
            const v = viewerLoader.item
            if (v && name === "viewer-fullscreen")
                v.toggleFullScreen()
            else if (v && name === "viewer-menu")
                v.openContextMenu(-1, -1)
            return
        }
        libraryView.showGroup(0)
        if (name === "summary") {
            dups.groups.selectAll()
            libraryView.openTrashSummary()
        } else if (name === "dossiers" || name === "aside" || name === "selection") {
            libraryView.devDuplicates(name)
        }
    }

    Timer {
        // Développement uniquement (PICAZOO_DEV_OPEN).
        interval: 1500
        running: win.ready && devOpenIndex >= 0
        onTriggered: win.openViewer(gridModel.itemAtFlat(devOpenIndex))
    }

    // --- 007 : « Mettre à la corbeille… » (bibliothèque, visionneuse intégrée), fond d'écran ;
    // confirmation commune et message « Annuler » visible dans les deux écrans.
    TrashConfirm {
        id: trashConfirm
        onConfirmed: trash.confirmItems(win.viewerOpen ? "viewer" : "library")
        onCancelled: trash.cancelItems()
    }

    function toast(message, kind, operationId) {
        toastText.text = message
        toastBox.kind = kind
        toastBox.operationId = operationId || 0
        toastBox.visible = true
        toastTimer.restart()
    }
    function undoToast() {
        if (toastBox.kind === "trash" && toastBox.operationId)
            trash.undo(toastBox.operationId)
        else if (toastBox.kind === "wallpaper")
            app.undoWallpaper()
        toastBox.visible = false
    }

    Connections {
        target: win.ready ? trash : null
        function onItemsPlanReady(plan) {
            if (plan.count > 0)
                trashConfirm.ask(plan)
            else
                win.toast(plan.noRecycleBin > 0 ? "Ce disque n'a pas de corbeille : Picazoo ne supprime pas ce fichier."
                                                : "Fichier introuvable : rien n'a été mis à la corbeille.", "", 0)
        }
        function onItemsTrashed(operationId, count, message) {
            win.toast(message, count > 0 ? "trash" : "", operationId)
        }
    }
    Connections {
        target: win.ready ? app : null
        function onWallpaperChanged(ok, restorable) {
            if (!ok)
                win.toast("Windows a refusé de changer le fond d'écran.", "", 0)
            else if (restorable)
                win.toast("Fond d'écran changé.", "wallpaper", 0)
            else
                win.toast("Fond d'écran changé. L'ancien fond (diaporama ou couleur) ne pourra pas être rétabli automatiquement.", "", 0)
        }
        function onWallpaperRestored(ok) {
            win.toast(ok ? "Ancien fond d'écran rétabli." : "L'ancien fond d'écran n'a pas pu être rétabli.", "", 0)
        }
    }
    Shortcut {
        sequences: ["Ctrl+Z"]
        enabled: toastBox.visible && toastBox.kind !== ""
        onActivated: win.undoToast()
    }

    Rectangle {
        id: toastBox
        property string kind: ""      // trash | wallpaper | "" (simple message)
        property var operationId: 0
        visible: false
        z: 100
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 32
        width: toastRow.implicitWidth + 28
        height: 48
        radius: 10
        color: "#2A3240"
        border.color: Theme.lineStrong
        RowLayout {
            id: toastRow
            anchors.verticalCenter: parent.verticalCenter
            x: 16
            spacing: 12
            Label { id: toastText; color: Theme.text; Layout.maximumWidth: win.width - 260; elide: Text.ElideRight }
            Button {
                id: toastUndo
                visible: toastBox.kind !== ""
                focusPolicy: Qt.NoFocus
                flat: true
                contentItem: Label { text: "Annuler"; color: Theme.accentText; font.weight: Font.DemiBold; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { radius: 8; color: toastUndo.hovered ? Theme.hover : "transparent" }
                onClicked: win.undoToast()
            }
            ToolButton {
                id: toastClose
                focusPolicy: Qt.NoFocus
                Accessible.name: "Fermer le message"
                contentItem: Label { text: "×"; color: Theme.text2; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { radius: 8; color: toastClose.hovered ? Theme.hover : "transparent" }
                onClicked: toastBox.visible = false
            }
        }
        Timer { id: toastTimer; interval: 8000; onTriggered: toastBox.visible = false }
    }

    FolderDialog {
        id: addFolderDialog
        title: "Ajouter un dossier à surveiller"
        onAccepted: app.addRoot(selectedFolder)
    }

    Dialog {
        id: storageDialog
        property alias message: storageText.text
        anchors.centerIn: parent
        modal: true
        title: "Picazoo"
        standardButtons: Dialog.Ok
        Label {
            id: storageText
            wrapMode: Text.WordWrap
            width: 420
        }
    }

    Connections {
        target: win.ready ? app : null
        function onStorageError(message) {
            storageDialog.message = message
            storageDialog.open()
        }
        // « Ouvrir dans Picazoo » depuis la visionneuse autonome (005)
        function onShowItem(itemId) {
            if (win.viewerOpen)
                win.closeViewer(0)
            selection.current = itemId
            libraryView.showItemInGrid(itemId)
            libraryView.forceActiveFocus()
        }
        function onShowMessage(message) {
            storageDialog.message = message
            storageDialog.open()
        }
        function onAskAddFolder(folder) {
            addFolderQuestion.folder = folder
            addFolderQuestion.open()
        }
    }

    // L'ajout d'un dossier n'est jamais fait d'office (principe IV).
    Dialog {
        id: addFolderQuestion
        property string folder
        anchors.centerIn: parent
        modal: true
        title: "Ajouter ce dossier à la bibliothèque ?"
        standardButtons: Dialog.Yes | Dialog.No
        width: Math.min(560, win.width - 64)
        onAccepted: app.addRootPath(folder)
        Label {
            width: addFolderQuestion.availableWidth
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            text: "La photo se trouve dans « " + addFolderQuestion.folder + " », qui ne fait pas partie de vos "
                  + "dossiers surveillés. Picazoo peut ajouter ce dossier et ses sous-dossiers à la bibliothèque."
        }
    }

    // Doublons (002) : « Voir » depuis la visionneuse, compte rendu de la corbeille.
    Connections {
        target: win.ready ? dups : null
        function onShowGroupRequested(groupId) {
            win.viewerOpen = false
            libraryView.showGroup(groupId)
        }
    }
    Connections {
        target: win.ready ? trash : null
        function onFinished(message) {
            storageDialog.message = message
            storageDialog.open()
        }
    }
}

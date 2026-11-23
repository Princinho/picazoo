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
        libraryView.showGroup(0)
        if (name === "summary")
            libraryView.openTrashSummary()
    }

    Timer {
        // Développement uniquement (PICAZOO_DEV_OPEN).
        interval: 1500
        running: win.ready && devOpenIndex >= 0
        onTriggered: win.openViewer(gridModel.itemAtFlat(devOpenIndex))
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

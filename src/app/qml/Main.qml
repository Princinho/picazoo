import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1400
    height: 900
    visible: true
    title: app ? "Picazoo " + app.version : "Picazoo"
    color: "#1e1e1e"

    readonly property bool ready: startupError === "" && app !== null
    property bool viewerOpen: false
    property var viewerItem: 0

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

    menuBar: MenuBar {
        visible: win.ready && !win.viewerOpen
        Menu {
            title: "&Fichier"
            Action {
                text: "Ajouter un dossier…"
                enabled: win.ready && onboarding.done
                onTriggered: addFolderDialog.open()
            }
            MenuSeparator {}
            Action {
                text: "&Quitter"
                shortcut: StandardKey.Quit
                onTriggered: Qt.quit()
            }
        }
        Menu {
            title: "&Affichage"
            Action {
                text: "Agrandir les vignettes"
                shortcut: "Ctrl++"
                enabled: win.ready
                onTriggered: gridModel.thumbSize = gridModel.thumbSize + 32
            }
            Action {
                text: "Réduire les vignettes"
                shortcut: "Ctrl+-"
                enabled: win.ready
                onTriggered: gridModel.thumbSize = gridModel.thumbSize - 32
            }
            MenuSeparator {}
            Action {
                text: "Dossiers par année"
                checkable: true
                checked: win.ready && folderModel.mode === "by_year"
                onTriggered: folderModel.mode = "by_year"
            }
            Action {
                text: "Dossiers en arborescence"
                checkable: true
                checked: win.ready && folderModel.mode === "tree"
                onTriggered: folderModel.mode = "tree"
            }
        }
        Menu {
            title: "&Outils"
            Action {
                text: "Dossiers surveillés…"
                enabled: win.ready && onboarding.done
                onTriggered: watchedFolders.open()
            }
            Action {
                text: "Réanalyser les dossiers"
                shortcut: "F5"
                enabled: win.ready && onboarding.done
                onTriggered: app.rescan()
            }
        }
    }

    // Erreur au démarrage : stockage de Picazoo inaccessible.
    Label {
        visible: startupError !== ""
        anchors.centerIn: parent
        width: Math.min(parent.width - 64, 640)
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
        text: startupError
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
        id: watchedFolders
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
    }
}

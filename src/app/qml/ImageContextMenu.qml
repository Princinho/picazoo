import QtQuick
import QtQuick.Controls

// Menu de l'image dans la visionneuse intégrée (007, contracts/ui-contract.md).
Menu {
    id: menu
    width: 340
    property var viewerRoot: null // Viewer.qml
    readonly property var itemId: viewer.current
    readonly property bool isVideo: viewerRoot ? viewerRoot.isVideo : false
    property int recycle: -1

    function openAt(parentItem, x, y) {
        recycle = app.recycleState([itemId])
        if (parentItem)
            popup(parentItem, x, y)
        else
            popup()
    }

    Connections {
        target: app
        function onRecycleInfoChanged() { if (menu.visible) menu.recycle = app.recycleState([menu.itemId]) }
    }

    PzMenuItem { text: "Précédente"; hint: "←"; enabled: viewer.hasPrevious; onTriggered: viewerRoot.goPrevious() }
    PzMenuItem { text: "Suivante"; hint: "→"; enabled: viewer.hasNext; onTriggered: viewerRoot.goNext() }
    MenuSeparator {}
    PzMenuItem { text: "Zoom avant"; hint: "↑"; enabled: !menu.isVideo; onTriggered: viewerRoot.zoomStep(+1, 2, false) }
    PzMenuItem { text: "Zoom arrière"; hint: "↓"; enabled: !menu.isVideo; onTriggered: viewerRoot.zoomStep(-1, 2, false) }
    PzMenuItem { text: "Ajuster"; hint: "0"; enabled: !menu.isVideo; onTriggered: viewerRoot.fit() }
    PzMenuItem { text: "100 %"; hint: "1"; enabled: !menu.isVideo; onTriggered: viewerRoot.setZoom(1) }
    MenuSeparator {}
    PzMenuItem {
        text: viewerRoot && viewerRoot.fullScreen ? "Quitter le plein écran" : "Plein écran"
        hint: "F11"
        onTriggered: viewerRoot.toggleFullScreen()
    }
    PzMenuItem {
        text: viewerRoot && viewerRoot.showInfo ? "Masquer les informations" : "Informations"
        hint: "I"
        enabled: viewerRoot ? !viewerRoot.fullScreen : true
        onTriggered: viewerRoot.showInfo = !viewerRoot.showInfo
    }
    MenuSeparator {}
    PzMenuItem { text: menu.isVideo ? "Copier" : "Copier l'image"; hint: "Ctrl+C"; onTriggered: app.copyItems([menu.itemId]) }
    PzMenuItem { text: "Copier le chemin"; hint: "Ctrl+Maj+C"; onTriggered: app.copyPaths([menu.itemId]) }
    PzMenuItem { text: "Afficher dans l'Explorateur"; hint: "Ctrl+Maj+E"; onTriggered: app.revealInExplorer(menu.itemId) }
    PzMenuItem { text: "Ouvrir avec l'application par défaut"; onTriggered: app.openWithDefaultApp(menu.itemId) }
    MenuSeparator {}
    PzMenuItem {
        readonly property bool possible: !menu.isVideo && app.canSetWallpaper(menu.itemId)
        text: "Définir comme fond d'écran"
        enabled: possible
        tip: possible ? "" : "format non accepté par Windows"
        onTriggered: app.setWallpaper(menu.itemId)
    }
    PzMenuItem { text: "Propriétés"; hint: "Alt+Entrée"; onTriggered: app.showProperties([menu.itemId]) }
    PzMenuItem {
        text: "Mettre à la corbeille…"
        hint: "Suppr"
        enabled: menu.recycle === 1
        tip: menu.recycle === 0 ? "pas de corbeille sur ce disque" : menu.recycle < 0 ? "vérification…" : ""
        onTriggered: trash.prepareItems([menu.itemId])
    }
    MenuSeparator {}
    PzMenuItem { text: "Retour à la bibliothèque"; hint: "Échap"; onTriggered: viewerRoot.close() }
}

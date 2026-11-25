import QtQuick
import QtQuick.Controls

// Menu d'une vignette ou d'une sélection de vignettes (007, contracts/ui-contract.md).
Menu {
    id: menu
    width: 360
    property var ids: []
    readonly property int itemCount: ids.length
    readonly property var single: itemCount === 1 ? ids[0] : 0
    readonly property var info: single ? app.itemInfo(single) : ({})
    readonly property bool isVideo: info.kind === "video"
    property int recycle: -1 // 1 : corbeille disponible ; 0 : non ; -1 : en cours de vérification
    signal openItem(var id)

    function openFor(list, parentItem, x, y) {
        ids = list
        recycle = app.recycleState(ids)
        if (parentItem)
            popup(parentItem, x, y)
        else
            popup()
    }
    function trashSelection() {
        if (itemCount > 0)
            trash.prepareItems(ids)
    }

    Connections {
        target: app
        function onRecycleInfoChanged() { if (menu.visible) menu.recycle = app.recycleState(menu.ids) }
    }

    PzMenuItem {
        shown: menu.itemCount > 1
        enabled: false
        text: menu.itemCount + " éléments sélectionnés"
    }
    MenuSeparator { visible: menu.itemCount > 1; height: visible ? implicitHeight : 0 }
    PzMenuItem {
        text: "Ouvrir"
        hint: "Entrée"
        enabled: menu.itemCount === 1
        onTriggered: menu.openItem(menu.single)
    }
    PzMenuItem {
        text: "Ouvrir dans la visionneuse autonome"
        enabled: menu.itemCount === 1 && !menu.isVideo
        onTriggered: app.openInStandaloneViewer(menu.single)
    }
    PzMenuItem {
        text: "Ouvrir avec l'application par défaut"
        enabled: menu.itemCount === 1
        onTriggered: app.openWithDefaultApp(menu.single)
    }
    MenuSeparator {}
    PzMenuItem {
        text: "Copier"
        hint: "Ctrl+C"
        onTriggered: app.copyItems(menu.ids)
    }
    PzMenuItem {
        text: "Copier le chemin"
        hint: "Ctrl+Maj+C"
        onTriggered: app.copyPaths(menu.ids)
    }
    PzMenuItem {
        text: "Afficher dans l'Explorateur"
        hint: "Ctrl+Maj+E"
        enabled: menu.itemCount === 1
        onTriggered: app.revealInExplorer(menu.single)
    }
    PzMenuItem {
        readonly property bool hasDuplicates: menu.itemCount === 1 && dups !== null && dups.duplicateCount(menu.single) > 0
        text: "Voir les doublons de cette photo"
        enabled: hasDuplicates
        tip: menu.itemCount === 1 && !hasDuplicates ? "aucun doublon connu" : ""
        onTriggered: dups.showGroup(dups.groupOf(menu.single))
    }
    MenuSeparator {}
    PzMenuItem {
        readonly property bool possible: menu.itemCount === 1 && app.canSetWallpaper(menu.single)
        text: "Définir comme fond d'écran"
        enabled: possible
        tip: menu.itemCount === 1 && !possible ? "format non accepté par Windows" : ""
        onTriggered: app.setWallpaper(menu.single)
    }
    PzMenuItem {
        text: "Propriétés"
        hint: "Alt+Entrée"
        onTriggered: app.showProperties(menu.ids)
    }
    PzMenuItem {
        text: "Mettre à la corbeille…"
        hint: "Suppr"
        enabled: menu.recycle === 1
        tip: menu.recycle === 0 ? "pas de corbeille sur ce disque" : menu.recycle < 0 ? "vérification…" : ""
        onTriggered: menu.trashSelection()
    }
}

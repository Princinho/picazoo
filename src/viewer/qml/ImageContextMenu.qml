import QtQuick
import QtQuick.Controls
import Picazoo.Zoom

// Menu de l'image dans la visionneuse autonome (007, contracts/ui-contract.md).
Menu {
    id: menu
    width: 360
    required property var win
    required property var stage
    readonly property bool isImage: session.currentKind === "image" || session.currentKind === "gif"
    signal trashRequested()

    function openAt(x, y) {
        session.requestRecycleInfo()
        popup(x, y)
    }

    PzMenuItem { text: "Précédente"; hint: "←"; enabled: session.count > 1; onTriggered: session.previous() }
    PzMenuItem { text: "Suivante"; hint: "→"; enabled: session.count > 1; onTriggered: session.next() }
    MenuSeparator {}
    PzMenuItem { text: "Zoom avant"; hint: "↑"; enabled: stage.currentReady; onTriggered: stage.zoomStep(+1, ZoomController.Button, false) }
    PzMenuItem { text: "Zoom arrière"; hint: "↓"; enabled: stage.currentReady; onTriggered: stage.zoomStep(-1, ZoomController.Button, false) }
    PzMenuItem { text: "Ajuster"; hint: "Ctrl+0"; enabled: stage.currentReady; onTriggered: stage.fit() }
    PzMenuItem { text: "100 %"; enabled: stage.currentReady; onTriggered: stage.actualSize() }
    MenuSeparator {}
    PzMenuItem {
        text: win.showInfo ? "Masquer les informations" : "Informations"
        hint: "I"
        onTriggered: win.showInfo = !win.showInfo
    }
    MenuSeparator {}
    PzMenuItem { text: menu.isImage ? "Copier l'image" : "Copier"; hint: "Ctrl+C"; onTriggered: session.copyImage() }
    PzMenuItem { text: "Copier le chemin"; hint: "Ctrl+Maj+C"; onTriggered: session.copyPath() }
    PzMenuItem { text: "Afficher dans l'Explorateur"; hint: "Ctrl+Maj+E"; onTriggered: session.revealInExplorer() }
    PzMenuItem { text: "Ouvrir dans Picazoo"; onTriggered: session.openInPicazoo() }
    PzMenuItem { text: "Ouvrir avec une autre application…"; onTriggered: session.openWithOther() }
    MenuSeparator {}
    PzMenuItem {
        readonly property bool possible: session.currentPath !== "" && session.canSetWallpaper()
        text: "Définir comme fond d'écran"
        enabled: possible
        tip: possible ? "" : "format non accepté par Windows"
        onTriggered: session.setWallpaper()
    }
    PzMenuItem { text: "Propriétés"; hint: "Alt+Entrée"; onTriggered: session.showProperties() }
    PzMenuItem {
        text: "Mettre à la corbeille…"
        hint: "Suppr"
        enabled: session.recycleState === 1
        tip: session.recycleState === 0 ? "pas de corbeille sur ce disque" : session.recycleState < 0 ? "vérification…" : ""
        onTriggered: menu.trashRequested()
    }
    MenuSeparator {}
    PzMenuItem { text: "Fermer"; hint: "Échap"; onTriggered: win.closeViewer() }
}

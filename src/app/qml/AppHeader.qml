import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Barre du haut : logo, menus Fichier / Affichage / Outils, pastille d'analyse.
Rectangle {
    id: root
    signal addFolder()
    signal watchedFolders()
    signal operationLog()
    signal showDuplicates()
    signal showRelated()

    implicitHeight: 56
    color: Theme.panel

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.line
    }

    component MenuButton: ToolButton {
        id: mb
        property Menu menu
        font.pixelSize: 14
        contentItem: Label {
            text: mb.text
            color: mb.hovered || mb.menu.visible ? Theme.text : Theme.textSoft
            font: mb.font
        }
        background: Rectangle {
            radius: 8
            color: mb.hovered || mb.menu.visible ? Theme.hover : "transparent"
        }
        onClicked: menu.popup(mb, 0, mb.height)
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.rightMargin: 16
        spacing: 16

        RowLayout {
            spacing: 10
            Image {
                source: "qrc:/brand/picazoo-small.svg"
                sourceSize: Qt.size(28, 28)
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
            }
            Label {
                text: "Picazoo"
                font.family: Theme.displayFont
                font.pixelSize: 20
                font.weight: Font.DemiBold
                color: Theme.text
            }
        }

        RowLayout {
            spacing: 2
            MenuButton { text: "Fichier"; menu: fileMenu }
            MenuButton { text: "Affichage"; menu: viewMenu }
            MenuButton { text: "Outils"; menu: toolsMenu }
        }

        Item { Layout.fillWidth: true }

        // Pastille des doublons (002) : analyse en cours, ou en attente (pause, batterie…).
        Rectangle {
            visible: !app.scanActive && dups !== null && (dups.active || dups.waiting)
            Layout.preferredHeight: 40
            Layout.preferredWidth: dupPill.implicitWidth + 24
            radius: 20
            color: Theme.amberBg
            border.color: Theme.amberLine
            RowLayout {
                id: dupPill
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 14
                spacing: 12
                Rectangle { Layout.preferredWidth: 8; Layout.preferredHeight: 8; radius: 4; color: Theme.amber }
                Label {
                    text: dups ? dups.progressText : ""
                    color: Theme.amberText
                    font.pixelSize: 13
                }
                Rectangle {
                    visible: dups !== null && dups.progressValue >= 0
                    Layout.preferredWidth: 120
                    Layout.preferredHeight: 4
                    radius: 2
                    color: Theme.amberTrack
                    Rectangle {
                        width: parent.width * (dups ? Math.max(0, Math.min(1, dups.progressValue)) : 0)
                        height: parent.height
                        radius: 2
                        color: Theme.amber
                    }
                }
                RoundButton {
                    visible: dups !== null && dups.autoSearch
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                    Accessible.name: dups && dups.paused ? "Reprendre la recherche des doublons" : "Mettre la recherche des doublons en pause"
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name
                    onClicked: dups.togglePause()
                    background: Rectangle { radius: 16; color: Theme.amberTrack }
                    contentItem: Item {
                        Label {
                            anchors.centerIn: parent
                            text: dups && dups.paused ? Theme.iconPlay : Theme.iconPause
                            font.family: Theme.iconFont
                            font.pixelSize: 12
                            color: Theme.amberText
                        }
                    }
                }
            }
        }

        // Pastille d'analyse (ambre) : visible seulement pendant une analyse.
        Rectangle {
            visible: app.scanActive
            Layout.preferredHeight: 40
            Layout.preferredWidth: pill.implicitWidth + 24
            radius: 20
            color: Theme.amberBg
            border.color: Theme.amberLine
            RowLayout {
                id: pill
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 14
                spacing: 12
                Rectangle { Layout.preferredWidth: 8; Layout.preferredHeight: 8; radius: 4; color: Theme.amber }
                Label {
                    text: app.scanPaused ? app.scanText + " — en pause" : app.scanText
                    color: Theme.amberText
                    font.pixelSize: 13
                }
                Rectangle {
                    visible: app.scanProgress >= 0
                    Layout.preferredWidth: 120
                    Layout.preferredHeight: 4
                    radius: 2
                    color: Theme.amberTrack
                    Rectangle {
                        width: parent.width * Math.max(0, Math.min(1, app.scanProgress))
                        height: parent.height
                        radius: 2
                        color: Theme.amber
                    }
                }
                RoundButton {
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                    Accessible.name: app.scanPaused ? "Reprendre l'analyse" : "Mettre l'analyse en pause"
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name
                    onClicked: app.togglePause()
                    background: Rectangle { radius: 16; color: Theme.amberTrack }
                    contentItem: Item {
                        // pause : deux barres ; reprise : triangle
                        Label {
                            anchors.centerIn: parent
                            text: app.scanPaused ? Theme.iconPlay : Theme.iconPause
                            font.family: Theme.iconFont
                            font.pixelSize: 12
                            color: Theme.amberText
                        }
                    }
                }
            }
        }
    }

    Menu {
        id: fileMenu
        Action { text: "Ajouter un dossier…"; onTriggered: root.addFolder() }
        MenuSeparator {}
        Action { text: "Quitter"; shortcut: StandardKey.Quit; onTriggered: Qt.quit() }
    }
    Menu {
        id: viewMenu
        Action {
            text: "Agrandir les vignettes"
            shortcut: "Ctrl++"
            onTriggered: gridModel.thumbSize = gridModel.thumbSize + 32
        }
        Action {
            text: "Réduire les vignettes"
            shortcut: "Ctrl+-"
            onTriggered: gridModel.thumbSize = gridModel.thumbSize - 32
        }
        MenuSeparator {}
        Action {
            text: "Dossiers par année"
            checkable: true
            checked: folderModel.mode === "by_year"
            onTriggered: folderModel.mode = "by_year"
        }
        Action {
            text: "Dossiers en arborescence"
            checkable: true
            checked: folderModel.mode === "tree"
            onTriggered: folderModel.mode = "tree"
        }
    }
    Menu {
        id: toolsMenu
        width: 520
        Action { text: "Dossiers surveillés…"; onTriggered: root.watchedFolders() }
        Action { text: "Réanalyser les dossiers"; shortcut: "F5"; onTriggered: app.rescan() }
        MenuSeparator {}
        Action { text: "Doublons"; onTriggered: root.showDuplicates() }
        Action {
            text: "Rechercher les doublons automatiquement"
            checkable: true
            checked: dups !== null && dups.autoSearch
            onTriggered: dups.autoSearch = checked
        }
        Action { text: "Rechercher les doublons maintenant"; onTriggered: dups.rescan() }
        MenuSeparator {}
        Action { text: "Images liées"; enabled: naming !== null; onTriggered: root.showRelated() }
        Action {
            text: "Inclure les noms donnés par l'appareil (IMG_, DSC_…) dans les images liées"
            enabled: naming !== null
            checkable: true
            checked: naming !== null && naming.includeGeneric
            onTriggered: naming.includeGeneric = checked
        }
        MenuSeparator {}
        Action { text: "Journal des opérations…"; onTriggered: root.operationLog() }
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Écran « Doublons » (002, 006 : contracts/ui-contract.md) : niveaux, filtres combinés, vue
// compacte ou détaillée, sélection de groupes et actions en lot, panneau « Dossiers »,
// « Écartés de la revue ». Rien ne part à la corbeille sans passer par le récapitulatif.
FocusScope {
    id: root
    signal openItem(var id)
    signal back()

    property var expandedGroup: 0
    property int anchorRow: -1          // début d'une plage Maj + clic / Maj + flèche
    property bool panelOpen: false
    property bool asideMode: false
    property var toastBatch: 0
    readonly property bool compact: dups.view === "compact"

    onPanelOpenChanged: dups.foldersActive = panelOpen && visible
    onVisibleChanged: dups.foldersActive = panelOpen && visible
    onAsideModeChanged: if (asideMode) dups.refreshSetAside()

    function focusGroup(groupId) {
        asideMode = false
        const r = groupId ? dups.groups.rowOfGroup(groupId) : -1
        if (r >= 0) {
            list.currentIndex = r
            list.positionViewAtIndex(r, ListView.Beginning)
            if (root.compact)
                root.expandedGroup = groupId
        }
    }
    function currentLine() { return list.currentItem }
    function currentRow() { return list.currentItem ? list.currentItem.row : null }
    function currentGroupId() { return list.currentItem ? list.currentItem.groupId : 0 }
    // Récapitulatif : la sélection, sinon le groupe courant seulement (FR-015).
    function openSummary() {
        if (trash.busy)
            return
        const ids = dups.selectedCount > 0 ? dups.selectedGroupIds() : (currentGroupId() ? [currentGroupId()] : [])
        if (ids.length === 0)
            return
        trash.prepare(ids)
        summary.open()
    }
    function toast(message, batchId) {
        toastText.text = message
        root.toastBatch = batchId
        toastBox.visible = true
        toastTimer.restart()
    }
    function toggleSelectAt(row, modifiers) {
        const id = dups.groups.groupAt(row).groupId
        if ((modifiers & Qt.ShiftModifier) && root.anchorRow >= 0)
            dups.groups.selectRange(root.anchorRow, row)
        else
            dups.groups.toggle(id)
        root.anchorRow = row
    }

    Connections {
        target: dups
        function onNotice(message, batchId) { root.toast(message, batchId) }
    }

    Keys.onPressed: event => {
        if (root.asideMode) {
            if (event.key === Qt.Key_Escape) { root.asideMode = false; event.accepted = true }
            return
        }
        const line = currentLine()
        const row = currentRow()
        const cardKeys = !root.compact || (line && line.expanded)
        const ctrl = event.modifiers & Qt.ControlModifier
        const shift = event.modifiers & Qt.ShiftModifier
        switch (event.key) {
        case Qt.Key_Up:
        case Qt.Key_Down: {
            const from = list.currentIndex
            if (event.key === Qt.Key_Up) list.decrementCurrentIndex(); else list.incrementCurrentIndex()
            if (shift) {
                if (root.anchorRow < 0) root.anchorRow = from
                dups.groups.selectRange(root.anchorRow, list.currentIndex)
            } else {
                root.anchorRow = list.currentIndex
            }
            break
        }
        case Qt.Key_Left: if (cardKeys && row && row.currentCopy > 0) row.currentCopy--; break
        case Qt.Key_Right: if (cardKeys && row && row.currentCopy < row.copyCount - 1) row.currentCopy++; break
        case Qt.Key_Space:
            if (cardKeys && row) row.toggleCurrent()
            else if (line) toggleSelectAt(list.currentIndex, 0)
            break
        case Qt.Key_A:
            if (!ctrl) return
            dups.groups.selectAll()
            break
        case Qt.Key_Z:
            if (!ctrl || !toastBox.visible || !root.toastBatch) return
            dups.undoBatch(root.toastBatch)
            toastBox.visible = false
            break
        case Qt.Key_D: root.panelOpen = !root.panelOpen; break
        case Qt.Key_K: if (cardKeys && row) row.keepCurrent(); break
        case Qt.Key_C: if (cardKeys && row) row.compareCurrent(); break
        case Qt.Key_Return:
        case Qt.Key_Enter:
            if (root.compact && line)
                root.expandedGroup = line.expanded ? 0 : line.groupId
            else if (row && row.copies[row.currentCopy])
                root.openItem(row.copies[row.currentCopy].id)
            break
        case Qt.Key_Delete: root.openSummary(); break
        case Qt.Key_Escape:
            if (dups.selectedCount > 0) dups.groups.clearSelection()
            else if (root.expandedGroup) root.expandedGroup = 0
            else if (root.panelOpen) root.panelOpen = false
            else root.back()
            break
        default: return
        }
        event.accepted = true
    }

    Rectangle { anchors.fill: parent; color: Theme.bg }

    component Segment: Button {
        id: seg
        property bool selected: false
        property string count: ""
        implicitHeight: 34
        leftPadding: 14
        rightPadding: 14
        focusPolicy: Qt.NoFocus
        Accessible.checked: selected
        contentItem: Label {
            text: seg.text + (seg.count !== "" ? "  <font color='" + Theme.text3 + "'>" + seg.count + "</font>" : "")
            textFormat: Text.StyledText
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            color: seg.selected ? "#FFFFFF" : Theme.text2
            font.weight: seg.selected ? Font.DemiBold : Font.Normal
        }
        background: Rectangle {
            radius: 8
            color: seg.selected ? "#2A3240" : (seg.hovered ? Theme.hover : "transparent")
        }
    }

    component ToolBtn: Button {
        id: tb
        property bool active: false
        implicitHeight: 36
        leftPadding: 14
        rightPadding: 14
        focusPolicy: Qt.NoFocus
        contentItem: Label { text: tb.text; color: tb.enabled ? Theme.text : Theme.text3; verticalAlignment: Text.AlignVCenter }
        background: Rectangle {
            radius: 8
            color: tb.active ? Theme.accentBg : (tb.hovered ? Theme.hover : Theme.surface)
            border.color: tb.active ? Theme.accent : Theme.lineStrong
        }
    }

    component BarBtn: Button {
        id: bb
        implicitHeight: 40
        leftPadding: 14
        rightPadding: 14
        focusPolicy: Qt.NoFocus
        contentItem: Label { text: bb.text; color: bb.enabled ? Theme.text : Theme.text3; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { radius: 10; color: bb.hovered ? Theme.hover : Theme.surface; border.color: Theme.lineStrong }
    }

    component PrimaryBtn: Button {
        id: pb
        implicitHeight: 40
        leftPadding: 18
        rightPadding: 18
        focusPolicy: Qt.NoFocus
        contentItem: Label {
            text: Theme.iconDelete + "   " + pb.text
            font.family: Theme.iconFont + ", " + Theme.font
            color: pb.enabled ? "#FFFFFF" : Theme.text3
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle { radius: 10; color: pb.enabled ? (pb.hovered ? Theme.accent : Theme.accentButton) : Theme.surface }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // En-tête : titre, résumé, niveaux, écartés.
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            Layout.topMargin: 18
            Layout.bottomMargin: 10
            spacing: 16
            ColumnLayout {
                spacing: 2
                Label {
                    text: "Doublons"
                    font.family: Theme.displayFont
                    font.pixelSize: 22
                    font.weight: Font.DemiBold
                    color: Theme.text
                }
                Label {
                    text: dups.summaryText + (dups.active ? " · " + dups.progressText : "")
                    color: Theme.text2
                }
            }
            Item { Layout.fillWidth: true }
            Rectangle {
                implicitHeight: 40
                implicitWidth: segments.implicitWidth + 6
                radius: 10
                color: Theme.field
                RowLayout {
                    id: segments
                    anchors.fill: parent
                    anchors.margins: 3
                    spacing: 3
                    Segment { text: "Tous"; count: dups.levelCounts.all; selected: !root.asideMode && dups.filter === "all"; onClicked: { root.asideMode = false; dups.filter = "all" } }
                    Segment { text: "Identiques"; count: dups.levelCounts.identical; selected: !root.asideMode && dups.filter === "identical"; onClicked: { root.asideMode = false; dups.filter = "identical" } }
                    Segment { text: "Même image"; count: dups.levelCounts.same_image; selected: !root.asideMode && dups.filter === "same_image"; onClicked: { root.asideMode = false; dups.filter = "same_image" } }
                    Segment { text: "Très ressemblantes"; count: dups.levelCounts.similar; selected: !root.asideMode && dups.filter === "similar"; onClicked: { root.asideMode = false; dups.filter = "similar" } }
                }
            }
            Rectangle { width: 1; height: 28; color: Theme.lineStrong }
            Segment {
                text: "Écartés"
                count: String(dups.setAsideCount)
                selected: root.asideMode
                onClicked: root.asideMode = !root.asideMode
            }
        }

        // Barre d'outils
        RowLayout {
            visible: !root.asideMode
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            Layout.bottomMargin: 8
            spacing: 8
            ToolBtn {
                text: "Dossiers"
                active: root.panelOpen
                onClicked: root.panelOpen = !root.panelOpen
            }
            ToolBtn {
                id: filtersBtn
                text: "Filtres  ▾"
                active: dups.typeFilter !== "any" || dups.stateFilter !== "any" || dups.minMegabytes > 0 || dups.yearFrom > 0 || dups.yearTo > 0
                onClicked: filtersMenu.popup(filtersBtn, 0, filtersBtn.height + 4)
            }
            ToolBtn {
                id: sortBtn
                text: "Tri : " + dups.sortLabel + "  ▾"
                onClicked: sortMenu.popup(sortBtn, 0, sortBtn.height + 4)
            }
            Item { Layout.fillWidth: true }
            Rectangle {
                implicitHeight: 36
                implicitWidth: viewSeg.implicitWidth + 6
                radius: 10
                color: Theme.field
                RowLayout {
                    id: viewSeg
                    anchors.fill: parent
                    anchors.margins: 3
                    spacing: 3
                    Segment { implicitHeight: 30; text: "Compacte"; selected: root.compact; onClicked: dups.view = "compact" }
                    Segment { implicitHeight: 30; text: "Détaillée"; selected: !root.compact; onClicked: dups.view = "detailed" }
                }
            }
        }

        // Puces de filtres et compte des groupes affichés
        Flow {
            visible: !root.asideMode
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            Layout.bottomMargin: 8
            spacing: 8
            Repeater {
                model: dups.chips
                delegate: Rectangle {
                    required property var modelData
                    height: 30
                    width: chipRow.implicitWidth + 18
                    radius: 15
                    color: modelData.key === "pair" || modelData.key === "folder" ? Theme.accentBg : Theme.surface
                    border.color: Theme.lineStrong
                    RowLayout {
                        id: chipRow
                        anchors.verticalCenter: parent.verticalCenter
                        x: 12
                        spacing: 6
                        Label {
                            text: modelData.label
                            color: modelData.key === "pair" || modelData.key === "folder" ? Theme.accentText : Theme.textSoft
                            elide: Text.ElideMiddle
                            Layout.maximumWidth: 520
                        }
                        ToolButton {
                            implicitWidth: 22
                            implicitHeight: 22
                            focusPolicy: Qt.NoFocus
                            Accessible.name: "Retirer le filtre " + modelData.label
                            contentItem: Label { text: "×"; color: Theme.text2; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { radius: 11; color: parent.hovered ? Theme.hover : "transparent" }
                            onClicked: dups.clearFilter(modelData.key)
                        }
                    }
                }
            }
            Label {
                height: 30
                verticalAlignment: Text.AlignVCenter
                text: dups.shownText
                color: Theme.text2
            }
        }

        // En-tête de colonnes (vue compacte)
        Rectangle {
            visible: !root.asideMode && root.compact
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: root.panelOpen ? 420 : 20
            Layout.preferredHeight: 34
            color: "transparent"
            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Theme.line }
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.line }
            RowLayout {
                anchors.fill: parent
                anchors.rightMargin: 16
                spacing: 16
                Item {
                    Layout.preferredWidth: 48
                    Layout.fillHeight: true
                    PzCheckBox {
                        anchors.centerIn: parent
                        focusPolicy: Qt.NoFocus
                        Accessible.name: "Tout sélectionner"
                        checked: dups.groups.count > 0 && dups.selectedCount === dups.groups.count
                        onToggled: {
                            if (dups.selectedCount === dups.groups.count) dups.groups.clearSelection(); else dups.groups.selectAll()
                            checked = Qt.binding(() => dups.groups.count > 0 && dups.selectedCount === dups.groups.count)
                        }
                    }
                }
                Label { Layout.preferredWidth: list.width < 1150 ? 112 : 132; text: "Niveau"; color: Theme.text3; font.pixelSize: 12 }
                Label { Layout.preferredWidth: list.width < 1150 ? 138 : 232; text: "Copies"; color: Theme.text3; font.pixelSize: 12 }
                Label { Layout.preferredWidth: list.width < 1150 ? 118 : 150; text: "Place"; color: Theme.text3; font.pixelSize: 12 }
                Label { Layout.fillWidth: true; Layout.preferredWidth: 1; text: "Copie gardée"; color: Theme.text3; font.pixelSize: 12 }
                Label { Layout.fillWidth: true; Layout.preferredWidth: 1; text: "Part à la corbeille"; color: Theme.text3; font.pixelSize: 12 }
                Item { Layout.preferredWidth: 14 }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ListView {
                    id: list
                    visible: !root.asideMode
                    anchors.fill: parent
                    anchors.leftMargin: 20
                    anchors.rightMargin: 20
                    anchors.topMargin: 8
                    clip: true
                    spacing: root.compact ? 6 : 12
                    model: dups.groups
                    boundsBehavior: Flickable.StopAtBounds
                    highlightFollowsCurrentItem: false
                    keyNavigationEnabled: false
                    cacheBuffer: 800
                    ScrollBar.vertical: ScrollBar {}
                    delegate: DuplicateGroupLine {
                        compact: root.compact
                        expanded: root.expandedGroup === groupId
                        current: ListView.isCurrentItem
                        onActivated: { list.currentIndex = index; root.forceActiveFocus() }
                        onToggleSelect: modifiers => root.toggleSelectAt(index, modifiers)
                        onToggleExpand: root.expandedGroup = expanded ? 0 : groupId
                        onOpenItem: id => root.openItem(id)
                        onCompare: (groupId, a, b) => compareView.show(groupId, a, b)
                    }

                    Column {
                        visible: list.count === 0
                        anchors.centerIn: parent
                        spacing: 8
                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: dups.groupCount > 0 ? "Aucun groupe à revoir dans ce filtre." : "Aucun doublon trouvé."
                            font.pixelSize: 18
                            color: Theme.text
                        }
                        Label {
                            visible: dups.chips.length > 0
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "Retirez un filtre pour voir plus de groupes."
                            color: Theme.text2
                        }
                        Label {
                            visible: dups.active || dups.waiting
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "L'analyse continue…"
                            color: Theme.text2
                        }
                    }
                }

                SetAsideView {
                    visible: root.asideMode
                    anchors.fill: parent
                }
            }

            FolderPanel {
                visible: root.panelOpen && !root.asideMode
                Layout.preferredWidth: 400
                Layout.fillHeight: true
                onClose: root.panelOpen = false
            }
        }

        // Barre du bas : opération en cours, sélection, ou groupe courant.
        Rectangle {
            visible: !root.asideMode
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            color: Theme.panel
            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Theme.line }

            // Pendant une opération (FR-019)
            RowLayout {
                visible: trash.busy
                anchors.fill: parent
                anchors.leftMargin: 24
                anchors.rightMargin: 24
                spacing: 16
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    Label { text: "Mise à la corbeille : " + trash.progressText; color: Theme.text; font.weight: Font.DemiBold }
                    ProgressBar { Layout.fillWidth: true; from: 0; to: 1; value: trash.progressValue }
                }
                Label { text: "Picazoo reste utilisable pendant l'opération"; color: Theme.text2; font.pixelSize: 12 }
                BarBtn { text: "■  Arrêter"; onClicked: trash.stop() }
            }

            // Avec sélection (US1)
            RowLayout {
                visible: !trash.busy && dups.selectedCount > 0
                anchors.fill: parent
                anchors.leftMargin: 24
                anchors.rightMargin: 24
                spacing: 10
                ColumnLayout {
                    spacing: 1
                    Label { text: dups.selectionText; color: Theme.text; font.weight: Font.DemiBold }
                    Label { text: dups.selectionDetail; color: Theme.text2; font.pixelSize: 12 }
                }
                ToolButton {
                    focusPolicy: Qt.NoFocus
                    Accessible.name: "Vider la sélection"
                    contentItem: Label { text: "×"; color: Theme.text2; font.pixelSize: 16; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { radius: 8; color: parent.hovered ? Theme.hover : "transparent" }
                    onClicked: dups.groups.clearSelection()
                }
                Rectangle { width: 1; height: 28; color: Theme.lineStrong }
                BarBtn { text: "Revenir à la suggestion"; onClicked: dups.batchReset() }
                BarBtn {
                    id: keepFolderBtn
                    text: "Garder les copies du dossier…  ▾"
                    onClicked: {
                        keepFolderMenu.folders = dups.selectionFolders()
                        keepFolderMenu.popup()
                    }
                }
                BarBtn { text: "Garder toutes"; onClicked: dups.batchKeepAll() }
                BarBtn { text: "Ce ne sont pas des doublons"; onClicked: dups.batchNotDuplicates() }
                Item { Layout.fillWidth: true }
                PrimaryBtn { text: "Mettre à la corbeille…"; onClicked: root.openSummary() }
            }

            // Sans sélection
            RowLayout {
                visible: !trash.busy && dups.selectedCount === 0
                anchors.fill: parent
                anchors.leftMargin: 24
                anchors.rightMargin: 24
                spacing: 10
                readonly property var cur: list.currentItem
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1
                    Label {
                        Layout.fillWidth: true
                        text: parent.parent.cur ? "Groupe courant : " + parent.parent.cur.keeperName : dups.trashText
                        color: Theme.text
                        font.weight: Font.DemiBold
                        elide: Text.ElideMiddle
                    }
                    Label {
                        text: (parent.parent.cur ? parent.parent.cur.reclaimableText + " récupérables · " : "") + "cochez des groupes pour agir en lot"
                        color: Theme.text2
                        font.pixelSize: 12
                    }
                }
                BarBtn { text: "Tout sélectionner"; enabled: list.count > 0; onClicked: dups.groups.selectAll() }
                PrimaryBtn {
                    text: "Mettre à la corbeille… (ce groupe)"
                    enabled: list.currentItem !== null && list.count > 0
                    onClicked: root.openSummary()
                }
            }
        }
    }

    // Message (« Annuler » pour une action en lot)
    Rectangle {
        id: toastBox
        visible: false
        z: 10
        x: 24
        y: root.height - 64 - height - 16
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
            Label { id: toastText; color: Theme.text; Layout.maximumWidth: root.width - 260; elide: Text.ElideRight }
            Button {
                visible: root.toastBatch !== 0
                text: "Annuler"
                focusPolicy: Qt.NoFocus
                flat: true
                contentItem: Label { text: "Annuler"; color: Theme.accentText; font.weight: Font.DemiBold; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { radius: 8; color: parent.hovered ? Theme.hover : "transparent" }
                onClicked: { dups.undoBatch(root.toastBatch); toastBox.visible = false }
            }
            ToolButton {
                focusPolicy: Qt.NoFocus
                Accessible.name: "Fermer le message"
                contentItem: Label { text: "×"; color: Theme.text2; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { radius: 8; color: parent.hovered ? Theme.hover : "transparent" }
                onClicked: toastBox.visible = false
            }
        }
        Timer { id: toastTimer; interval: 10000; onTriggered: toastBox.visible = false }
    }

    Menu {
        id: filtersMenu
        Menu {
            title: "Type"
            Action { text: "Tous"; checkable: true; checked: dups.typeFilter === "any"; onTriggered: dups.typeFilter = "any" }
            Action { text: "Photos"; checkable: true; checked: dups.typeFilter === "photos"; onTriggered: dups.typeFilter = "photos" }
            Action { text: "Vidéos"; checkable: true; checked: dups.typeFilter === "videos"; onTriggered: dups.typeFilter = "videos" }
            Action { text: "RAW + JPEG"; checkable: true; checked: dups.typeFilter === "raw_jpeg"; onTriggered: dups.typeFilter = "raw_jpeg" }
        }
        Menu {
            title: "État"
            Action { text: "Tous"; checkable: true; checked: dups.stateFilter === "any"; onTriggered: dups.stateFilter = "any" }
            Action { text: "Suggestion intacte"; checkable: true; checked: dups.stateFilter === "untouched"; onTriggered: dups.stateFilter = "untouched" }
            Action { text: "Choix modifié"; checkable: true; checked: dups.stateFilter === "modified"; onTriggered: dups.stateFilter = "modified" }
            Action { text: "Signalés"; checkable: true; checked: dups.stateFilter === "flagged"; onTriggered: dups.stateFilter = "flagged" }
        }
        Menu {
            title: "Taille minimale"
            Action { text: "Aucune"; checkable: true; checked: dups.minMegabytes === 0; onTriggered: dups.minMegabytes = 0 }
            Action { text: "1 Mo"; checkable: true; checked: dups.minMegabytes === 1; onTriggered: dups.minMegabytes = 1 }
            Action { text: "10 Mo"; checkable: true; checked: dups.minMegabytes === 10; onTriggered: dups.minMegabytes = 10 }
            Action { text: "100 Mo"; checkable: true; checked: dups.minMegabytes === 100; onTriggered: dups.minMegabytes = 100 }
            Action { text: "1 Go"; checkable: true; checked: dups.minMegabytes === 1000; onTriggered: dups.minMegabytes = 1000 }
        }
        Menu {
            title: "Dates"
            readonly property int year: new Date().getFullYear()
            Action { text: "Toutes"; checkable: true; checked: dups.yearFrom === 0 && dups.yearTo === 0; onTriggered: dups.setYears(0, 0) }
            Action { text: "Cette année"; onTriggered: dups.setYears(new Date().getFullYear(), new Date().getFullYear()) }
            Action { text: "L'an dernier"; onTriggered: dups.setYears(new Date().getFullYear() - 1, new Date().getFullYear() - 1) }
            Action { text: "Plage d'années…"; onTriggered: yearsDialog.open() }
        }
        MenuSeparator {}
        Action { text: "Retirer tous les filtres"; enabled: dups.chips.length > 0; onTriggered: dups.clearFilter("all") }
    }

    Menu {
        id: sortMenu
        Action { text: "Place récupérable"; checkable: true; checked: dups.sort === "reclaimable"; onTriggered: dups.sort = "reclaimable" }
        Action { text: "Nombre de copies"; checkable: true; checked: dups.sort === "copies"; onTriggered: dups.sort = "copies" }
        Action { text: "Date"; checkable: true; checked: dups.sort === "date"; onTriggered: dups.sort = "date" }
        Action { text: "Dossier"; checkable: true; checked: dups.sort === "folder"; onTriggered: dups.sort = "folder" }
    }

    Menu {
        id: keepFolderMenu
        property var folders: []
        Instantiator {
            model: keepFolderMenu.folders
            delegate: MenuItem {
                required property var modelData
                text: modelData.label + "  (" + modelData.count + ")"
                onTriggered: dups.batchKeepFolder(modelData.path)
            }
            onObjectAdded: (index, object) => keepFolderMenu.insertItem(index, object)
            onObjectRemoved: (index, object) => keepFolderMenu.removeItem(object)
        }
    }

    Dialog {
        id: yearsDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        title: "Plage d'années"
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAboutToShow: {
            fromBox.value = dups.yearFrom > 0 ? dups.yearFrom : new Date().getFullYear() - 5
            toBox.value = dups.yearTo > 0 ? dups.yearTo : new Date().getFullYear()
        }
        onAccepted: dups.setYears(fromBox.value, toBox.value)
        RowLayout {
            spacing: 12
            Label { text: "De" }
            SpinBox { id: fromBox; from: 1900; to: 2100; editable: true; textFromValue: v => String(v) }
            Label { text: "à" }
            SpinBox { id: toBox; from: 1900; to: 2100; editable: true; textFromValue: v => String(v) }
        }
    }

    TrashSummary {
        id: summary
        onClosed: root.forceActiveFocus()
    }

    CompareView {
        id: compareView
        onClosed: root.forceActiveFocus()
    }
}

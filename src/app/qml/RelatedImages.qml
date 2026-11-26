import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Écran « Images liées » (003, contracts/ui-contract.md) : groupes proposés d'après les noms de
// fichiers, à confirmer ou corriger. Aucune action ne modifie, ne déplace ni ne renomme un fichier.
FocusScope {
    id: root
    signal openItem(var id)
    signal back()

    property var renamingGroup: 0
    property var splitGroup: 0
    property var splitIds: []
    // Dernier « Ce n'est pas un groupe », annulable depuis le message en bas.
    property string undoKey: ""

    function focusGroup(groupId) {
        if (groupId)
            naming.filter = "all"
        const r = groupId ? naming.groups.rowOfGroup(groupId) : -1
        if (r >= 0) {
            list.currentIndex = r
            list.positionViewAtIndex(r, ListView.Beginning)
        }
    }
    function endRename(groupId, name) {
        root.renamingGroup = 0
        root.forceActiveFocus()
        if (name.trim() !== "")
            naming.rename(groupId, name)
    }
    function startSplit(groupId) {
        root.splitIds = []
        root.splitGroup = groupId
    }
    function toggleSplit(id) {
        const next = root.splitIds.slice()
        const i = next.indexOf(id)
        if (i >= 0) next.splice(i, 1); else next.push(id)
        root.splitIds = next
    }
    function endSplit() {
        root.splitGroup = 0
        root.splitIds = []
        root.forceActiveFocus()
    }

    Keys.onPressed: event => {
        switch (event.key) {
        case Qt.Key_Up: list.decrementCurrentIndex(); break
        case Qt.Key_Down: list.incrementCurrentIndex(); break
        case Qt.Key_F2: if (list.currentItem) root.renamingGroup = list.currentItem.groupId; break
        case Qt.Key_Escape:
            if (root.splitGroup) root.endSplit()
            else if (root.renamingGroup) root.renamingGroup = 0
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
        Accessible.name: text + " " + count
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
        implicitHeight: 36
        leftPadding: 14
        rightPadding: 14
        focusPolicy: Qt.NoFocus
        Accessible.name: text
        contentItem: Label { text: tb.text; color: tb.enabled ? Theme.text : Theme.text3; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { radius: 8; color: tb.hovered ? Theme.hover : Theme.surface; border.color: Theme.lineStrong }
    }

    component Sheet: Popup {
        property string title
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        focus: true
        padding: 24
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onClosed: root.forceActiveFocus()
        background: Rectangle { radius: 14; color: Theme.panel; border.color: Theme.lineStrong }
    }

    component SheetTitle: Label {
        Layout.fillWidth: true
        elide: Text.ElideRight
        font.family: Theme.displayFont
        font.pixelSize: 20
        font.weight: Font.DemiBold
        color: Theme.text
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

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
                    text: "Images liées"
                    font.family: Theme.displayFont
                    font.pixelSize: 22
                    font.weight: Font.DemiBold
                    color: Theme.text
                }
                Label {
                    text: naming.summaryText + " · d'après les noms de fichiers"
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
                    Segment { text: "Tous"; count: naming.counts.all; selected: naming.filter === "all"; onClicked: naming.filter = "all" }
                    Segment { text: "Séries régulières"; count: naming.counts.regular_series; selected: naming.filter === "regular_series"; onClicked: naming.filter = "regular_series" }
                    Segment { text: "Même racine"; count: naming.counts.same_root; selected: naming.filter === "same_root"; onClicked: naming.filter = "same_root" }
                    Segment {
                        visible: naming.includeGeneric || naming.counts.generic_root > 0
                        text: "Racines génériques"
                        count: naming.counts.generic_root
                        selected: naming.filter === "generic_root"
                        onClicked: naming.filter = "generic_root"
                    }
                }
            }
            ToolBtn { text: "Rechercher maintenant"; onClicked: naming.rescan() }
            ToolBtn {
                text: "Décisions (" + naming.decisions.length + ")"
                enabled: naming.decisions.length > 0
                onClicked: decisionsSheet.open()
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 20
            Layout.rightMargin: 8
            Layout.bottomMargin: 12
            clip: true
            spacing: 12
            model: naming.groups
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            delegate: Item {
                id: line
                required property int index
                required property var groupId
                required property string name
                required property string confidence
                required property string confidenceLabel
                required property bool confirmed
                required property int memberCount
                required property var members
                width: ListView.view.width - 12
                height: card.height

                RelatedGroupRow {
                    id: card
                    width: parent.width
                    groupId: line.groupId
                    name: line.name
                    confidence: line.confidence
                    confidenceLabel: line.confidenceLabel
                    confirmed: line.confirmed
                    memberCount: line.memberCount
                    members: line.members
                    current: list.currentIndex === line.index
                    renaming: root.renamingGroup === line.groupId
                    splitting: root.splitGroup === line.groupId
                    splitIds: root.splitIds
                    onActivated: { list.currentIndex = line.index; if (!renaming && !splitting) root.forceActiveFocus() }
                    onOpenItem: id => root.openItem(id)
                    onRenameRequested: root.renamingGroup = line.groupId
                    onRenameDone: name => root.endRename(line.groupId, name)
                    onMergeRequested: mergeSheet.ask(line.groupId, line.name)
                    onSplitRequested: root.startSplit(line.groupId)
                    onSplitToggled: id => root.toggleSplit(id)
                    onSplitConfirmed: name => {
                        if (naming.split(line.groupId, root.splitIds, name))
                            root.endSplit()
                    }
                    onSplitCancelled: root.endSplit()
                    onAddRequested: addSheet.ask(line.groupId, line.name)
                }
            }

            Label {
                visible: list.count === 0
                anchors.centerIn: parent
                width: parent.width - 64
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: naming.groupCount > 0 ? "Aucun groupe dans ce filtre." : "Aucune image liée trouvée pour l'instant."
                color: Theme.text2
                font.pixelSize: 16
            }
        }
    }

    // « Ajouter des éléments… » : recherche par nom parmi les éléments absents du groupe.
    Sheet {
        id: addSheet
        property var groupId: 0
        property var results: []
        property var chosen: []
        function ask(id, name) {
            groupId = id
            title = name
            chosen = []
            search.text = ""
            results = naming.searchItems(id, "")
            open()
            search.forceActiveFocus()
        }
        function toggle(id) {
            const next = chosen.slice()
            const i = next.indexOf(id)
            if (i >= 0) next.splice(i, 1); else next.push(id)
            chosen = next
        }
        width: Math.min(860, parent ? parent.width - 64 : 860)
        height: Math.min(640, parent ? parent.height - 64 : 640)

        ColumnLayout {
            anchors.fill: parent
            spacing: 14
            SheetTitle { text: "Ajouter des éléments à « " + addSheet.title + " »" }
            PzTextField {
                id: search
                Layout.fillWidth: true
                placeholderText: "Rechercher par nom de fichier"
                onTextEdited: searchDelay.restart()
                Timer {
                    id: searchDelay
                    interval: 200
                    onTriggered: addSheet.results = naming.searchItems(addSheet.groupId, search.text)
                }
            }
            GridView {
                id: found
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                cellWidth: 160
                cellHeight: 160
                model: addSheet.results
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}
                delegate: Rectangle {
                    id: cell
                    required property var modelData
                    readonly property bool picked: addSheet.chosen.indexOf(modelData.id) >= 0
                    width: 150
                    height: 150
                    radius: 8
                    color: picked ? Theme.accentBg : Theme.surface
                    border.color: picked ? Theme.accentText : Theme.line
                    border.width: picked ? 2 : 1
                    Accessible.role: Accessible.CheckBox
                    Accessible.name: modelData.name
                    Accessible.checked: picked
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 6
                        spacing: 4
                        Image {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            fillMode: Image.PreserveAspectFit
                            asynchronous: true
                            source: cell.modelData.thumb
                            sourceSize: Qt.size(320, 320)
                        }
                        Label {
                            Layout.fillWidth: true
                            text: cell.modelData.name
                            color: Theme.text2
                            font.pixelSize: 12
                            elide: Text.ElideMiddle
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                    MouseArea {
                        id: cellMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: addSheet.toggle(cell.modelData.id)
                        ToolTip.visible: containsMouse
                        ToolTip.delay: 800
                        ToolTip.text: cell.modelData.folder + "\\" + cell.modelData.name
                    }
                }
                Label {
                    visible: found.count === 0
                    anchors.centerIn: parent
                    text: "Aucun élément à ajouter."
                    color: Theme.text2
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                Label {
                    Layout.fillWidth: true
                    text: addSheet.results.length >= 200 ? "200 premiers résultats : précisez la recherche." : ""
                    color: Theme.text3
                    font.pixelSize: 12
                }
                ToolBtn { text: "Annuler"; onClicked: addSheet.close() }
                ToolBtn {
                    text: "Ajouter (" + addSheet.chosen.length + ")"
                    enabled: addSheet.chosen.length > 0
                    onClicked: {
                        naming.addItems(addSheet.groupId, addSheet.chosen)
                        addSheet.close()
                    }
                }
            }
        }
    }

    // « Fusionner avec… » : choisir l'autre groupe, puis le nom du groupe réuni.
    Sheet {
        id: mergeSheet
        property var groupId: 0
        property var results: []
        property var otherId: 0
        function ask(id, name) {
            groupId = id
            title = name
            otherId = 0
            mergeSearch.text = ""
            mergedName.text = name
            results = naming.otherGroups(id, "")
            open()
            mergeSearch.forceActiveFocus()
        }
        width: Math.min(620, parent ? parent.width - 64 : 620)
        height: Math.min(560, parent ? parent.height - 64 : 560)

        ColumnLayout {
            anchors.fill: parent
            spacing: 14
            SheetTitle { text: "Fusionner « " + mergeSheet.title + " » avec…" }
            PzTextField {
                id: mergeSearch
                Layout.fillWidth: true
                placeholderText: "Rechercher un groupe"
                onTextEdited: {
                    mergeSheet.otherId = 0
                    mergeSheet.results = naming.otherGroups(mergeSheet.groupId, text)
                }
            }
            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 2
                model: mergeSheet.results
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}
                delegate: ItemDelegate {
                    id: choice
                    required property var modelData
                    width: ListView.view.width
                    Accessible.name: modelData.name
                    contentItem: Label { text: choice.modelData.name + "  ·  " + choice.modelData.count + " éléments"; color: Theme.text; elide: Text.ElideRight }
                    background: Rectangle { radius: 8; color: mergeSheet.otherId === choice.modelData.id ? Theme.accentBg : (choice.hovered ? Theme.hover : "transparent") }
                    onClicked: mergeSheet.otherId = modelData.id
                }
            }
            Label { text: "Nom du groupe réuni"; color: Theme.text2 }
            PzTextField {
                id: mergedName
                Layout.fillWidth: true
                Accessible.name: "Nom du groupe réuni"
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                Item { Layout.fillWidth: true }
                ToolBtn { text: "Annuler"; onClicked: mergeSheet.close() }
                ToolBtn {
                    text: "Fusionner"
                    enabled: mergeSheet.otherId !== 0 && mergedName.text.trim() !== ""
                    onClicked: {
                        naming.merge(mergeSheet.groupId, mergeSheet.otherId, mergedName.text)
                        mergeSheet.close()
                    }
                }
            }
        }
    }

    // Décisions durables, annulables (FR-009).
    Sheet {
        id: decisionsSheet
        width: Math.min(620, parent ? parent.width - 64 : 620)
        height: Math.min(480, parent ? parent.height - 64 : 480)

        ColumnLayout {
            anchors.fill: parent
            spacing: 14
            SheetTitle { text: "Décisions" }
            Label {
                visible: naming.decisions.length === 0
                text: "Aucune décision à annuler."
                color: Theme.text2
            }
            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 6
                model: naming.decisions
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}
                delegate: RowLayout {
                    id: decision
                    required property var modelData
                    width: ListView.view.width
                    spacing: 12
                    Label { Layout.fillWidth: true; text: decision.modelData.text; color: Theme.text; wrapMode: Text.WordWrap }
                    ToolBtn { text: "Annuler la décision"; onClicked: naming.undoDecision(decision.modelData.key) }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                ToolBtn { text: "Fermer"; onClicked: decisionsSheet.close() }
            }
        }
    }
}

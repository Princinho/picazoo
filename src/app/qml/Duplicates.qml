import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Écran « Doublons » (US1 à US5, contracts/ui-contract.md) : filtres, groupes, barre
// d'action. Rien ne part à la corbeille sans passer par le récapitulatif.
FocusScope {
    id: root
    signal openItem(var id)
    signal back()

    function focusGroup(groupId) {
        const r = groupId ? dups.groups.rowOfGroup(groupId) : -1
        if (r >= 0) {
            list.currentIndex = r
            list.positionViewAtIndex(r, ListView.Beginning)
        }
    }
    function currentRow() { return list.currentItem }
    function openSummary(identicalOnly) {
        summary.plan = identicalOnly ? trash.prepareAllIdentical() : trash.prepare([])
        summary.open()
    }

    Keys.onPressed: event => {
        const row = currentRow()
        switch (event.key) {
        case Qt.Key_Up: list.decrementCurrentIndex(); break
        case Qt.Key_Down: list.incrementCurrentIndex(); break
        case Qt.Key_Left: if (row && row.currentCopy > 0) row.currentCopy--; break
        case Qt.Key_Right: if (row && row.currentCopy < row.copyCount - 1) row.currentCopy++; break
        case Qt.Key_Space: if (row) row.toggleCurrent(); break
        case Qt.Key_K: if (row) row.keepCurrent(); break
        case Qt.Key_C: if (row) row.compareCurrent(); break
        case Qt.Key_Return:
        case Qt.Key_Enter: if (row && row.copies[row.currentCopy]) root.openItem(row.copies[row.currentCopy].id); break
        case Qt.Key_Delete: root.openSummary(false); break
        case Qt.Key_Escape: root.back(); break
        default: return
        }
        event.accepted = true
    }

    Rectangle { anchors.fill: parent; color: Theme.bg }

    component Segment: Button {
        id: seg
        property bool selected: false
        implicitHeight: 34
        leftPadding: 14
        rightPadding: 14
        focusPolicy: Qt.NoFocus
        Accessible.checked: selected
        contentItem: Label {
            text: seg.text
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

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // En-tête : titre, résumé, filtres.
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 20
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
                    Segment { text: "Tous"; selected: dups.filter === "all"; onClicked: dups.filter = "all" }
                    Segment { text: "Identiques"; selected: dups.filter === "identical"; onClicked: dups.filter = "identical" }
                    Segment { text: "Même image"; selected: dups.filter === "same_image"; onClicked: dups.filter = "same_image" }
                    Segment { text: "Très ressemblantes"; selected: dups.filter === "similar"; onClicked: dups.filter = "similar" }
                }
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            clip: true
            spacing: 12
            model: dups.groups
            boundsBehavior: Flickable.StopAtBounds
            highlightFollowsCurrentItem: false
            keyNavigationEnabled: false
            ScrollBar.vertical: ScrollBar {}
            delegate: DuplicateGroupRow {
                current: ListView.isCurrentItem
                onActivated: { list.currentIndex = index; root.forceActiveFocus() }
                onOpenItem: id => root.openItem(id)
                onCompare: (groupId, a, b) => compareView.show(groupId, a, b)
            }

            Column {
                visible: list.count === 0
                anchors.centerIn: parent
                spacing: 8
                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Aucun doublon trouvé."
                    font.pixelSize: 18
                    color: Theme.text
                }
                Label {
                    visible: dups.active || dups.waiting
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "L'analyse continue…"
                    color: Theme.text2
                }
            }
        }

        // Barre d'action.
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            color: Theme.panel
            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Theme.line }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 24
                anchors.rightMargin: 24
                spacing: 16
                Label {
                    text: trash.busy ? "Opération en cours… " + trash.progressText : dups.trashText
                    color: Theme.text2
                }
                Item { Layout.fillWidth: true }
                Button {
                    id: allIdentical
                    visible: dups.identicalGroupCount > 0
                    enabled: !trash.busy
                    implicitHeight: 40
                    leftPadding: 16
                    rightPadding: 16
                    focusPolicy: Qt.NoFocus
                    onClicked: root.openSummary(true)
                    contentItem: Label { text: "Appliquer les suggestions à tous les groupes identiques…"; color: allIdentical.enabled ? Theme.text : Theme.text3; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { radius: 10; color: allIdentical.hovered ? Theme.hover : Theme.surface; border.color: Theme.lineStrong }
                }
                Button {
                    id: trashBtn
                    enabled: dups.trashCount > 0 && !trash.busy
                    implicitHeight: 40
                    leftPadding: 18
                    rightPadding: 18
                    focusPolicy: Qt.NoFocus
                    onClicked: root.openSummary(false)
                    contentItem: Label {
                        text: Theme.iconDelete + "   Mettre à la corbeille…"
                        font.family: Theme.iconFont + ", " + Theme.font
                        color: trashBtn.enabled ? "#FFFFFF" : Theme.text3
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle { radius: 10; color: trashBtn.enabled ? (trashBtn.hovered ? Theme.accent : Theme.accentButton) : Theme.surface }
                }
            }
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

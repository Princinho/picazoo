import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Un groupe de doublons (contracts/ui-contract.md) : niveau de confiance, place récupérable,
// copie gardée (étoile) et copies côte à côte avec leurs informations.
Rectangle {
    id: row
    required property int index
    required property var groupId
    required property string level
    required property string levelLabel
    required property string reasonLabel
    required property string reclaimableText
    required property int copyCount
    required property var copies
    property bool selected: false     // 006 : groupe sélectionné
    property string flagText: ""      // 006 : signalement (règle, retouche probable)
    property bool embedded: false     // 006 : carte dépliée sous une ligne compacte (sélection portée par la ligne)

    property bool current: false
    property int currentCopy: 0
    signal openItem(var id)
    signal compare(var groupId, var a, var b)
    signal activated()
    signal toggleSelect(int modifiers)

    readonly property color badgeBg: level === "identical" ? Theme.okBg : (level === "same_image" ? Theme.accentBg : Theme.amberBg)
    readonly property color badgeText: level === "identical" ? Theme.okText : (level === "same_image" ? Theme.accentText : Theme.amberText)

    function keeperIndex() {
        for (let i = 0; i < copies.length; ++i)
            if (copies[i].isSuggested)
                return i
        return 0
    }
    function toggleCurrent() {
        const c = copies[currentCopy]
        if (c)
            setKeep(c, !c.keep)
    }
    function keepCurrent() {
        const c = copies[currentCopy]
        if (c)
            dups.keepOnly(groupId, c.id)
    }
    function compareCurrent() {
        const k = keeperIndex()
        const other = currentCopy !== k ? currentCopy : (k === 0 ? 1 : 0)
        if (copies.length >= 2)
            row.compare(groupId, copies[k], copies[other])
    }
    function setKeep(c, keep) {
        if (!dups.setKeep(groupId, c.id, keep)) {
            refusal.show("Gardez au moins une copie de chaque photo.")
        }
    }

    width: ListView.view ? ListView.view.width : (parent ? parent.width : 800)
    height: 410
    radius: Theme.radius
    color: Theme.panel
    border.color: current ? Theme.accent : (selected ? Theme.accentButton : Theme.line)
    border.width: current ? 2 : 1

    ToolTip {
        id: refusal
        timeout: 3000
        x: 16
        y: 44
    }

    MouseArea {
        anchors.fill: parent
        onClicked: row.activated()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            PzCheckBox {
                visible: !row.embedded
                checked: row.selected
                focusPolicy: Qt.NoFocus
                Accessible.name: "Sélectionner ce groupe"
                onToggled: {
                    row.toggleSelect(0)
                    checked = Qt.binding(() => row.selected)
                }
            }
            Rectangle {
                Layout.preferredHeight: 26
                Layout.preferredWidth: badge.implicitWidth + 20
                radius: 13
                color: row.badgeBg
                Label {
                    id: badge
                    anchors.centerIn: parent
                    text: row.level === "similar" ? row.levelLabel + " — à vérifier" : row.levelLabel
                    color: row.badgeText
                    font.weight: Font.DemiBold
                }
            }
            Label {
                text: row.copyCount + " copies · " + row.reclaimableText + " récupérables"
                color: Theme.text2
            }
            Label {
                visible: row.flagText !== "" && row.level !== "similar"
                text: "⚠  " + row.flagText
                color: Theme.amberText
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            Item { Layout.fillWidth: true }
            Button {
                id: compareBtn
                visible: row.copyCount >= 2
                focusPolicy: Qt.NoFocus
                implicitHeight: 32
                onClicked: { row.currentCopy = row.currentCopy === row.keeperIndex() ? (row.keeperIndex() === 0 ? 1 : 0) : row.currentCopy; row.compareCurrent() }
                contentItem: Label { text: "Comparer"; color: Theme.text; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { radius: 8; color: compareBtn.hovered ? Theme.hover : Theme.surface; border.color: Theme.lineStrong }
            }
            ToolButton {
                id: moreBtn
                focusPolicy: Qt.NoFocus
                Accessible.name: "Actions du groupe"
                contentItem: Label { text: Theme.iconMore; font.family: Theme.iconFont; color: Theme.text2; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { radius: 8; color: moreBtn.hovered ? Theme.hover : "transparent" }
                onClicked: groupMenu.popup(moreBtn, 0, moreBtn.height)
            }
        }

        ListView {
            id: copyList
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: ListView.Horizontal
            spacing: 12
            clip: true
            model: row.copies
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.horizontal: ScrollBar {}

            delegate: Rectangle {
                id: card
                required property var modelData
                required property int index
                readonly property bool selected: row.current && row.currentCopy === index
                width: 214
                height: copyList.height - 12
                radius: 8
                color: modelData.keep ? "#1B2330" : Theme.surface
                border.color: selected ? Theme.accentText : (modelData.keep ? Theme.accent : Theme.line)
                border.width: selected ? 2 : 1

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: mouse => {
                        row.activated()
                        row.currentCopy = card.index
                        if (mouse.button === Qt.RightButton)
                            copyMenu.popup()
                    }
                    onDoubleClicked: row.openItem(card.modelData.id)
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 4

                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 140
                        Image {
                            anchors.fill: parent
                            fillMode: Image.PreserveAspectFit
                            asynchronous: true
                            source: "image://thumb/" + card.modelData.id + "/" + card.modelData.version + "/done"
                            sourceSize: Qt.size(320, 320)
                        }
                        Rectangle {
                            visible: card.modelData.keep
                            anchors.top: parent.top
                            anchors.left: parent.left
                            width: keepLabel.implicitWidth + 16
                            height: 24
                            radius: 12
                            color: Theme.accentButton
                            Label {
                                id: keepLabel
                                anchors.centerIn: parent
                                text: Theme.iconStar + "  Gardée"
                                font.family: Theme.iconFont + ", " + Theme.font
                                color: "#FFFFFF"
                                font.pixelSize: 12
                            }
                        }
                    }
                    Label {
                        visible: card.modelData.isSuggested
                        Layout.fillWidth: true
                        text: "Suggérée : " + row.reasonLabel
                        color: Theme.accentText
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                    Label {
                        visible: !card.modelData.isSuggested && card.modelData.levelLabel !== ""
                        Layout.fillWidth: true
                        text: card.modelData.levelLabel
                        color: Theme.text3
                        font.pixelSize: 12
                    }
                    PzCheckBox {
                        id: trashBox
                        text: "À la corbeille"
                        checked: !card.modelData.keep
                        focusPolicy: Qt.NoFocus
                        onToggled: {
                            row.activated()
                            row.currentCopy = card.index
                            row.setKeep(card.modelData, !checked)
                            checked = Qt.binding(() => !card.modelData.keep)
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: card.modelData.name
                        font.weight: Font.DemiBold
                        color: Theme.text
                        elide: Text.ElideMiddle
                    }
                    Label {
                        Layout.fillWidth: true
                        text: card.modelData.folder
                        color: Theme.text2
                        font.pixelSize: 12
                        elide: Text.ElideLeft
                        MouseArea {
                            id: folderHover
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.NoButton
                        }
                        ToolTip.visible: folderHover.containsMouse
                        ToolTip.delay: 600
                        ToolTip.text: card.modelData.folderPath
                    }
                    Label {
                        Layout.fillWidth: true
                        text: (card.modelData.dimensions || "") + (card.modelData.dimensions ? " · " : "") + card.modelData.size
                        color: Theme.text3
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                    Label {
                        Layout.fillWidth: true
                        text: String(card.modelData.format).toUpperCase() + " · " + (card.modelData.date || "")
                        color: Theme.text3
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                    Item { Layout.fillHeight: true }
                }

                Menu {
                    id: copyMenu
                    Action { text: "Garder celle-ci"; onTriggered: dups.keepOnly(row.groupId, card.modelData.id) }
                    Action { text: "Retirer de ce groupe"; onTriggered: dups.removeFromGroup(row.groupId, card.modelData.id) }
                    MenuSeparator {}
                    Action { text: "Ouvrir"; onTriggered: row.openItem(card.modelData.id) }
                    Action { text: "Afficher dans l'Explorateur"; onTriggered: app.revealInExplorer(card.modelData.id) }
                }
            }
        }
    }

    Menu {
        id: groupMenu
        Action { text: "Comparer"; enabled: row.copyCount >= 2; onTriggered: row.compareCurrent() }
        Action { text: "Revenir à la suggestion"; onTriggered: dups.resetChoice(row.groupId) }
        MenuSeparator {}
        Action { text: "Garder toutes"; onTriggered: dups.keepAll(row.groupId) }
        Action { text: "Ce ne sont pas des doublons"; onTriggered: dups.markNotDuplicates(row.groupId) }
    }
}

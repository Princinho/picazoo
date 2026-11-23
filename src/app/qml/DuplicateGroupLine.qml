import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Un groupe de doublons (006, contracts/ui-contract.md) : en vue compacte, une ligne de 64 px qui
// se déplie sur place en carte détaillée de la 002 ; en vue détaillée, la carte seule.
Item {
    id: line
    required property int index
    required property var groupId
    required property string level
    required property string levelLabel
    required property string reasonLabel
    required property string reclaimableText
    required property int copyCount
    required property var copies
    required property bool selected
    required property string keeperName
    required property string keeperFolder
    required property string outText
    required property string flagText

    property bool compact: true
    property bool expanded: false
    property bool current: false
    readonly property bool showCard: !compact || expanded
    // Colonnes plus étroites quand la place manque (panneau « Dossiers » ouvert, petite fenêtre).
    readonly property bool narrow: width < 1150
    readonly property int levelWidth: narrow ? 112 : 132
    readonly property int thumbsWidth: narrow ? 138 : 232
    readonly property int placeWidth: narrow ? 118 : 150
    readonly property alias row: card

    signal activated()
    signal toggleSelect(int modifiers)
    signal toggleExpand()
    signal openItem(var id)
    signal compare(var groupId, var a, var b)

    readonly property color badgeBg: level === "identical" ? Theme.okBg : (level === "same_image" ? Theme.accentBg : Theme.amberBg)
    readonly property color badgeText: level === "identical" ? Theme.okText : (level === "same_image" ? Theme.accentText : Theme.amberText)

    width: ListView.view ? ListView.view.width : 800
    height: (compact ? 64 : 0) + (showCard ? card.height + (compact ? 8 : 0) : 0)

    // ---------------------------------------------------------------- Ligne compacte
    Rectangle {
        id: compactRow
        visible: line.compact
        width: parent.width
        height: 64
        radius: 10
        color: line.selected ? "#1B2330" : (rowMouse.containsMouse ? Theme.hover : Theme.panel)
        border.color: line.current ? Theme.accent : (line.selected ? Theme.accentButton : Theme.line)
        border.width: line.current ? 2 : 1

        MouseArea {
            id: rowMouse
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton
            cursorShape: Qt.PointingHandCursor
            onClicked: mouse => {
                line.activated()
                if (mouse.modifiers & (Qt.ShiftModifier | Qt.ControlModifier))
                    line.toggleSelect(mouse.modifiers)
                else
                    line.toggleExpand()
            }
        }

        RowLayout {
            anchors.fill: parent
            anchors.rightMargin: 16
            spacing: 16

            Item {
                Layout.preferredWidth: 48
                Layout.fillHeight: true
                PzCheckBox {
                    anchors.centerIn: parent
                    checked: line.selected
                    focusPolicy: Qt.NoFocus
                    Accessible.name: "Sélectionner le groupe " + line.keeperName
                    onToggled: {
                        line.activated()
                        line.toggleSelect(0)
                        checked = Qt.binding(() => line.selected)
                    }
                }
            }

            Item {
                Layout.preferredWidth: line.levelWidth
                Layout.fillHeight: true
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    height: 24
                    width: badge.implicitWidth + 20
                    radius: 12
                    color: line.badgeBg
                    Label {
                        id: badge
                        anchors.centerIn: parent
                        text: line.levelLabel
                        color: line.badgeText
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                    }
                }
            }

            Row {
                Layout.preferredWidth: line.thumbsWidth
                spacing: 6
                Repeater {
                    model: line.copies.slice(0, line.narrow ? 3 : 5)
                    delegate: Rectangle {
                        required property var modelData
                        required property int index
                        readonly property bool more: index === (line.narrow ? 2 : 4) && line.copies.length > (line.narrow ? 3 : 5)
                        width: 40
                        height: 40
                        radius: 6
                        color: Theme.surface
                        border.color: modelData.keep ? Theme.accent : "transparent"
                        border.width: modelData.keep ? 2 : 0
                        Image {
                            anchors.fill: parent
                            anchors.margins: 2
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            opacity: modelData.keep ? 1 : 0.5
                            source: "image://thumb/" + modelData.id + "/" + modelData.version + "/done"
                            sourceSize: Qt.size(96, 96)
                        }
                        Rectangle {
                            visible: modelData.keep && modelData.isSuggested && !parent.more
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: -4
                            width: 18
                            height: 18
                            radius: 9
                            color: Theme.accentButton
                            Label {
                                anchors.centerIn: parent
                                text: Theme.iconStar
                                font.family: Theme.iconFont
                                font.pixelSize: 10
                                color: "#FFFFFF"
                            }
                        }
                        Rectangle {
                            visible: parent.more
                            anchors.fill: parent
                            radius: 6
                            color: "#990E1013"
                            Label {
                                anchors.centerIn: parent
                                text: "+" + (line.copies.length - (line.narrow ? 2 : 4))
                                color: Theme.text
                                font.weight: Font.DemiBold
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.preferredWidth: line.placeWidth
                spacing: 2
                Label { text: line.copyCount + " copies"; color: Theme.text; font.weight: Font.DemiBold }
                Label { text: line.reclaimableText + " récupérables"; color: Theme.text2; font.pixelSize: 12 }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                spacing: 2
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    Label { text: Theme.iconStar; font.family: Theme.iconFont; font.pixelSize: 12; color: Theme.accentText }
                    Label { Layout.fillWidth: true; text: line.keeperName; color: Theme.text; font.weight: Font.DemiBold; elide: Text.ElideMiddle }
                }
                Label { Layout.fillWidth: true; text: line.keeperFolder; color: Theme.text2; font.pixelSize: 12; elide: Text.ElideLeft }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                spacing: 2
                Label { Layout.fillWidth: true; text: "→ " + line.outText; color: Theme.textSoft; elide: Text.ElideLeft }
                Label {
                    visible: line.flagText !== ""
                    Layout.fillWidth: true
                    text: "⚠  " + line.flagText
                    color: Theme.amberText
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }
            }

            Label {
                text: "▾"
                color: Theme.text2
                rotation: line.expanded ? 180 : 0
                font.pixelSize: 14
            }
        }
    }

    // ------------------------------------------------------------- Carte détaillée (002)
    DuplicateGroupRow {
        id: card
        visible: line.showCard
        x: line.compact ? 48 : 0
        y: line.compact ? 72 : 0
        width: line.width - x
        index: line.index
        groupId: line.groupId
        level: line.level
        levelLabel: line.levelLabel
        reasonLabel: line.reasonLabel
        reclaimableText: line.reclaimableText
        copyCount: line.copyCount
        copies: line.copies
        selected: line.selected
        flagText: line.flagText
        embedded: line.compact
        current: line.current && !line.compact
        onActivated: line.activated()
        onToggleSelect: modifiers => line.toggleSelect(modifiers)
        onOpenItem: id => line.openItem(id)
        onCompare: (groupId, a, b) => line.compare(groupId, a, b)
    }
}

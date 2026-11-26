import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Un groupe d'images liées (003, contracts/ui-contract.md) : nom suggéré modifiable en place,
// confiance en mots, vignettes dans l'ordre des noms. Aucune action ne touche un fichier.
Rectangle {
    id: row
    required property var groupId
    required property string name
    required property string confidence
    required property string confidenceLabel
    required property bool confirmed
    required property int memberCount
    required property var members

    property bool current: false
    property bool renaming: false
    // Scission : l'écran garde la sélection, une ligne recyclée la retrouve.
    property bool splitting: false
    property var splitIds: []

    signal activated()
    signal openItem(var id)
    signal renameRequested()
    signal renameDone(string name)   // chaîne vide : abandon
    signal mergeRequested()
    signal splitRequested()
    signal splitToggled(var id)
    signal splitConfirmed(string name)
    signal splitCancelled()
    signal addRequested()

    readonly property color badgeBg: confidence === "regular_series" ? Theme.okBg
                                     : (confidence === "same_root" ? Theme.accentBg : Theme.surface)
    readonly property color badgeText: confidence === "regular_series" ? Theme.okText
                                       : (confidence === "same_root" ? Theme.accentText : Theme.text2)

    width: ListView.view ? ListView.view.width : 800
    height: splitting ? 288 : 236
    radius: Theme.radius
    color: Theme.panel
    border.color: current ? Theme.accent : Theme.line
    border.width: current ? 2 : 1

    Accessible.role: Accessible.Grouping
    Accessible.name: name + ", " + confidenceLabel + ", " + memberCount + " éléments"

    component RowBtn: Button {
        id: rb
        implicitHeight: 32
        leftPadding: 12
        rightPadding: 12
        focusPolicy: Qt.NoFocus
        Accessible.name: text
        contentItem: Label { text: rb.text; color: rb.enabled ? Theme.text : Theme.text3; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { radius: 8; color: rb.hovered ? Theme.hover : Theme.surface; border.color: Theme.lineStrong }
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
            Label {
                visible: !row.renaming
                Layout.maximumWidth: 360
                text: row.name
                elide: Text.ElideRight
                font.family: Theme.displayFont
                font.pixelSize: 17
                font.weight: Font.DemiBold
                color: Theme.text
                MouseArea {
                    anchors.fill: parent
                    onDoubleClicked: row.renameRequested()
                    onClicked: row.activated()
                }
            }
            PzTextField {
                id: nameField
                visible: row.renaming
                Layout.preferredWidth: 320
                Accessible.name: "Nom du groupe"
                onVisibleChanged: {
                    if (visible) {
                        text = row.name
                        forceActiveFocus()
                        selectAll()
                    }
                }
                Component.onCompleted: if (row.renaming) { text = row.name; forceActiveFocus(); selectAll() }
                onAccepted: row.renameDone(text)
                Keys.onEscapePressed: row.renameDone("")
            }
            RowBtn { visible: row.renaming; text: "Enregistrer"; onClicked: row.renameDone(nameField.text) }
            RowBtn { visible: row.renaming; text: "Annuler"; onClicked: row.renameDone("") }
            Rectangle {
                Layout.preferredHeight: 26
                Layout.preferredWidth: badge.implicitWidth + 20
                radius: 13
                color: row.badgeBg
                Label {
                    id: badge
                    anchors.centerIn: parent
                    text: row.confidenceLabel
                    color: row.badgeText
                    font.weight: Font.DemiBold
                }
            }
            Label {
                text: row.memberCount + " éléments" + (row.confirmed ? " · ✓ Confirmé" : "")
                color: Theme.text2
            }
            Item { Layout.fillWidth: true }
            RowBtn { visible: !row.confirmed; text: "Confirmer"; onClicked: naming.confirmGroup(row.groupId) }
            RowBtn { text: "Ajouter des éléments…"; onClicked: row.addRequested() }
            ToolButton {
                id: moreBtn
                focusPolicy: Qt.NoFocus
                Accessible.name: "Actions du groupe " + row.name
                contentItem: Label { text: Theme.iconMore; font.family: Theme.iconFont; color: Theme.text2; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { radius: 8; color: moreBtn.hovered ? Theme.hover : "transparent" }
                onClicked: { row.activated(); groupMenu.popup(moreBtn, 0, moreBtn.height) }
            }
        }

        // Liste virtualisée : un groupe de plusieurs centaines d'éléments défile sans attente.
        ListView {
            id: thumbs
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: ListView.Horizontal
            spacing: 10
            clip: true
            model: row.members
            reuseItems: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.horizontal: ScrollBar {}

            delegate: Rectangle {
                id: card
                required property var modelData
                readonly property bool marked: row.splitting && row.splitIds.indexOf(modelData.id) >= 0
                property string freshThumb: "" // calculée après l'affichage de la carte
                onModelDataChanged: freshThumb = ""
                Connections {
                    target: naming
                    function onItemUpdated(itemId) {
                        if (itemId === card.modelData.id)
                            card.freshThumb = naming.thumbUrl(itemId)
                    }
                }
                width: 150
                height: thumbs.height - 12
                radius: 8
                color: Theme.surface
                border.color: marked ? Theme.accentText : Theme.line
                border.width: marked ? 2 : 1
                Accessible.role: Accessible.ListItem
                Accessible.name: modelData.name

                MouseArea {
                    id: cardMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    cursorShape: Qt.PointingHandCursor
                    onClicked: mouse => {
                        row.activated()
                        if (mouse.button === Qt.RightButton)
                            itemMenu.popup()
                        else if (row.splitting)
                            row.splitToggled(card.modelData.id)
                    }
                    onDoubleClicked: mouse => {
                        if (mouse.button === Qt.LeftButton && !row.splitting)
                            row.openItem(card.modelData.id)
                    }
                    ToolTip.visible: containsMouse
                    ToolTip.delay: 800
                    ToolTip.text: card.modelData.folder + "\\" + card.modelData.name
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 6
                    spacing: 4
                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Image {
                            anchors.fill: parent
                            fillMode: Image.PreserveAspectFit
                            asynchronous: true
                            source: card.freshThumb || card.modelData.thumb
                            sourceSize: Qt.size(320, 320)
                            opacity: card.modelData.available ? 1 : 0.4
                        }
                        Label {
                            visible: card.modelData.video
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            text: Theme.iconPlay
                            font.family: Theme.iconFont
                            color: "#FFFFFF"
                            style: Text.Outline
                            styleColor: "#000000"
                        }
                        PzCheckBox {
                            visible: row.splitting
                            anchors.top: parent.top
                            anchors.left: parent.left
                            checked: card.marked
                            focusPolicy: Qt.NoFocus
                            Accessible.name: "Détacher " + card.modelData.name
                            onToggled: {
                                row.splitToggled(card.modelData.id)
                                checked = Qt.binding(() => card.marked)
                            }
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: card.modelData.available ? card.modelData.name : "Indisponible"
                        color: Theme.text2
                        font.pixelSize: 12
                        elide: Text.ElideMiddle
                        horizontalAlignment: Text.AlignHCenter
                    }
                }

                Menu {
                    id: itemMenu
                    width: 280
                    PzMenuItem { text: "Retirer de ce groupe"; onTriggered: naming.removeItem(row.groupId, card.modelData.id) }
                    MenuSeparator {}
                    PzMenuItem { text: "Ouvrir"; onTriggered: row.openItem(card.modelData.id) }
                    PzMenuItem { text: "Afficher dans l'Explorateur"; onTriggered: app.revealInExplorer(card.modelData.id) }
                }
            }
        }

        // Scission : choisir les éléments à détacher, puis nommer le nouveau groupe.
        RowLayout {
            visible: row.splitting
            Layout.fillWidth: true
            spacing: 12
            readonly property bool possible: row.splitIds.length >= 2 && row.memberCount - row.splitIds.length >= 2
            Label {
                text: row.splitIds.length + " à détacher"
                      + (parent.possible ? "" : " — deux éléments au moins de chaque côté")
                color: parent.possible ? Theme.text : Theme.amberText
            }
            Item { Layout.fillWidth: true }
            PzTextField {
                id: splitName
                Layout.preferredWidth: 260
                placeholderText: "Nom du nouveau groupe"
                onAccepted: if (parent.possible && text.trim() !== "") row.splitConfirmed(text)
                Keys.onEscapePressed: row.splitCancelled()
            }
            RowBtn {
                text: "Scinder"
                enabled: parent.possible && splitName.text.trim() !== ""
                onClicked: row.splitConfirmed(splitName.text)
            }
            RowBtn { text: "Annuler"; onClicked: row.splitCancelled() }
        }
    }

    Menu {
        id: groupMenu
        width: 260
        PzMenuItem { text: "Renommer"; onTriggered: row.renameRequested() }
        PzMenuItem { text: "Confirmer ce groupe"; shown: !row.confirmed; onTriggered: naming.confirmGroup(row.groupId) }
        PzMenuItem { text: "Fusionner avec…"; enabled: naming.groupCount > 1; onTriggered: row.mergeRequested() }
        PzMenuItem { text: "Scinder…"; enabled: row.memberCount >= 4; tip: "4 éléments au moins"; onTriggered: row.splitRequested() }
        MenuSeparator {}
        PzMenuItem { text: "Ce n'est pas un groupe"; onTriggered: naming.notAGroup(row.groupId) }
    }
}

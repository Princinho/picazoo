import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Proposition d'association (FR-030, FR-031) : proposée, jamais imposée ; c'est
// Windows qui enregistre le choix de l'utilisateur.
Rectangle {
    id: card
    width: Math.min(640, parent ? parent.width - 48 : 640)
    implicitHeight: col.implicitHeight + 48
    radius: 16
    color: Theme.panel
    border.color: Theme.lineStrong

    property var families: session.formatFamilies()
    property var chosen: families.map(f => f.id)
    property bool done: false

    function toggle(id, on) {
        const c = chosen.filter(x => x !== id)
        if (on)
            c.push(id)
        chosen = c
    }


    ColumnLayout {
        id: col
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 24
        spacing: 14

        Label {
            text: card.done ? "Dernière étape, dans Windows" : "Ouvrir vos photos avec Picazoo ?"
            font.family: Theme.displayFont
            font.pixelSize: 18
            font.weight: Font.DemiBold
            color: Theme.text
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.textSoft
            text: card.done
                  ? "Dans la page de Windows qui vient de s'ouvrir, choisissez Picazoo pour : "
                    + session.extensionsFor(card.chosen).join(", ") + "."
                  : "Quand vous double-cliquez sur une photo dans l'Explorateur, elle s'ouvrira ici. "
                    + "Vous confirmerez dans les réglages de Windows ; rien ne change sans votre accord."
        }
        Flow {
            Layout.fillWidth: true
            visible: !card.done
            spacing: 8
            Repeater {
                model: card.families
                delegate: CheckBox {
                    required property var modelData
                    text: modelData.label
                    checked: card.chosen.indexOf(modelData.id) >= 0
                    onToggled: card.toggle(modelData.id, checked)
                    ToolTip.visible: hovered
                    ToolTip.delay: 500
                    ToolTip.text: modelData.extensions
                }
            }
        }
        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: 8
            PillButton {
                visible: !card.done
                text: "Ne plus demander"
                onClicked: session.answerProposal("never")
            }
            PillButton {
                visible: !card.done
                text: "Plus tard"
                onClicked: session.answerProposal("later")
            }
            PillButton {
                visible: !card.done
                primary: true
                enabled: card.chosen.length > 0
                text: "Choisir dans Windows…"
                onClicked: {
                    card.done = true
                    session.answerProposal("accepted", card.chosen)
                    session.showProposal()
                }
            }
            PillButton {
                visible: card.done
                primary: true
                text: "Compris"
                onClicked: {
                    card.done = false
                    session.hideProposal()
                }
            }
        }
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Entrée de menu contextuel (007) : libellé à gauche, raccourci à droite (FR-067). Le
// raccourci n'est qu'affiché : les touches sont traitées par l'écran (pas de raccourci global).
// `shown: false` retire l'entrée sans laisser de trou.
MenuItem {
    id: item
    property string hint: ""
    property bool shown: true
    // Raison d'une entrée grisée, affichée à la place du raccourci (une entrée désactivée ne
    // reçoit pas le survol : une info-bulle ne s'afficherait jamais).
    property string tip: ""
    visible: shown
    height: shown ? implicitHeight : 0
    contentItem: RowLayout {
        spacing: 24
        Label {
            Layout.fillWidth: true
            text: item.text
            color: item.enabled ? Theme.text : Theme.text3
            elide: Text.ElideRight
        }
        Label {
            readonly property string shownHint: !item.enabled && item.tip !== "" ? item.tip : item.hint
            visible: shownHint !== ""
            text: shownHint
            color: Theme.text3
            font.pixelSize: 12
        }
    }
}

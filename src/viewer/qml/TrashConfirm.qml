import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Confirmation de « Mettre à la corbeille… » (007, FR-071) : « Annuler » est le choix par
// défaut (focus, Entrée, Échap) ; rien ne part sans un clic sur « Mettre à la corbeille ».
Dialog {
    id: dlg
    property var plan: ({})
    signal confirmed()
    signal cancelled()

    function ask(p) {
        plan = p
        open()
    }

    modal: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(520, (parent ? parent.width : 520) - 48)
    padding: 24
    closePolicy: Popup.CloseOnEscape
    onRejected: cancelled()
    onOpened: cancelBtn.forceActiveFocus()

    background: Rectangle { radius: 14; color: Theme.surface; border.color: Theme.lineStrong }

    contentItem: ColumnLayout {
        spacing: 14
        Keys.onReturnPressed: dlg.reject()
        Keys.onEnterPressed: dlg.reject()
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.pixelSize: 18
            font.weight: Font.DemiBold
            color: Theme.text
            text: dlg.plan.count === 1 ? "Mettre « " + dlg.plan.name + " » à la corbeille ?"
                                        : "Mettre " + dlg.plan.count + " fichiers à la corbeille ?"
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.text2
            text: (dlg.plan.count === 1 ? "Le fichier part" : "Les fichiers (" + dlg.plan.size + ") partent")
                  + " dans la corbeille de Windows. Vous pourrez l'annuler."
        }
        Label {
            Layout.fillWidth: true
            visible: (dlg.plan.noRecycleBin || 0) + (dlg.plan.missing || 0) > 0
            wrapMode: Text.WordWrap
            color: Theme.text3
            font.pixelSize: 12
            text: ((dlg.plan.noRecycleBin || 0) > 0 ? dlg.plan.noRecycleBin + " fichier(s) sur un disque sans corbeille ne seront pas touchés. " : "")
                  + ((dlg.plan.missing || 0) > 0 ? dlg.plan.missing + " fichier(s) introuvable(s) ignoré(s)." : "")
        }
        RowLayout {
            Layout.alignment: Qt.AlignRight
            Layout.topMargin: 8
            spacing: 10
            Button {
                id: cancelBtn
                text: "Annuler"
                focus: true
                onClicked: dlg.reject()
                Keys.onReturnPressed: dlg.reject()
                Keys.onEnterPressed: dlg.reject()
                contentItem: Label { text: cancelBtn.text; color: Theme.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle {
                    implicitWidth: 110; implicitHeight: 40; radius: 10
                    color: cancelBtn.hovered ? Theme.hover : "#1A1D23"
                    border.color: cancelBtn.activeFocus ? Theme.accent : Theme.lineStrong
                    border.width: cancelBtn.activeFocus ? 2 : 1
                }
            }
            Button {
                id: okBtn
                text: "Mettre à la corbeille"
                onClicked: { dlg.close(); dlg.confirmed() }
                Keys.onReturnPressed: { dlg.close(); dlg.confirmed() }
                contentItem: Label { text: okBtn.text; color: "#FFFFFF"; font.weight: Font.DemiBold; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle {
                    implicitWidth: 180; implicitHeight: 40; radius: 10
                    color: okBtn.hovered ? "#C2453B" : "#A8382F"
                    border.color: okBtn.activeFocus ? Theme.accent : "transparent"
                    border.width: 2
                }
            }
        }
    }
}

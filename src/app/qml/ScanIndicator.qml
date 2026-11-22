import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Indicateur d'analyse discret (US1-2) : invisible au repos.
Rectangle {
    implicitHeight: app.scanActive ? 28 : 0
    visible: app.scanActive
    color: "#252525"

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 10

        BusyIndicator {
            running: app.scanActive && !app.scanPaused
            Layout.preferredWidth: 18
            Layout.preferredHeight: 18
        }
        Label {
            text: app.scanPaused ? app.scanText + " (en pause)" : app.scanText
            color: "#b0b0b0"
            font.pixelSize: 12
        }
        Item { Layout.fillWidth: true }
        ToolButton {
            text: app.scanPaused ? "Reprendre" : "Pause"
            font.pixelSize: 12
            onClicked: app.togglePause()
        }
    }
}

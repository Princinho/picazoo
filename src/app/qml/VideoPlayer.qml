import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia

// Lecteur vidéo intégré (US3, T063) : lecture/pause, barre de progression, volume.
Item {
    id: root
    property url source
    property var itemId: 0

    function togglePlay() {
        if (player.playbackState === MediaPlayer.PlayingState)
            player.pause()
        else
            player.play()
    }

    onSourceChanged: player.stop()

    MediaPlayer {
        id: player
        source: root.source
        audioOutput: AudioOutput {
            id: audio
            volume: volumeSlider.value
        }
        videoOutput: output
    }

    VideoOutput {
        id: output
        anchors.fill: parent
        anchors.bottomMargin: controls.height
        fillMode: VideoOutput.PreserveAspectFit
        TapHandler { onTapped: root.togglePlay() }
    }

    ColumnLayout {
        anchors.centerIn: output
        visible: player.error !== MediaPlayer.NoError
        spacing: 12
        Label {
            text: "Picazoo ne sait pas afficher ce fichier."
            font.pixelSize: 18
            Layout.alignment: Qt.AlignHCenter
        }
        Button {
            text: "Ouvrir avec l'application par défaut"
            Layout.alignment: Qt.AlignHCenter
            onClicked: app.openWithDefaultApp(root.itemId)
        }
    }

    Rectangle {
        id: controls
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 44
        color: "#181818"

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 10
            ToolButton {
                text: player.playbackState === MediaPlayer.PlayingState ? "❚❚" : "▶"
                onClicked: root.togglePlay()
                ToolTip.visible: hovered
                ToolTip.text: "Lecture / pause (Espace)"
            }
            Label {
                text: app.formatDuration(player.position) + " / " + app.formatDuration(player.duration)
                color: "#c0c0c0"
            }
            Slider {
                id: progress
                Layout.fillWidth: true
                from: 0
                to: Math.max(1, player.duration)
                value: player.position
                enabled: player.seekable
                onMoved: player.position = value
            }
            Label { text: "Volume"; color: "#a0a0a0" }
            Slider {
                id: volumeSlider
                implicitWidth: 110
                from: 0
                to: 1
                value: 0.8
            }
        }
    }
}

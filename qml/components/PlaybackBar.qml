import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.casts 1.0

Item {
    id: root
    visible: Player.active
    width: parent ? parent.width : 0
    height: visible ? Theme.itemSizeMedium : 0

    signal openPlayer()

    Rectangle {
        anchors.fill: parent
        color: Theme.rgba(Theme.highlightColor, 0.12)
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.openPlayer()
    }

    Icon {
        id: playPauseIcon
        anchors {
            left: parent.left
            leftMargin: Theme.horizontalPageMargin
            verticalCenter: parent.verticalCenter
        }
        source: Player.playing ? "image://theme/icon-m-pause" : "image://theme/icon-m-play"
    }

    MouseArea {
        anchors.fill: playPauseIcon
        onClicked: Player.togglePlay()
    }

    Column {
        anchors {
            left: playPauseIcon.right
            leftMargin: Theme.paddingMedium
            right: skipIcon.left
            rightMargin: Theme.paddingMedium
            verticalCenter: parent.verticalCenter
        }
        Label {
            text: Player.title
            width: parent.width
            truncationMode: TruncationMode.Fade
            color: Theme.primaryColor
            font.pixelSize: Theme.fontSizeSmall
        }
        Label {
            visible: Player.duration > 0
            text: Player.formatTime(Player.position) + " / " + Player.formatTime(Player.duration)
            color: Theme.secondaryColor
            font.pixelSize: Theme.fontSizeExtraSmall
        }
    }

    Icon {
        id: skipIcon
        anchors {
            right: parent.right
            rightMargin: Theme.horizontalPageMargin
            verticalCenter: parent.verticalCenter
        }
        source: "image://theme/icon-m-forward"
    }

    MouseArea {
        anchors.fill: skipIcon
        onClicked: Player.skipForward()
    }
}

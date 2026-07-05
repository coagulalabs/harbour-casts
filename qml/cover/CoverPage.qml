import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.casts 1.0

CoverBackground {
    Column {
        anchors.centerIn: parent
        spacing: Theme.paddingMedium
        width: parent.width - 2 * Theme.paddingLarge

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: Player.active ? Player.title : qsTr("Casts")
            font.pixelSize: Theme.fontSizeMedium
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            width: parent.width
            color: Theme.highlightColor
        }

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: Player.active
            text: Player.feedTitle
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.secondaryColor
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            width: parent.width
        }

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: Player.active && Player.duration > 0
            text: Player.formatTime(Player.position) + " / " + Player.formatTime(Player.duration)
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.primaryColor
        }
    }
}

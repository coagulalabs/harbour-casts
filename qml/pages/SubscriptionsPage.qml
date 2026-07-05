import QtQuick 2.0
import Sailfish.Silica 1.0
import Nemo.Notifications 1.0
import harbour.casts 1.0
import "../components"

Page {
    id: page
    allowedOrientations: Orientation.All

    Connections {
        target: PodcastStore
        onError: function(message) {
            notification.text = message;
            notification.publish();
        }
    }

    Notification {
        id: notification
        appIcon: "image://theme/icon-m-browser-alert"
        previewBody: ""
    }

    PullDownMenu {
        MenuItem {
            text: qsTr("Add podcast")
            onClicked: pageStack.push(Qt.resolvedUrl("AddPodcastPage.qml"))
        }
        MenuItem {
            text: qsTr("Refresh all")
            enabled: !PodcastStore.busy
            onClicked: PodcastStore.refreshAll()
        }
        MenuItem {
            visible: PodcastStore.queueCount > 0
            text: qsTr("Queue (%1)").arg(PodcastStore.queueCount)
            onClicked: pageStack.push(Qt.resolvedUrl("QueuePage.qml"))
        }
        MenuItem {
            visible: Player.active
            text: qsTr("Now playing")
            onClicked: pageStack.push(Qt.resolvedUrl("PlayerPage.qml"))
        }
    }

    SilicaListView {
        id: list
        anchors.fill: parent
        anchors.bottomMargin: playbackBar.visible ? playbackBar.height : 0
        model: PodcastStore.subscriptionsModel

        header: Column {
            width: list.width

            PageHeader {
                title: qsTr("Casts")
                description: PodcastStore.statusMessage.length > 0
                    ? PodcastStore.statusMessage
                    : qsTr("%1 subscriptions").arg(list.count)
            }

            BackgroundItem {
                width: parent.width
                height: Theme.itemSizeMedium
                onClicked: pageStack.push(Qt.resolvedUrl("AddPodcastPage.qml"))

                Label {
                    anchors.centerIn: parent
                    text: qsTr("Add podcast")
                    color: Theme.highlightColor
                    font.pixelSize: Theme.fontSizeMedium
                }
            }
        }

        delegate: BackgroundItem {
            width: list.width
            height: Theme.itemSizeLarge
            onClicked: {
                PodcastStore.openFeed(feedId);
                pageStack.push(Qt.resolvedUrl("EpisodesPage.qml"));
            }
            Item {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                height: parent.height

                Image {
                    id: art
                    width: Theme.itemSizeMedium
                    height: Theme.itemSizeMedium
                    anchors.verticalCenter: parent.verticalCenter
                    source: imageUrl.length > 0 ? imageUrl : "image://theme/icon-m-audio"
                    fillMode: Image.PreserveAspectCrop
                    smooth: true
                }

                Column {
                    anchors {
                        left: art.right
                        leftMargin: Theme.paddingMedium
                        right: parent.right
                        verticalCenter: parent.verticalCenter
                    }
                    Label {
                        text: title
                        font.pixelSize: Theme.fontSizeMedium
                        color: Theme.primaryColor
                        width: parent.width
                        truncationMode: TruncationMode.Fade
                    }
                    Label {
                        text: unplayedCount > 0
                            ? qsTr("%1 unplayed · %2 episodes").arg(unplayedCount).arg(episodeCount)
                            : qsTr("%1 episodes").arg(episodeCount)
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.secondaryColor
                    }
                }
            }
        }

        VerticalScrollDecorator {}
    }

    PlaybackBar {
        id: playbackBar
        anchors.bottom: parent.bottom
        onOpenPlayer: pageStack.push(Qt.resolvedUrl("PlayerPage.qml"))
    }
}

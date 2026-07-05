import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.casts 1.0

Page {
    id: page
    allowedOrientations: Orientation.All

    property var items: PodcastStore.queueItems()

    function reload() {
        items = PodcastStore.queueItems();
    }

    onStatusChanged: if (status === PageStatus.Active) reload()

    SilicaListView {
        id: list
        anchors.fill: parent
        model: items

        header: PageHeader {
            title: qsTr("Queue")
            description: qsTr("%1 episodes").arg(list.count)
        }

        delegate: ListItem {
            width: list.width
            contentHeight: Theme.itemSizeMedium
            onClicked: {
                Player.playEpisode(modelData.episodeId);
                pageStack.push(Qt.resolvedUrl("PlayerPage.qml"));
            }

            Label {
                anchors.verticalCenter: parent.verticalCenter
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: modelData.title
                color: Theme.primaryColor
                font.pixelSize: Theme.fontSizeMedium
                truncationMode: TruncationMode.Fade
            }

            Label {
                anchors {
                    bottom: parent.bottom
                    bottomMargin: Theme.paddingSmall
                    left: parent.left
                    leftMargin: Theme.horizontalPageMargin
                }
                text: modelData.feedTitle
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
            }
        }

        PullDownMenu {
            MenuItem {
                text: qsTr("Clear queue")
                enabled: list.count > 0
                onClicked: {
                    PodcastStore.clearQueue();
                    reload();
                }
            }
        }

        VerticalScrollDecorator {}
    }
}

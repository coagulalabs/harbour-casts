import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.casts 1.0

Page {
    id: page
    allowedOrientations: Orientation.All

    onStatusChanged: {
        if (status === PageStatus.Active)
            PodcastStore.loadEpisodes(PodcastStore.openFeedId)
    }

    PullDownMenu {
        MenuItem {
            text: qsTr("Refresh feed")
            enabled: !PodcastStore.busy
            onClicked: PodcastStore.refreshFeed(PodcastStore.openFeedId)
        }
        MenuItem {
            text: qsTr("Remove subscription")
            onClicked: {
                PodcastStore.removeFeed(PodcastStore.openFeedId);
                pageStack.pop();
            }
        }
    }

    BusyIndicator {
        anchors.centerIn: parent
        running: PodcastStore.episodesLoading
    }

    SilicaListView {
        id: list
        anchors.fill: parent
        visible: !PodcastStore.episodesLoading
        model: PodcastStore.episodesModel

        header: PageHeader {
            title: PodcastStore.openFeedTitle.length > 0
                ? PodcastStore.openFeedTitle
                : qsTr("Episodes")
            description: PodcastStore.episodesLoading
                ? qsTr("Loading…")
                : qsTr("%1 of %2 episodes").arg(list.count).arg(PodcastStore.episodesTotalCount)
        }

        delegate: ListItem {
            width: list.width
            contentHeight: Theme.itemSizeMedium

            onClicked: {
                Player.playEpisode(episodeId);
                pageStack.push(Qt.resolvedUrl("PlayerPage.qml"));
            }

            Label {
                anchors.verticalCenter: parent.verticalCenter
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin - metaLbl.width - Theme.paddingMedium
                text: title
                color: completed ? Theme.secondaryColor : Theme.primaryColor
                font.pixelSize: Theme.fontSizeMedium
                truncationMode: TruncationMode.Fade
            }

            Label {
                id: metaLbl
                anchors {
                    verticalCenter: parent.verticalCenter
                    right: parent.right
                    rightMargin: Theme.horizontalPageMargin
                }
                text: PodcastStore.formatDuration(durationSec)
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
            }

            Icon {
                anchors {
                    right: metaLbl.left
                    rightMargin: Theme.paddingSmall
                    verticalCenter: parent.verticalCenter
                }
                visible: downloaded
                source: "image://theme/icon-m-download"
                opacity: 0.7
            }

            Icon {
                anchors {
                    right: metaLbl.left
                    rightMargin: Theme.paddingSmall
                    verticalCenter: parent.verticalCenter
                }
                visible: inQueue && !downloaded
                source: "image://theme/icon-m-playlist"
                opacity: 0.7
            }

            ContextMenu {
                MenuItem {
                    text: qsTr("Show notes")
                    onClicked: {
                        PodcastStore.openEpisodeNotes(episodeId);
                        pageStack.push(Qt.resolvedUrl("EpisodeNotesPage.qml"));
                    }
                }
                MenuItem {
                    text: PodcastStore.isDownloading(episodeId)
                        ? qsTr("Downloading…")
                        : (downloaded ? qsTr("Downloaded") : qsTr("Download"))
                    enabled: !downloaded && !PodcastStore.isDownloading(episodeId)
                    onClicked: PodcastStore.downloadEpisode(episodeId)
                }
                MenuItem {
                    text: inQueue ? qsTr("Remove from queue") : qsTr("Add to queue")
                    onClicked: {
                        if (inQueue)
                            PodcastStore.removeFromQueue(episodeId);
                        else
                            PodcastStore.addToQueue(episodeId);
                    }
                }
                MenuItem {
                    text: completed ? qsTr("Mark unplayed") : qsTr("Mark played")
                    onClicked: PodcastStore.markCompleted(episodeId, !completed)
                }
            }
        }

        footer: BackgroundItem {
            width: list.width
            height: Theme.itemSizeMedium
            visible: PodcastStore.episodesHasMore && !PodcastStore.episodesLoading
            onClicked: PodcastStore.loadMoreEpisodes()

            Label {
                anchors.centerIn: parent
                text: qsTr("Load more")
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeMedium
            }
        }

        VerticalScrollDecorator {}
    }
}

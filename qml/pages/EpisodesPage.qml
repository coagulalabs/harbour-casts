import QtQuick 2.0
import Sailfish.Silica 1.0
import Nemo.Notifications 1.0
import harbour.casts 1.0

Page {
    id: page
    allowedOrientations: Orientation.All

    onStatusChanged: {
        if (status === PageStatus.Active)
            PodcastStore.loadEpisodes(PodcastStore.openFeedId)
    }

    Connections {
        target: PodcastStore
        onError: function(message) {
            notification.summary = qsTr("Casts")
            notification.body = message
            notification.previewSummary = qsTr("Download failed")
            notification.previewBody = message
            notification.icon = "image://theme/icon-lock-warning"
            notification.publish()
        }
        onDownloadFinished: function(episodeId, success) {
            if (!success)
                return
            notification.summary = qsTr("Casts")
            notification.body = qsTr("Episode downloaded")
            notification.previewSummary = qsTr("Downloaded")
            notification.previewBody = PodcastStore.statusMessage
            notification.icon = "image://theme/icon-lock-installed"
            notification.publish()
        }
    }

    Notification {
        id: notification
        appName: qsTr("Casts")
        appIcon: "image://theme/icon-m-cloud-download"
        previewBody: ""
    }

    BusyIndicator {
        anchors.centerIn: parent
        running: PodcastStore.episodesLoading && list.count === 0
        size: BusyIndicatorSize.Large
        z: 1
    }

    RemorsePopup { id: remorse }

    SilicaListView {
        id: list
        anchors.fill: parent
        model: PodcastStore.episodesModel

        PullDownMenu {
            busy: PodcastStore.busy || PodcastStore.episodesLoading || PodcastStore.downloading
            MenuItem {
                text: qsTr("Refresh feed")
                enabled: !PodcastStore.busy
                onClicked: PodcastStore.refreshFeed(PodcastStore.openFeedId)
            }
            MenuItem {
                visible: PodcastStore.downloading
                text: qsTr("Cancel download")
                onClicked: PodcastStore.cancelDownload()
            }
            MenuItem {
                text: qsTr("Remove subscription")
                onClicked: remorse.execute(qsTr("Removing subscription"), function() {
                    PodcastStore.removeFeed(PodcastStore.openFeedId);
                    pageStack.pop();
                })
            }
        }

        header: PageHeader {
            title: PodcastStore.openFeedTitle.length > 0
                ? PodcastStore.openFeedTitle
                : qsTr("Episodes")
            description: {
                if (PodcastStore.downloading) {
                    if (PodcastStore.downloadPercent >= 0)
                        return qsTr("Downloading… %1%").arg(PodcastStore.downloadPercent)
                    return qsTr("Downloading…")
                }
                if (PodcastStore.episodesLoading)
                    return qsTr("Loading…")
                return qsTr("%1 of %2 episodes").arg(list.count).arg(PodcastStore.episodesTotalCount)
            }
        }

        delegate: ListItem {
            id: episodeItem
            width: list.width
            contentHeight: Theme.itemSizeMedium

            property bool rowDownloading: PodcastStore.downloading
                                          && PodcastStore.downloadingEpisodeId === episodeId

            menu: ContextMenu {
                MenuItem {
                    text: qsTr("Show notes")
                    onClicked: {
                        PodcastStore.openEpisodeNotes(episodeId);
                        pageStack.push(Qt.resolvedUrl("EpisodeNotesPage.qml"));
                    }
                }
                MenuItem {
                    text: {
                        if (episodeItem.rowDownloading)
                            return qsTr("Downloading… %1%").arg(Math.max(0, PodcastStore.downloadPercent))
                        if (downloaded)
                            return qsTr("Downloaded")
                        return qsTr("Download")
                    }
                    enabled: !downloaded && !PodcastStore.downloading
                    onClicked: PodcastStore.downloadEpisode(episodeId)
                }
                MenuItem {
                    visible: episodeItem.rowDownloading
                    text: qsTr("Cancel download")
                    onClicked: PodcastStore.cancelDownload()
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
                text: episodeItem.rowDownloading
                      ? (PodcastStore.downloadPercent >= 0
                         ? qsTr("%1%").arg(PodcastStore.downloadPercent)
                         : qsTr("…"))
                      : PodcastStore.formatDuration(durationSec)
                color: episodeItem.rowDownloading ? Theme.highlightColor : Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
            }

            Icon {
                anchors {
                    right: metaLbl.left
                    rightMargin: Theme.paddingSmall
                    verticalCenter: parent.verticalCenter
                }
                visible: downloaded && !episodeItem.rowDownloading
                source: "image://theme/icon-m-download"
                opacity: 0.7
            }

            Icon {
                anchors {
                    right: metaLbl.left
                    rightMargin: Theme.paddingSmall
                    verticalCenter: parent.verticalCenter
                }
                visible: episodeItem.rowDownloading
                source: "image://theme/icon-m-cloud-download"
                opacity: 0.9
            }

            Icon {
                anchors {
                    right: metaLbl.left
                    rightMargin: Theme.paddingSmall
                    verticalCenter: parent.verticalCenter
                }
                visible: inQueue && !downloaded && !episodeItem.rowDownloading
                source: "image://theme/icon-m-playlist"
                opacity: 0.7
            }

            ProgressBar {
                anchors {
                    left: parent.left
                    right: parent.right
                    bottom: parent.bottom
                }
                visible: episodeItem.rowDownloading && PodcastStore.downloadPercent >= 0
                minimumValue: 0
                maximumValue: 100
                value: Math.max(0, PodcastStore.downloadPercent)
                leftMargin: Theme.horizontalPageMargin
                rightMargin: Theme.horizontalPageMargin
                height: Theme.paddingSmall
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

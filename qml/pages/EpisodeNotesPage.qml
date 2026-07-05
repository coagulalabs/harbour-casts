import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.casts 1.0

Page {
    id: page
    allowedOrientations: Orientation.All

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: page.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: PodcastStore.openEpisodeTitle.length > 0
                    ? PodcastStore.openEpisodeTitle
                    : qsTr("Show notes")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                color: Theme.primaryColor
                font.pixelSize: Theme.fontSizeMedium
                text: PodcastStore.episodeDescription(PodcastStore.openEpisodeId)
            }

            Item { width: 1; height: Theme.paddingLarge }
        }
        VerticalScrollDecorator {}
    }
}

import QtQuick 2.0
import Sailfish.Silica 1.0
import Sailfish.Pickers 1.0
import harbour.casts 1.0

Page {
    id: page
    allowedOrientations: Orientation.All

    property bool waitingForFeed: false

    Connections {
        target: PodcastStore
        onFeedAdded: {
            if (waitingForFeed)
                pageStack.pop();
        }
        onError: waitingForFeed = false
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: page.width
            spacing: Theme.paddingLarge

            PageHeader {
                title: qsTr("Add podcast")
                description: PodcastStore.busy
                    ? qsTr("Fetching feed…")
                    : qsTr("Subscribe via RSS feed URL")
            }

            TextField {
                id: urlField
                width: parent.width - 2 * Theme.horizontalPageMargin
                x: Theme.horizontalPageMargin
                placeholderText: qsTr("https://example.com/feed.xml")
                focus: !PodcastStore.busy
            }

            Label {
                visible: PodcastStore.lastError.length > 0
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.errorColor
                font.pixelSize: Theme.fontSizeSmall
                text: PodcastStore.lastError
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: PodcastStore.busy ? qsTr("Subscribing…") : qsTr("Subscribe")
                enabled: urlField.text.trim().length > 0 && !PodcastStore.busy
                onClicked: {
                    waitingForFeed = true;
                    PodcastStore.addFeed(urlField.text.trim());
                }
            }

            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                running: PodcastStore.busy
            }

            SectionHeader { text: qsTr("Import") }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Import OPML file")
                enabled: !PodcastStore.busy
                onClicked: opmlPicker.open()
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                text: qsTr("Export your subscriptions from another podcast app as OPML and import them here.")
            }
        }
        VerticalScrollDecorator {}
    }

    MultiDocumentPickerDialog {
        id: opmlPicker
        onDone: {
            if (result !== DialogResult.Accepted || selectedContent.count === 0)
                return;
            var url = selectedContent.get(0).url.toString();
            if (url.indexOf("file://") === 0)
                url = url.substring(7);
            waitingForFeed = false;
            PodcastStore.importOpmlFile(url);
            pageStack.pop();
        }
    }
}

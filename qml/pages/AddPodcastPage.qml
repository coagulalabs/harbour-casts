import QtQuick 2.0
import Sailfish.Silica 1.0
import Sailfish.Pickers 1.0
import harbour.casts 1.0

Page {
    id: page
    allowedOrientations: Orientation.All

    property bool waitingForFeed: false

    function pathFromPicker(props) {
        if (!props)
            return ""
        if (props.filePath && props.filePath.length > 0)
            return props.filePath
        var url = props.url ? props.url.toString() : ""
        if (url.indexOf("file://") === 0)
            return decodeURIComponent(url.substring(7))
        return url
    }

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
                inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase | Qt.ImhUrlCharactersOnly
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
                // FilePicker browses the filesystem (incl. Downloads).
                // DocumentPicker only lists Tracker "documents", so .opml never appears
                // and Accept stays disabled — which matches the XA2 report.
                onClicked: pageStack.animatorPush(opmlPickerPage)
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                text: qsTr("Place an OPML export (e.g. from gPodder) in Downloads, then choose the .opml or .xml file here.")
            }
        }
        VerticalScrollDecorator {}
    }

    Component {
        id: opmlPickerPage
        FilePickerPage {
            title: qsTr("Select OPML file")
            nameFilters: [ "*.opml", "*.xml" ]
            onSelectedContentPropertiesChanged: {
                var path = page.pathFromPicker(selectedContentProperties)
                if (!path || path.length === 0)
                    return
                waitingForFeed = false
                PodcastStore.importOpmlFile(path)
                // Pop FilePicker + Add page back to subscriptions.
                var dest = pageStack.previousPage(page)
                if (dest)
                    pageStack.pop(dest)
                else
                    pageStack.pop()
            }
        }
    }
}

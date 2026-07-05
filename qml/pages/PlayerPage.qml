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
            spacing: Theme.paddingLarge

            PageHeader {
                title: Player.title.length > 0 ? Player.title : qsTr("Now playing")
                description: Player.feedTitle
            }

            Slider {
                id: seekSlider
                width: parent.width
                enabled: Player.duration > 0
                minimumValue: 0
                maximumValue: Player.duration > 0 ? Player.duration : 1
                value: seekSlider.pressed ? seekSlider.value : Player.position
                onPressedChanged: {
                    if (!seekSlider.pressed)
                        Player.seek(seekSlider.value)
                }
            }

            Label {
                x: Theme.horizontalPageMargin
                text: Player.formatTime(Player.position) + " / " + Player.formatTime(Player.duration)
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
            }

            Row {
                x: Theme.horizontalPageMargin
                spacing: Theme.paddingLarge

                Item {
                    width: Theme.itemSizeMedium
                    height: width
                    Icon {
                        id: backIcon
                        anchors.centerIn: parent
                        source: "image://theme/icon-m-backward"
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: Player.skipBackward()
                    }
                }

                Item {
                    width: Theme.itemSizeMedium
                    height: width
                    Icon {
                        id: playIcon
                        anchors.centerIn: parent
                        source: Player.playing ? "image://theme/icon-m-pause" : "image://theme/icon-m-play"
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: Player.togglePlay()
                    }
                }

                Item {
                    width: Theme.itemSizeMedium
                    height: width
                    Icon {
                        id: fwdIcon
                        anchors.centerIn: parent
                        source: "image://theme/icon-m-forward"
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: Player.skipForward()
                    }
                }

                Item {
                    width: Theme.itemSizeMedium
                    height: width
                    Icon {
                        id: stopIcon
                        anchors.centerIn: parent
                        source: "image://theme/icon-m-stop"
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: Player.stop()
                    }
                }
            }

            SectionHeader { text: qsTr("Speed") }

            Repeater {
                model: [0.75, 1.0, 1.25, 1.5, 1.75, 2.0]
                delegate: BackgroundItem {
                    width: page.width
                    height: Theme.itemSizeSmall
                    onClicked: Player.setPlaybackRate(modelData)
                    Label {
                        x: Theme.horizontalPageMargin
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.toFixed(2) + "×"
                        color: Math.abs(Player.playbackRate - modelData) < 0.01
                            ? Theme.highlightColor : Theme.primaryColor
                        font.bold: Math.abs(Player.playbackRate - modelData) < 0.01
                    }
                }
            }

            SectionHeader {
                text: Player.sleepRemainingSec > 0
                    ? qsTr("Sleep timer · %1 remaining").arg(Player.formatTime(Player.sleepRemainingSec * 1000))
                    : qsTr("Sleep timer")
            }

            Repeater {
                model: [
                    { label: qsTr("Off"), min: 0 },
                    { label: qsTr("15 min"), min: 15 },
                    { label: qsTr("30 min"), min: 30 },
                    { label: qsTr("45 min"), min: 45 },
                    { label: qsTr("60 min"), min: 60 }
                ]
                delegate: BackgroundItem {
                    width: page.width
                    height: Theme.itemSizeSmall
                    onClicked: Player.setSleepMinutes(modelData.min)
                    Label {
                        x: Theme.horizontalPageMargin
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.label
                        color: Player.sleepMinutes === modelData.min
                            ? Theme.highlightColor : Theme.primaryColor
                    }
                }
            }

            SectionHeader { text: qsTr("Show notes") }

            BackgroundItem {
                width: page.width
                height: showNotesPreview.height + 2 * Theme.paddingMedium
                enabled: Player.episodeId > 0
                onClicked: {
                    PodcastStore.openEpisodeNotes(Player.episodeId);
                    pageStack.push(Qt.resolvedUrl("EpisodeNotesPage.qml"));
                }

                Label {
                    id: showNotesPreview
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    wrapMode: Text.WordWrap
                    maximumLineCount: 4
                    elide: Text.ElideRight
                    color: Theme.secondaryColor
                    font.pixelSize: Theme.fontSizeSmall
                    text: Player.episodeId > 0
                        ? PodcastStore.episodeDescription(Player.episodeId)
                        : qsTr("No episode selected")
                }
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Add to queue")
                enabled: Player.episodeId > 0
                onClicked: PodcastStore.addToQueue(Player.episodeId)
            }

            Item { width: 1; height: Theme.paddingLarge }
        }
        VerticalScrollDecorator {}
    }
}

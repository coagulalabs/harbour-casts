TARGET = harbour-casts

CONFIG += sailfishapp c++14

QT += core gui qml quick network sql multimedia concurrent

SOURCES += \
    src/artworkcache.cpp \
    src/episodesmodel.cpp \
    src/feedparser.cpp \
    src/harbour-casts.cpp \
    src/playercontroller.cpp \
    src/podcaststore.cpp \
    src/subscriptionsmodel.cpp

HEADERS += \
    src/artworkcache.h \
    src/episodesmodel.h \
    src/feedparser.h \
    src/playercontroller.h \
    src/podcaststore.h \
    src/subscriptionsmodel.h

DISTFILES += \
    qml/harbour-casts.qml \
    qml/cover/CoverPage.qml \
    qml/pages/SubscriptionsPage.qml \
    qml/pages/EpisodesPage.qml \
    qml/pages/EpisodeNotesPage.qml \
    qml/pages/PlayerPage.qml \
    qml/pages/AddPodcastPage.qml \
    qml/pages/QueuePage.qml \
    qml/components/PlaybackBar.qml \
    rpm/harbour-casts.changes.in \
    rpm/harbour-casts.changes.run.in \
    rpm/harbour-casts.profile \
    rpm/harbour-casts.spec \
    harbour-casts.desktop \
    icons/harbour-casts.svg

SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172

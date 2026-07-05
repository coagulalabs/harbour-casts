#ifdef QT_QML_DEBUG
#include <QtQuick>
#endif

#include <QQmlEngine>
#include <QtQml>
#include <sailfishapp.h>

#include "episodesmodel.h"
#include "playercontroller.h"
#include "podcaststore.h"
#include "subscriptionsmodel.h"

static PodcastStore *g_store = nullptr;
static PlayerController *g_player = nullptr;

static QObject *storeProvider(QQmlEngine *, QJSEngine *)
{
    if (!g_store) {
        g_store = new PodcastStore();
        g_store->init();
        if (g_player)
            g_player->setStore(g_store);
    }
    return g_store;
}

static QObject *playerProvider(QQmlEngine *, QJSEngine *)
{
    if (!g_player) {
        g_player = new PlayerController();
        if (g_store)
            g_player->setStore(g_store);
    }
    return g_player;
}

int main(int argc, char *argv[])
{
    qmlRegisterSingletonType<PodcastStore>("harbour.casts", 1, 0, "PodcastStore", storeProvider);
    qmlRegisterSingletonType<PlayerController>("harbour.casts", 1, 0, "Player", playerProvider);
    qmlRegisterUncreatableType<SubscriptionsModel>("harbour.casts", 1, 0, "SubscriptionsModel",
        QStringLiteral("Use PodcastStore.subscriptionsModel"));
    qmlRegisterUncreatableType<EpisodesModel>("harbour.casts", 1, 0, "EpisodesModel",
        QStringLiteral("Use PodcastStore.episodesModel"));

    return SailfishApp::main(argc, argv);
}

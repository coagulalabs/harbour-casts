#ifndef PODCASTSTORE_H
#define PODCASTSTORE_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QString>
#include <QVariantList>

class EpisodesModel;
class SubscriptionsModel;
class ArtworkCache;

class PodcastStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(int queueCount READ queueCount NOTIFY queueChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(SubscriptionsModel *subscriptionsModel READ subscriptionsModel CONSTANT)
    Q_PROPERTY(EpisodesModel *episodesModel READ episodesModel CONSTANT)
    Q_PROPERTY(int openFeedId READ openFeedId NOTIFY openFeedChanged)
    Q_PROPERTY(QString openFeedTitle READ openFeedTitle NOTIFY openFeedChanged)
    Q_PROPERTY(int openEpisodeId READ openEpisodeId NOTIFY openEpisodeChanged)
    Q_PROPERTY(QString openEpisodeTitle READ openEpisodeTitle NOTIFY openEpisodeChanged)
    Q_PROPERTY(bool episodesLoading READ episodesLoading NOTIFY episodesLoadingChanged)
    Q_PROPERTY(bool episodesHasMore READ episodesHasMore NOTIFY episodesMetaChanged)
    Q_PROPERTY(int episodesTotalCount READ episodesTotalCount NOTIFY episodesMetaChanged)

public:
    explicit PodcastStore(QObject *parent = nullptr);

    bool busy() const { return m_busy; }
    QString statusMessage() const { return m_statusMessage; }
    int queueCount() const { return m_queueCount; }
    QString lastError() const { return m_lastError; }

    SubscriptionsModel *subscriptionsModel() const { return m_subscriptions; }
    EpisodesModel *episodesModel() const { return m_episodes; }
    int openFeedId() const { return m_openFeedId; }
    QString openFeedTitle() const { return m_openFeedTitle; }
    int openEpisodeId() const { return m_openEpisodeId; }
    QString openEpisodeTitle() const { return m_openEpisodeTitle; }
    bool episodesLoading() const { return m_episodesLoading; }
    bool episodesHasMore() const;
    int episodesTotalCount() const;

    Q_INVOKABLE bool init();
    Q_INVOKABLE void addFeed(const QString &url);
    Q_INVOKABLE void refreshFeed(int feedId);
    Q_INVOKABLE void refreshAll();
    Q_INVOKABLE void removeFeed(int feedId);
    Q_INVOKABLE void importOpmlFile(const QString &path);
    Q_INVOKABLE void openFeed(int feedId);
    Q_INVOKABLE void openEpisodeNotes(int episodeId);
    Q_INVOKABLE void loadEpisodes(int feedId);
    Q_INVOKABLE void loadMoreEpisodes();
    Q_INVOKABLE int feedIdForEpisode(int episodeId) const;
    Q_INVOKABLE QString episodeTitle(int episodeId) const;
    Q_INVOKABLE QString episodeDescription(int episodeId) const;
    Q_INVOKABLE QString episodeAudioUrl(int episodeId) const;
    Q_INVOKABLE QString episodeLocalPath(int episodeId) const;
    Q_INVOKABLE int episodePositionMs(int episodeId) const;
    Q_INVOKABLE bool episodeCompleted(int episodeId) const;
    Q_INVOKABLE void markCompleted(int episodeId, bool completed);
    Q_INVOKABLE void savePosition(int episodeId, int positionMs);
    Q_INVOKABLE void downloadEpisode(int episodeId);
    Q_INVOKABLE void cancelDownload();
    Q_INVOKABLE bool isDownloading(int episodeId) const;
    Q_INVOKABLE void addToQueue(int episodeId);
    Q_INVOKABLE void removeFromQueue(int episodeId);
    Q_INVOKABLE void clearQueue();
    Q_INVOKABLE int nextQueuedEpisode(int afterEpisodeId) const;
    Q_INVOKABLE QVariantList queueItems() const;
    Q_INVOKABLE QString formatDuration(int seconds) const;
    Q_INVOKABLE QString formatRelativeDate(qint64 epoch) const;
    Q_INVOKABLE QString artworkForFeed(int feedId, const QString &remoteUrl) const;

signals:
    void busyChanged();
    void statusMessageChanged();
    void lastErrorChanged();
    void queueChanged();
    void feedAdded(int feedId);
    void feedUpdated(int feedId);
    void openFeedChanged();
    void openEpisodeChanged();
    void episodesLoadingChanged();
    void episodesMetaChanged();
    void episodesChanged();
    void downloadProgress(int episodeId, int percent);
    void downloadFinished(int episodeId, bool success);
    void artworkCached(int feedId);
    void error(const QString &message);

private slots:
    void onFeedReplyFinished();

private:
    void setBusy(bool busy);
    void setStatus(const QString &message);
    void setError(const QString &message);
    QString normalizeFeedUrl(const QString &url) const;
    void fetchFeed(const QString &url, int existingFeedId = -1);
    void ingestFeed(const struct ParsedFeed &feed, int existingFeedId);
    void updateQueueCount();
    QString downloadsDir() const;
    QString dbPath() const;

    ArtworkCache *m_artwork = nullptr;
    QNetworkAccessManager m_nam;
    QNetworkReply *m_feedReply = nullptr;
    QNetworkReply *m_downloadReply = nullptr;
    QString m_pendingFeedUrl;
    int m_pendingFeedId = -1;
    int m_downloadEpisodeId = 0;
    QString m_downloadTargetPath;

    SubscriptionsModel *m_subscriptions = nullptr;
    EpisodesModel *m_episodes = nullptr;
    bool m_busy = false;
    QString m_statusMessage;
    QString m_lastError;
    int m_queueCount = 0;
    int m_openFeedId = 0;
    QString m_openFeedTitle;
    int m_openEpisodeId = 0;
    QString m_openEpisodeTitle;
    bool m_episodesLoading = false;
};

#endif // PODCASTSTORE_H

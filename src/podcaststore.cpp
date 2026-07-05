#include "podcaststore.h"

#include "artworkcache.h"
#include "episodesmodel.h"
#include "feedparser.h"
#include "subscriptionsmodel.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUrl>
#include <QDebug>

PodcastStore::PodcastStore(QObject *parent)
    : QObject(parent)
    , m_subscriptions(new SubscriptionsModel(this))
    , m_episodes(new EpisodesModel(this))
    , m_artwork(new ArtworkCache(&m_nam, this))
{
    m_subscriptions->setArtworkCache(m_artwork);
    connect(m_artwork, &ArtworkCache::artworkCached, this, &PodcastStore::artworkCached);
    connect(m_episodes, &EpisodesModel::loadingChanged, this, [this]() {
        const bool loading = m_episodes->loading();
        if (m_episodesLoading == loading) return;
        m_episodesLoading = loading;
        emit episodesLoadingChanged();
    });
    connect(m_episodes, &EpisodesModel::metaChanged, this, &PodcastStore::episodesMetaChanged);
}

QString PodcastStore::dbPath() const
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(base);
    return base + QStringLiteral("/casts.db");
}

QString PodcastStore::downloadsDir() const
{
    QString dl = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (dl.isEmpty())
        dl = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString dir = dl + QStringLiteral("/Casts");
    QDir().mkpath(dir);
    return dir;
}

bool PodcastStore::init()
{
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
    db.setDatabaseName(dbPath());
    if (!db.open()) {
        emit error(tr("Cannot open database: %1").arg(db.lastError().text()));
        return false;
    }

    QSqlQuery q;
    q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS feeds ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " title TEXT NOT NULL,"
        " url TEXT NOT NULL UNIQUE,"
        " image_url TEXT,"
        " description TEXT,"
        " last_refresh INTEGER DEFAULT 0)"));

    q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS episodes ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " feed_id INTEGER NOT NULL,"
        " guid TEXT NOT NULL,"
        " title TEXT NOT NULL,"
        " audio_url TEXT NOT NULL,"
        " duration_sec INTEGER DEFAULT 0,"
        " pub_date INTEGER DEFAULT 0,"
        " description TEXT,"
        " local_path TEXT,"
        " position_ms INTEGER DEFAULT 0,"
        " completed INTEGER DEFAULT 0,"
        " UNIQUE(feed_id, guid),"
        " FOREIGN KEY(feed_id) REFERENCES feeds(id))"));

    q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS queue ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " episode_id INTEGER NOT NULL UNIQUE,"
        " sort_order INTEGER NOT NULL,"
        " FOREIGN KEY(episode_id) REFERENCES episodes(id))"));

    q.exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_episodes_feed_pub"
        " ON episodes(feed_id, pub_date DESC)"));

    m_episodes->setDatabasePath(dbPath());
    m_subscriptions->reload();
    updateQueueCount();
    return true;
}

void PodcastStore::setBusy(bool busy)
{
    if (m_busy == busy) return;
    m_busy = busy;
    emit busyChanged();
}

void PodcastStore::setStatus(const QString &message)
{
    if (m_statusMessage == message) return;
    m_statusMessage = message;
    emit statusMessageChanged();
}

void PodcastStore::setError(const QString &message)
{
    setStatus(QString());
    if (m_lastError == message) {
        if (!message.isEmpty()) emit error(message);
        return;
    }
    m_lastError = message;
    emit lastErrorChanged();
    if (!message.isEmpty()) emit error(message);
}

QString PodcastStore::normalizeFeedUrl(const QString &url) const
{
    QString trimmed = url.trimmed();
    if (trimmed.isEmpty()) return trimmed;
    if (!trimmed.contains(QStringLiteral("://")))
        trimmed = QStringLiteral("https://") + trimmed;
    return trimmed;
}

void PodcastStore::addFeed(const QString &url)
{
    const QString normalized = normalizeFeedUrl(url);
    if (normalized.isEmpty()) {
        setError(tr("Feed URL is empty"));
        return;
    }
    m_lastError.clear();
    emit lastErrorChanged();
    fetchFeed(normalized);
}

void PodcastStore::refreshFeed(int feedId)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT url FROM feeds WHERE id=?"));
    q.addBindValue(feedId);
    if (!q.exec() || !q.next()) return;
    fetchFeed(q.value(0).toString(), feedId);
}

void PodcastStore::refreshAll()
{
    QSqlQuery q(QStringLiteral("SELECT id FROM feeds ORDER BY title"));
    while (q.next())
        refreshFeed(q.value(0).toInt());
}

void PodcastStore::fetchFeed(const QString &url, int existingFeedId)
{
    if (m_feedReply) {
        m_feedReply->abort();
        m_feedReply->deleteLater();
        m_feedReply = nullptr;
    }

    m_pendingFeedUrl = url;
    m_pendingFeedId = existingFeedId;
    setBusy(true);
    setStatus(tr("Fetching feed…"));

    QNetworkRequest req;
    req.setUrl(QUrl(url));
    req.setRawHeader("User-Agent", "harbour-casts/1.0");
    m_feedReply = m_nam.get(req);
    connect(m_feedReply, &QNetworkReply::finished, this, &PodcastStore::onFeedReplyFinished);
}

void PodcastStore::onFeedReplyFinished()
{
    QNetworkReply *reply = m_feedReply;
    m_feedReply = nullptr;
    setBusy(false);

    if (!reply) return;

    const QString url = m_pendingFeedUrl;
    const int feedId = m_pendingFeedId;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "Feed fetch failed:" << url << reply->errorString();
        setError(tr("Feed fetch failed: %1").arg(reply->errorString()));
        return;
    }

    const QByteArray body = reply->readAll();
    if (body.isEmpty()) {
        setError(tr("Feed returned no data"));
        return;
    }

    ParsedFeed feed;
    if (!FeedParser::parseRss(body, url, &feed)) {
        qWarning() << "Feed parse failed:" << url << "bytes" << body.size();
        setError(tr("Could not parse feed (got %1 bytes)").arg(body.size()));
        return;
    }

    ingestFeed(feed, feedId);
}

void PodcastStore::ingestFeed(const ParsedFeed &feed, int existingFeedId)
{
    QSqlQuery q;
    int feedId = existingFeedId;

    if (feedId < 0) {
        q.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO feeds (title, url, image_url, description, last_refresh)"
            " VALUES (?, ?, ?, ?, ?)"));
        q.addBindValue(feed.title);
        q.addBindValue(feed.url);
        q.addBindValue(feed.imageUrl);
        q.addBindValue(feed.description);
        q.addBindValue(QDateTime::currentDateTime().toMSecsSinceEpoch() / 1000);
        if (!q.exec()) {
            emit error(tr("Database error: %1").arg(q.lastError().text()));
            return;
        }
        if (q.numRowsAffected() == 0) {
            q.prepare(QStringLiteral("SELECT id FROM feeds WHERE url=?"));
            q.addBindValue(feed.url);
            q.exec();
            if (!q.next()) return;
            feedId = q.value(0).toInt();
        } else {
            feedId = q.lastInsertId().toInt();
        }
        emit feedAdded(feedId);
    } else {
        q.prepare(QStringLiteral(
            "UPDATE feeds SET title=?, image_url=?, description=?, last_refresh=? WHERE id=?"));
        q.addBindValue(feed.title);
        q.addBindValue(feed.imageUrl);
        q.addBindValue(feed.description);
        q.addBindValue(QDateTime::currentDateTime().toMSecsSinceEpoch() / 1000);
        q.addBindValue(feedId);
        q.exec();
        emit feedUpdated(feedId);
    }

    if (!feed.imageUrl.isEmpty())
        m_artwork->prefetch(feedId, feed.imageUrl);

    const bool initialImport = existingFeedId < 0;
    const int importLimit = initialImport ? 100 : feed.episodes.size();
    int imported = 0;
    for (const ParsedEpisode &ep : feed.episodes) {
        if (ep.audioUrl.isEmpty()) continue;
        if (imported >= importLimit) break;
        q.prepare(QStringLiteral(
            "INSERT INTO episodes"
            " (feed_id, guid, title, audio_url, duration_sec, pub_date, description)"
            " VALUES (?, ?, ?, ?, ?, ?, ?)"
            " ON CONFLICT(feed_id, guid) DO UPDATE SET"
            " title=excluded.title,"
            " audio_url=excluded.audio_url,"
            " duration_sec=excluded.duration_sec,"
            " pub_date=excluded.pub_date,"
            " description=CASE WHEN length(excluded.description)>0"
            " THEN excluded.description ELSE episodes.description END"));
        q.addBindValue(feedId);
        q.addBindValue(ep.guid);
        q.addBindValue(ep.title);
        q.addBindValue(ep.audioUrl);
        q.addBindValue(ep.durationSec);
        q.addBindValue(ep.pubDate);
        q.addBindValue(ep.description);
        q.exec();
        ++imported;
    }

    m_subscriptions->reload();
    emit episodesChanged();

    setError(QString());
    if (initialImport && feed.episodes.size() > importLimit) {
        setStatus(tr("Subscribed to %1 · %2 recent episodes").arg(feed.title).arg(imported));
    } else {
        setStatus(tr("Subscribed to %1 · %2 episodes").arg(feed.title).arg(imported));
    }
}

void PodcastStore::removeFeed(int feedId)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("DELETE FROM queue WHERE episode_id IN (SELECT id FROM episodes WHERE feed_id=?)"));
    q.addBindValue(feedId);
    q.exec();
    q.prepare(QStringLiteral("DELETE FROM episodes WHERE feed_id=?"));
    q.addBindValue(feedId);
    q.exec();
    q.prepare(QStringLiteral("DELETE FROM feeds WHERE id=?"));
    q.addBindValue(feedId);
    q.exec();
    m_subscriptions->reload();
    updateQueueCount();
    emit episodesChanged();
}

void PodcastStore::importOpmlFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        emit error(tr("Cannot read OPML file"));
        return;
    }
    const QVector<OpmlOutline> outlines = FeedParser::parseOpml(f.readAll());
    if (outlines.isEmpty()) {
        emit error(tr("No subscriptions found in OPML"));
        return;
    }
    setStatus(tr("Importing %1 feeds…").arg(outlines.size()));
    for (const OpmlOutline &o : outlines)
        fetchFeed(o.xmlUrl);
}

void PodcastStore::loadEpisodes(int feedId)
{
    m_episodes->loadFeed(feedId);
}

void PodcastStore::loadMoreEpisodes()
{
    m_episodes->loadMore();
}

bool PodcastStore::episodesHasMore() const
{
    return m_episodes ? m_episodes->hasMore() : false;
}

int PodcastStore::episodesTotalCount() const
{
    return m_episodes ? m_episodes->totalCount() : 0;
}

void PodcastStore::openFeed(int feedId)
{
    QString title;
    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT title FROM feeds WHERE id=?"));
    q.addBindValue(feedId);
    if (q.exec() && q.next())
        title = q.value(0).toString();

    const bool changed = m_openFeedId != feedId || m_openFeedTitle != title;
    m_openFeedId = feedId;
    m_openFeedTitle = title;
    if (changed)
        emit openFeedChanged();
}

void PodcastStore::openEpisodeNotes(int episodeId)
{
    const QString title = episodeTitle(episodeId);
    const bool changed = m_openEpisodeId != episodeId || m_openEpisodeTitle != title;
    m_openEpisodeId = episodeId;
    m_openEpisodeTitle = title;
    if (changed)
        emit openEpisodeChanged();
}

int PodcastStore::feedIdForEpisode(int episodeId) const
{
    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT feed_id FROM episodes WHERE id=?"));
    q.addBindValue(episodeId);
    if (q.exec() && q.next()) return q.value(0).toInt();
    return 0;
}

QString PodcastStore::episodeTitle(int episodeId) const
{
    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT title FROM episodes WHERE id=?"));
    q.addBindValue(episodeId);
    return q.exec() && q.next() ? q.value(0).toString() : QString();
}

QString PodcastStore::episodeDescription(int episodeId) const
{
    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT description FROM episodes WHERE id=?"));
    q.addBindValue(episodeId);
    if (q.exec() && q.next()) {
        const QString desc = q.value(0).toString().trimmed();
        if (!desc.isEmpty()) return desc;
    }
    return tr("No show notes for this episode.");
}

QString PodcastStore::artworkForFeed(int feedId, const QString &remoteUrl) const
{
    return m_artwork ? m_artwork->resolve(feedId, remoteUrl) : remoteUrl;
}

QString PodcastStore::episodeAudioUrl(int episodeId) const
{
    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT audio_url FROM episodes WHERE id=?"));
    q.addBindValue(episodeId);
    return q.exec() && q.next() ? q.value(0).toString() : QString();
}

QString PodcastStore::episodeLocalPath(int episodeId) const
{
    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT local_path FROM episodes WHERE id=?"));
    q.addBindValue(episodeId);
    if (q.exec() && q.next()) {
        const QString path = q.value(0).toString();
        if (!path.isEmpty() && QFile::exists(path)) return path;
    }
    return QString();
}

int PodcastStore::episodePositionMs(int episodeId) const
{
    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT position_ms FROM episodes WHERE id=?"));
    q.addBindValue(episodeId);
    return q.exec() && q.next() ? q.value(0).toInt() : 0;
}

bool PodcastStore::episodeCompleted(int episodeId) const
{
    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT completed FROM episodes WHERE id=?"));
    q.addBindValue(episodeId);
    return q.exec() && q.next() && q.value(0).toInt() != 0;
}

void PodcastStore::markCompleted(int episodeId, bool completed)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("UPDATE episodes SET completed=? WHERE id=?"));
    q.addBindValue(completed ? 1 : 0);
    q.addBindValue(episodeId);
    q.exec();
    m_episodes->reload();
}

void PodcastStore::savePosition(int episodeId, int positionMs)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("UPDATE episodes SET position_ms=? WHERE id=?"));
    q.addBindValue(positionMs);
    q.addBindValue(episodeId);
    q.exec();
}

void PodcastStore::downloadEpisode(int episodeId)
{
    if (m_downloadReply) cancelDownload();

    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT title, audio_url, local_path FROM episodes WHERE id=?"));
    q.addBindValue(episodeId);
    if (!q.exec() || !q.next()) return;

    const QString existing = q.value(2).toString();
    if (!existing.isEmpty() && QFile::exists(existing)) {
        emit downloadFinished(episodeId, true);
        m_episodes->reload();
        return;
    }

    const QString title = q.value(0).toString();
    const QString audioUrl = q.value(1).toString();
    QString safe = title;
    safe.replace(QRegularExpression(QStringLiteral("[^a-zA-Z0-9._-]+")), QStringLiteral("_"));
    if (safe.length() > 80) safe = safe.left(80);

    const QString ext = audioUrl.contains(QStringLiteral(".ogg"), Qt::CaseInsensitive)
        ? QStringLiteral(".ogg") : QStringLiteral(".mp3");
    m_downloadTargetPath = downloadsDir() + QLatin1Char('/') + safe + ext;
    m_downloadEpisodeId = episodeId;

    QNetworkRequest req;
    req.setUrl(QUrl(audioUrl));
    req.setRawHeader("User-Agent", "harbour-casts/1.0");
    m_downloadReply = m_nam.get(req);
    connect(m_downloadReply, &QNetworkReply::downloadProgress, this,
            [this](qint64 received, qint64 total) {
                if (total > 0)
                    emit downloadProgress(m_downloadEpisodeId, int(received * 100 / total));
            });
    connect(m_downloadReply, &QNetworkReply::finished, this, [this]() {
        const int epId = m_downloadEpisodeId;
        QNetworkReply *reply = m_downloadReply;
        m_downloadReply = nullptr;
        if (!reply) return;

        bool ok = reply->error() == QNetworkReply::NoError;
        if (ok) {
            QFile f(m_downloadTargetPath);
            ok = f.open(QIODevice::WriteOnly) && f.write(reply->readAll()) >= 0;
            f.close();
            if (ok) {
                QSqlQuery q;
                q.prepare(QStringLiteral("UPDATE episodes SET local_path=? WHERE id=?"));
                q.addBindValue(m_downloadTargetPath);
                q.addBindValue(epId);
                q.exec();
            }
        }
        reply->deleteLater();
        emit downloadFinished(epId, ok);
        m_episodes->reload();
    });
}

void PodcastStore::cancelDownload()
{
    if (!m_downloadReply) return;
    m_downloadReply->abort();
    m_downloadReply->deleteLater();
    m_downloadReply = nullptr;
}

bool PodcastStore::isDownloading(int episodeId) const
{
    return m_downloadReply && m_downloadEpisodeId == episodeId;
}

void PodcastStore::updateQueueCount()
{
    QSqlQuery q(QStringLiteral("SELECT COUNT(*) FROM queue"));
    int count = 0;
    if (q.exec() && q.next()) count = q.value(0).toInt();
    if (m_queueCount != count) {
        m_queueCount = count;
        emit queueChanged();
    }
}

void PodcastStore::addToQueue(int episodeId)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT COALESCE(MAX(sort_order), 0) + 1 FROM queue"));
    int order = 1;
    if (q.exec() && q.next()) order = q.value(0).toInt();

    q.prepare(QStringLiteral("INSERT OR IGNORE INTO queue (episode_id, sort_order) VALUES (?, ?)"));
    q.addBindValue(episodeId);
    q.addBindValue(order);
    q.exec();
    updateQueueCount();
}

void PodcastStore::removeFromQueue(int episodeId)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("DELETE FROM queue WHERE episode_id=?"));
    q.addBindValue(episodeId);
    q.exec();
    updateQueueCount();
}

void PodcastStore::clearQueue()
{
    QSqlQuery q(QStringLiteral("DELETE FROM queue"));
    q.exec();
    updateQueueCount();
}

int PodcastStore::nextQueuedEpisode(int afterEpisodeId) const
{
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "SELECT episode_id FROM queue"
        " WHERE sort_order > COALESCE("
        "  (SELECT sort_order FROM queue WHERE episode_id=?), 0)"
        " ORDER BY sort_order LIMIT 1"));
    q.addBindValue(afterEpisodeId);
    if (q.exec() && q.next()) return q.value(0).toInt();

    q.prepare(QStringLiteral("SELECT episode_id FROM queue ORDER BY sort_order LIMIT 1"));
    if (q.exec() && q.next()) return q.value(0).toInt();
    return 0;
}

QVariantList PodcastStore::queueItems() const
{
    QVariantList items;
    QSqlQuery q(QStringLiteral(
        "SELECT e.id, e.title, f.title FROM queue q"
        " JOIN episodes e ON e.id=q.episode_id"
        " JOIN feeds f ON f.id=e.feed_id"
        " ORDER BY q.sort_order"));
    while (q.next()) {
        QVariantMap row;
        row.insert(QStringLiteral("episodeId"), q.value(0).toInt());
        row.insert(QStringLiteral("title"), q.value(1).toString());
        row.insert(QStringLiteral("feedTitle"), q.value(2).toString());
        items.append(row);
    }
    return items;
}

QString PodcastStore::formatDuration(int seconds) const
{
    if (seconds <= 0) return QStringLiteral("—");
    const int h = seconds / 3600;
    const int m = (seconds % 3600) / 60;
    const int s = seconds % 60;
    if (h > 0)
        return QStringLiteral("%1:%2:%3").arg(h).arg(m, 2, 10, QLatin1Char('0')).arg(s, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1:%2").arg(m).arg(s, 2, 10, QLatin1Char('0'));
}

QString PodcastStore::formatRelativeDate(qint64 epoch) const
{
    if (epoch <= 0) return QStringLiteral("—");
    const QDateTime dt = QDateTime::fromMSecsSinceEpoch(epoch * 1000);
    const int days = dt.daysTo(QDateTime::currentDateTime());
    if (days == 0) return tr("Today");
    if (days == 1) return tr("Yesterday");
    if (days < 7) return tr("%1 days ago").arg(days);
    return dt.date().toString(Qt::SystemLocaleShortDate);
}

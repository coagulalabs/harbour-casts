#include "podcaststore.h"

#include "artworkcache.h"
#include "episodesmodel.h"
#include "feedparser.h"
#include "subscriptionsmodel.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
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
    configureRequest(&req, QUrl(url));
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
        if (isUsableLocalFile(path)) return path;
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

void PodcastStore::configureRequest(QNetworkRequest *req, const QUrl &url, bool followRedirects) const
{
    req->setUrl(url);
    // Browser-like UA: some CDNs reject short custom agents.
    req->setRawHeader("User-Agent",
                      "Mozilla/5.0 (Mobile; Sailfish; rv:1.0) harbour-casts/1.1");
    req->setRawHeader("Accept", "*/*");
    if (followRedirects) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
        req->setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif
    } else {
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
        req->setAttribute(QNetworkRequest::FollowRedirectsAttribute, false);
#endif
    }
}

bool PodcastStore::isRedirectStatus(int status)
{
    return status == 301 || status == 302 || status == 303
        || status == 307 || status == 308;
}

void PodcastStore::resetDownloadIO()
{
    if (m_downloadReply) {
        disconnect(m_downloadReply, nullptr, this, nullptr);
        m_downloadReply->abort();
        m_downloadReply->deleteLater();
        m_downloadReply = nullptr;
    }
    if (m_downloadFile) {
        if (m_downloadFile->isOpen())
            m_downloadFile->close();
        m_downloadFile->deleteLater();
        m_downloadFile = nullptr;
    }
    if (!m_downloadTargetPath.isEmpty())
        QFile::remove(m_downloadTargetPath);
}

bool PodcastStore::openDownloadFile()
{
    if (m_downloadFile)
        return m_downloadFile->isOpen();

    QFile::remove(m_downloadTargetPath);
    m_downloadFile = new QFile(m_downloadTargetPath, this);
    if (!m_downloadFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        const QString err = tr("Cannot write download: %1").arg(m_downloadFile->errorString());
        qWarning() << err << "path" << m_downloadTargetPath;
        m_downloadFile->deleteLater();
        m_downloadFile = nullptr;
        finishDownload(false, err);
        return false;
    }
    qWarning() << "Download writing to" << m_downloadTargetPath;
    return true;
}

void PodcastStore::issueDownloadRequest(const QUrl &url)
{
    if (!url.isValid()) {
        finishDownload(false, tr("Invalid download URL"));
        return;
    }

    QNetworkRequest req;
    // Manual redirects: Qt 5.6 can prepend redirect bodies when auto-following,
    // which corrupts the audio file and fails validation.
    configureRequest(&req, url, false);
    req.setRawHeader("Accept", "audio/*,*/*;q=0.2");

    m_downloadReply = m_nam.get(req);
    connect(m_downloadReply, &QNetworkReply::metaDataChanged,
            this, &PodcastStore::onDownloadMetaDataChanged);
    connect(m_downloadReply, &QNetworkReply::readyRead,
            this, &PodcastStore::onDownloadReadyRead);
    connect(m_downloadReply, &QNetworkReply::downloadProgress, this,
            [this](qint64 received, qint64 total) {
                // Ignore tiny redirect payloads in the UI.
                if (!m_downloadFile) return;
                m_downloadBytes = received;
                int percent = 0;
                if (total > 0)
                    percent = int(qMin(qint64(100), (received * 100) / total));
                else if (received > 0)
                    percent = -1;
                if (m_downloadPercent != percent) {
                    m_downloadPercent = percent;
                    emit downloadProgress(m_downloadEpisodeId, m_downloadPercent);
                }
                if (percent >= 0)
                    setStatus(tr("Downloading “%1”… %2%").arg(m_downloadTitle).arg(percent));
                else
                    setStatus(tr("Downloading “%1”… %2 KB")
                                  .arg(m_downloadTitle)
                                  .arg(received / 1024));
            });
    connect(m_downloadReply, &QNetworkReply::finished,
            this, &PodcastStore::onDownloadFinished);
}

bool PodcastStore::isUsableLocalFile(const QString &path) const
{
    if (path.isEmpty()) return false;
    QFileInfo info(path);
    // Redirect/error bodies we previously saved were ~1KB text.
    if (!info.exists() || !info.isFile() || info.size() < 64 * 1024)
        return false;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QByteArray head = f.read(64);
    if (head.startsWith("Temporary Redirect") || head.startsWith("Redirecting")
        || head.startsWith("<!DOCTYPE") || head.startsWith("<html")
        || head.startsWith("<?xml") || head.startsWith("{")) {
        return false;
    }
    // ID3 / MPEG frame sync / Ogg / fLaC / MP4 ftyp
    if (head.startsWith("ID3") || head.startsWith("OggS") || head.startsWith("fLaC"))
        return true;
    if (head.size() >= 2) {
        const uchar b0 = uchar(head.at(0));
        const uchar b1 = uchar(head.at(1));
        if (b0 == 0xFF && (b1 & 0xE0) == 0xE0) return true;
    }
    if (head.size() >= 8 && head.mid(4, 4) == "ftyp") return true;
    // Large opaque binary is still usable for offline playback.
    return true;
}

void PodcastStore::clearLocalPath(int episodeId)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT local_path FROM episodes WHERE id=?"));
    q.addBindValue(episodeId);
    if (q.exec() && q.next()) {
        const QString path = q.value(0).toString();
        if (!path.isEmpty())
            QFile::remove(path);
    }
    q.prepare(QStringLiteral("UPDATE episodes SET local_path='' WHERE id=?"));
    q.addBindValue(episodeId);
    q.exec();
}

QString PodcastStore::extensionForReply(QNetworkReply *reply, const QString &audioUrl) const
{
    const QUrl finalUrl = reply ? reply->url() : QUrl(audioUrl);
    const QString path = finalUrl.path().toLower();
    if (path.endsWith(QLatin1String(".m4a")) || path.endsWith(QLatin1String(".aac")))
        return QStringLiteral(".m4a");
    if (path.endsWith(QLatin1String(".ogg")) || path.endsWith(QLatin1String(".opus")))
        return QStringLiteral(".ogg");
    if (path.endsWith(QLatin1String(".flac")))
        return QStringLiteral(".flac");
    if (path.endsWith(QLatin1String(".mp3")))
        return QStringLiteral(".mp3");

    const QByteArray ctype = reply
        ? reply->header(QNetworkRequest::ContentTypeHeader).toByteArray().toLower()
        : QByteArray();
    if (ctype.contains("mp4") || ctype.contains("m4a") || ctype.contains("aac"))
        return QStringLiteral(".m4a");
    if (ctype.contains("ogg") || ctype.contains("opus"))
        return QStringLiteral(".ogg");
    if (ctype.contains("flac"))
        return QStringLiteral(".flac");
    return QStringLiteral(".mp3");
}

void PodcastStore::downloadEpisode(int episodeId)
{
    resetDownloadIO();
    m_downloadEpisodeId = 0;
    m_downloadPercent = 0;
    m_downloadBytes = 0;
    m_downloadRedirects = 0;
    m_downloadTargetPath.clear();
    m_downloadSafeName.clear();
    m_downloadTitle.clear();
    m_downloadSourceUrl.clear();
    emit downloadStateChanged();

    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT title, audio_url, local_path FROM episodes WHERE id=?"));
    q.addBindValue(episodeId);
    if (!q.exec() || !q.next()) return;

    const QString existing = q.value(2).toString();
    if (isUsableLocalFile(existing)) {
        setStatus(tr("Already downloaded"));
        emit downloadFinished(episodeId, true);
        m_episodes->reload();
        return;
    }
    if (!existing.isEmpty()) {
        // Stale redirect/error body from older builds — wipe and retry.
        QFile::remove(existing);
        clearLocalPath(episodeId);
    }

    const QString title = q.value(0).toString();
    const QString audioUrl = q.value(1).toString();
    if (audioUrl.isEmpty()) {
        setError(tr("Episode has no audio URL"));
        emit downloadFinished(episodeId, false);
        return;
    }

    QString safe = title;
    safe.replace(QRegularExpression(QStringLiteral("[^a-zA-Z0-9._-]+")), QStringLiteral("_"));
    if (safe.isEmpty()) safe = QStringLiteral("episode-%1").arg(episodeId);
    if (safe.length() > 80) safe = safe.left(80);

    m_downloadEpisodeId = episodeId;
    m_downloadPercent = 0;
    m_downloadBytes = 0;
    m_downloadRedirects = 0;
    m_downloadTitle = title;
    m_downloadSafeName = safe;
    m_downloadSourceUrl = audioUrl;
    m_downloadTargetPath = downloadsDir() + QLatin1Char('/') + safe + QStringLiteral(".part");
    QFile::remove(m_downloadTargetPath);

    emit downloadStateChanged();
    emit downloadProgress(m_downloadEpisodeId, 0);
    setStatus(tr("Downloading “%1”…").arg(m_downloadTitle));
    m_episodes->reload();

    qWarning() << "Download start" << audioUrl << "-> dir" << downloadsDir();
    issueDownloadRequest(QUrl(audioUrl));
}

void PodcastStore::onDownloadMetaDataChanged()
{
    if (!m_downloadReply) return;
    const int status = m_downloadReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (isRedirectStatus(status)) {
        // Discard redirect payload; never open the audio file for these.
        m_downloadReply->readAll();
        return;
    }
    if (status == 200 || status == 206) {
        if (openDownloadFile())
            onDownloadReadyRead(); // flush any already-buffered body
    }
}

void PodcastStore::onDownloadReadyRead()
{
    if (!m_downloadReply || !m_downloadFile) return;
    const QByteArray chunk = m_downloadReply->readAll();
    if (!chunk.isEmpty())
        m_downloadFile->write(chunk);
}

void PodcastStore::onDownloadFinished()
{
    QNetworkReply *reply = m_downloadReply;
    m_downloadReply = nullptr;
    if (!reply) {
        finishDownload(false, tr("Download aborted"));
        return;
    }

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    qWarning() << "Download finished status" << status
               << "error" << reply->error() << reply->errorString()
               << "url" << reply->url()
               << "bytes" << (m_downloadFile ? m_downloadFile->size() : -1)
               << "redirects" << m_downloadRedirects;

    if (isRedirectStatus(status)
        || reply->attribute(QNetworkRequest::RedirectionTargetAttribute).isValid()) {
        QUrl next = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
        if (next.isRelative())
            next = reply->url().resolved(next);
        reply->deleteLater();

        if (!next.isValid() || next.toString().isEmpty()) {
            finishDownload(false, tr("Download redirect missing location"));
            return;
        }
        if (++m_downloadRedirects > 10) {
            finishDownload(false, tr("Too many download redirects"));
            return;
        }

        setStatus(tr("Downloading “%1”… (redirect %2)")
                      .arg(m_downloadTitle)
                      .arg(m_downloadRedirects));
        issueDownloadRequest(next);
        return;
    }

    // Flush any remaining buffered bytes (only if we opened for a 200).
    if (m_downloadFile)
        m_downloadFile->write(reply->readAll());

    const int epId = m_downloadEpisodeId;
    const QString title = m_downloadTitle;
    QString err;

    const QByteArray ctype = reply->header(QNetworkRequest::ContentTypeHeader).toByteArray().toLower();
    bool ok = reply->error() == QNetworkReply::NoError;
    if (ok && status > 0 && status != 200 && status != 206) {
        ok = false;
        err = tr("Download failed (HTTP %1)").arg(status);
    }
    if (ok && (ctype.startsWith("text/html") || ctype.startsWith("text/plain")
               || ctype.startsWith("application/json"))) {
        ok = false;
        err = tr("Server returned %1 instead of audio").arg(QString::fromUtf8(ctype));
    }
    if (ok && !m_downloadFile) {
        ok = false;
        err = tr("Download produced no file");
    }

    if (m_downloadFile) {
        m_downloadFile->flush();
        m_downloadFile->close();
    }

    QString finalPath;
    if (ok) {
        const QString ext = extensionForReply(reply, m_downloadSourceUrl);
        finalPath = downloadsDir() + QLatin1Char('/') + m_downloadSafeName + ext;
        QFile::remove(finalPath);
        if (!QFile::rename(m_downloadTargetPath, finalPath)) {
            ok = QFile::copy(m_downloadTargetPath, finalPath);
            QFile::remove(m_downloadTargetPath);
        }
        if (ok && !isUsableLocalFile(finalPath)) {
            ok = false;
            err = tr("Downloaded file is not valid audio (%1 bytes)")
                      .arg(QFileInfo(finalPath).size());
            QFile::remove(finalPath);
        }
    }

    reply->deleteLater();

    if (ok) {
        QSqlQuery q;
        q.prepare(QStringLiteral("UPDATE episodes SET local_path=? WHERE id=?"));
        q.addBindValue(finalPath);
        q.addBindValue(epId);
        q.exec();
        qWarning() << "Download OK" << finalPath << QFileInfo(finalPath).size();
        finishDownload(true);
        setStatus(tr("Downloaded “%1”").arg(title));
    } else {
        QFile::remove(m_downloadTargetPath);
        if (err.isEmpty())
            err = reply->error() != QNetworkReply::NoError
                ? tr("Download failed: %1").arg(reply->errorString())
                : tr("Download failed");
        qWarning() << "Download FAIL" << err;
        finishDownload(false, err);
    }
}

void PodcastStore::finishDownload(bool success, const QString &errorMessage)
{
    if (m_downloadFile) {
        if (m_downloadFile->isOpen())
            m_downloadFile->close();
        m_downloadFile->deleteLater();
        m_downloadFile = nullptr;
    }

    const int epId = m_downloadEpisodeId;
    m_downloadEpisodeId = 0;
    m_downloadPercent = 0;
    m_downloadBytes = 0;
    m_downloadRedirects = 0;
    m_downloadTargetPath.clear();
    m_downloadSafeName.clear();
    m_downloadTitle.clear();
    m_downloadSourceUrl.clear();
    emit downloadStateChanged();
    emit downloadProgress(0, 0);

    if (!success && !errorMessage.isEmpty())
        setError(errorMessage);
    if (epId > 0)
        emit downloadFinished(epId, success);
    m_episodes->reload();
}

void PodcastStore::cancelDownload()
{
    if (!m_downloadReply && !m_downloadFile && m_downloadEpisodeId == 0)
        return;

    resetDownloadIO();
    finishDownload(false, tr("Download cancelled"));
}

bool PodcastStore::isDownloading(int episodeId) const
{
    return m_downloadEpisodeId == episodeId && (m_downloadReply || m_downloadFile);
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

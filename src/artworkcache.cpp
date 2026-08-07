#include "artworkcache.h"

#include <QDir>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QUrl>
#include <QDebug>

namespace {

bool looksLikeImage(const QByteArray &data)
{
    if (data.size() < 24)
        return false;
    // JPEG / PNG / GIF / WEBP (RIFF....WEBP)
    if (uchar(data.at(0)) == 0xFF && uchar(data.at(1)) == 0xD8)
        return true;
    if (data.startsWith("\x89PNG\r\n\x1a\n"))
        return true;
    if (data.startsWith("GIF87a") || data.startsWith("GIF89a"))
        return true;
    if (data.size() >= 12 && data.startsWith("RIFF") && data.mid(8, 4) == "WEBP")
        return true;
    return false;
}

QString upgradeToHttps(const QString &remoteUrl)
{
    QUrl url(remoteUrl);
    if (url.scheme() == QLatin1String("http")) {
        url.setScheme(QStringLiteral("https"));
        return url.toString();
    }
    return remoteUrl;
}

} // namespace

ArtworkCache::ArtworkCache(QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent)
    , m_nam(nam)
{
}

QString ArtworkCache::cacheDir() const
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    const QString dir = base + QStringLiteral("/artwork");
    QDir().mkpath(dir);
    return dir;
}

QString ArtworkCache::extensionForUrl(const QString &url) const
{
    const QString path = QUrl(url).path().toLower();
    if (path.endsWith(QStringLiteral(".png"))) return QStringLiteral(".png");
    if (path.endsWith(QStringLiteral(".webp"))) return QStringLiteral(".webp");
    if (path.endsWith(QStringLiteral(".gif"))) return QStringLiteral(".gif");
    return QStringLiteral(".jpg");
}

QString ArtworkCache::localPath(int feedId) const
{
    if (feedId <= 0) return QString();
    const QDir dir(cacheDir());
    const QStringList matches = dir.entryList(
        QStringList() << QString::number(feedId) + QStringLiteral(".*"),
        QDir::Files);
    if (matches.isEmpty()) return QString();
    const QString path = dir.filePath(matches.first());
    // Reject leftover redirect/error bodies (e.g. 167-byte CloudFront HTML).
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly) || f.size() < 256)
        return QString();
    const QByteArray head = f.read(16);
    if (!looksLikeImage(head))
        return QString();
    return path;
}

QString ArtworkCache::resolve(int feedId, const QString &remoteUrl) const
{
    const QString local = localPath(feedId);
    if (!local.isEmpty())
        return QUrl::fromLocalFile(local).toString();
    return remoteUrl;
}

void ArtworkCache::prefetch(int feedId, const QString &remoteUrl)
{
    if (!m_nam || feedId <= 0 || remoteUrl.isEmpty()) return;
    if (!localPath(feedId).isEmpty()) {
        emit artworkCached(feedId);
        return;
    }
    if (m_inFlight.contains(feedId)) return;

    // Drop any corrupt cache entry so we can rewrite it.
    const QDir dir(cacheDir());
    const QStringList matches = dir.entryList(
        QStringList() << QString::number(feedId) + QStringLiteral(".*"),
        QDir::Files);
    for (const QString &name : matches)
        QFile::remove(dir.filePath(name));

    QNetworkRequest req;
    req.setUrl(QUrl(upgradeToHttps(remoteUrl)));
    req.setRawHeader("User-Agent",
                     "Mozilla/5.0 (Mobile; Sailfish; rv:1.0) harbour-casts/1.1");
    req.setRawHeader("Accept", "image/*,*/*;q=0.2");
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
    req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif
    QNetworkReply *reply = m_nam->get(req);
    m_inFlight.insert(feedId);
    m_replyFeedIds.insert(reply, feedId);
    connect(reply, &QNetworkReply::finished, this, &ArtworkCache::onReplyFinished);
}

void ArtworkCache::onReplyFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) return;

    const int feedId = m_replyFeedIds.take(reply);
    m_inFlight.remove(feedId);

    if (reply->error() == QNetworkReply::NoError) {
        const QByteArray data = reply->readAll();
        if (looksLikeImage(data)) {
            const QString path = cacheDir() + QLatin1Char('/')
                + QString::number(feedId) + extensionForUrl(reply->url().toString());
            QFile f(path);
            if (f.open(QIODevice::WriteOnly)) {
                f.write(data);
                f.close();
                emit artworkCached(feedId);
            }
        } else {
            qWarning() << "Artwork fetch not an image for feed" << feedId
                       << "bytes" << data.size()
                       << "url" << reply->url();
        }
    } else {
        qWarning() << "Artwork fetch failed for feed" << feedId
                   << reply->errorString() << reply->url();
    }

    reply->deleteLater();
}

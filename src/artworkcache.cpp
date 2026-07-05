#include "artworkcache.h"

#include <QDir>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QUrl>

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
    return dir.filePath(matches.first());
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

    QNetworkRequest req;
    req.setUrl(QUrl(remoteUrl));
    req.setRawHeader("User-Agent", "harbour-casts/1.1");
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
        if (!data.isEmpty()) {
            const QString path = cacheDir() + QLatin1Char('/')
                + QString::number(feedId) + extensionForUrl(reply->url().toString());
            QFile f(path);
            if (f.open(QIODevice::WriteOnly)) {
                f.write(data);
                f.close();
                emit artworkCached(feedId);
            }
        }
    }

    reply->deleteLater();
}

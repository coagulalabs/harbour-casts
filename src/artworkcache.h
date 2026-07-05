#ifndef ARTWORKCACHE_H
#define ARTWORKCACHE_H

#include <QHash>
#include <QObject>
#include <QSet>

class QNetworkAccessManager;
class QNetworkReply;

class ArtworkCache : public QObject
{
    Q_OBJECT

public:
    explicit ArtworkCache(QNetworkAccessManager *nam, QObject *parent = nullptr);

    QString cacheDir() const;
    QString localPath(int feedId) const;
    QString resolve(int feedId, const QString &remoteUrl) const;
    void prefetch(int feedId, const QString &remoteUrl);

signals:
    void artworkCached(int feedId);

private slots:
    void onReplyFinished();

private:
    QString extensionForUrl(const QString &url) const;

    QNetworkAccessManager *m_nam = nullptr;
    QSet<int> m_inFlight;
    QHash<QNetworkReply *, int> m_replyFeedIds;
};

#endif // ARTWORKCACHE_H

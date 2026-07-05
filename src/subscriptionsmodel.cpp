#include "subscriptionsmodel.h"

#include "artworkcache.h"

#include <QSqlQuery>

SubscriptionsModel::SubscriptionsModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void SubscriptionsModel::setArtworkCache(ArtworkCache *cache)
{
    if (m_cache == cache) return;
    if (m_cache)
        disconnect(m_cache, nullptr, this, nullptr);
    m_cache = cache;
    if (m_cache)
        connect(m_cache, &ArtworkCache::artworkCached, this, &SubscriptionsModel::onArtworkCached);
}

int SubscriptionsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_rows.size();
}

QVariant SubscriptionsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size()) return QVariant();
    const Row &r = m_rows.at(index.row());
    switch (role) {
    case FeedIdRole: return r.id;
    case TitleRole: return r.title;
    case ImageUrlRole: return r.imageUrl;
    case EpisodeCountRole: return r.episodeCount;
    case UnplayedCountRole: return r.unplayedCount;
    default: return QVariant();
    }
}

QHash<int, QByteArray> SubscriptionsModel::roleNames() const
{
    return {
        { FeedIdRole, "feedId" },
        { TitleRole, "title" },
        { ImageUrlRole, "imageUrl" },
        { EpisodeCountRole, "episodeCount" },
        { UnplayedCountRole, "unplayedCount" }
    };
}

void SubscriptionsModel::reload()
{
    beginResetModel();
    m_rows.clear();

    QSqlQuery q(QStringLiteral(
        "SELECT f.id, f.title, f.image_url,"
        " (SELECT COUNT(*) FROM episodes e WHERE e.feed_id=f.id),"
        " (SELECT COUNT(*) FROM episodes e WHERE e.feed_id=f.id AND e.completed=0)"
        " FROM feeds f ORDER BY f.title COLLATE NOCASE"));
    while (q.next()) {
        Row r;
        r.id = q.value(0).toInt();
        r.title = q.value(1).toString();
        r.remoteImageUrl = q.value(2).toString();
        r.imageUrl = m_cache
            ? m_cache->resolve(r.id, r.remoteImageUrl)
            : r.remoteImageUrl;
        r.episodeCount = q.value(3).toInt();
        r.unplayedCount = q.value(4).toInt();
        m_rows.append(r);
        if (m_cache && !r.remoteImageUrl.isEmpty())
            m_cache->prefetch(r.id, r.remoteImageUrl);
    }
    endResetModel();
}

void SubscriptionsModel::onArtworkCached(int feedId)
{
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows.at(i).id != feedId) continue;
        m_rows[i].imageUrl = m_cache->resolve(feedId, m_rows.at(i).remoteImageUrl);
        const QModelIndex idx = index(i);
        emit dataChanged(idx, idx, { ImageUrlRole });
        break;
    }
}

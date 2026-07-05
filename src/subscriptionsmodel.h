#ifndef SUBSCRIPTIONSMODEL_H
#define SUBSCRIPTIONSMODEL_H

#include <QAbstractListModel>

class ArtworkCache;

class SubscriptionsModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        FeedIdRole = Qt::UserRole + 1,
        TitleRole,
        ImageUrlRole,
        EpisodeCountRole,
        UnplayedCountRole
    };

    explicit SubscriptionsModel(QObject *parent = nullptr);

    void setArtworkCache(ArtworkCache *cache);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void reload();

private slots:
    void onArtworkCached(int feedId);

private:
    ArtworkCache *m_cache = nullptr;
    struct Row {
        int id = 0;
        QString title;
        QString remoteImageUrl;
        QString imageUrl;
        int episodeCount = 0;
        int unplayedCount = 0;
    };
    QVector<Row> m_rows;
};

#endif // SUBSCRIPTIONSMODEL_H

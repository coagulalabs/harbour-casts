#ifndef EPISODESMODEL_H
#define EPISODESMODEL_H

#include <QAbstractListModel>
#include <QFutureWatcher>
#include <QVector>

struct EpisodeRow {
    int id = 0;
    QString title;
    int durationSec = 0;
    qint64 pubDate = 0;
    bool completed = false;
    bool downloaded = false;
    int positionMs = 0;
    bool inQueue = false;
};

struct EpisodeLoadResult {
    int generation = 0;
    int totalCount = 0;
    QVector<EpisodeRow> rows;
};

class EpisodesModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        EpisodeIdRole = Qt::UserRole + 1,
        TitleRole,
        DurationSecRole,
        PubDateRole,
        CompletedRole,
        DownloadedRole,
        PositionMsRole,
        InQueueRole
    };

    explicit EpisodesModel(QObject *parent = nullptr);

    bool loading() const { return m_loading; }
    bool hasMore() const { return m_rows.size() < m_totalCount; }
    int totalCount() const { return m_totalCount; }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setDatabasePath(const QString &path);
    Q_INVOKABLE void loadFeed(int feedId);
    Q_INVOKABLE void loadMore();
    void reload();

signals:
    void loadingChanged();
    void metaChanged();

private slots:
    void onLoadFinished();

private:
    void setLoading(bool loading);
    void startLoad();

    QString m_dbPath;
    int m_feedId = 0;
    int m_limit = 50;
    int m_totalCount = 0;
    int m_loadGeneration = 0;
    bool m_loading = false;
    QVector<EpisodeRow> m_rows;
    QFutureWatcher<EpisodeLoadResult> m_watcher;
};

#endif // EPISODESMODEL_H

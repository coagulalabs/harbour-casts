#include "episodesmodel.h"

#include <QtConcurrent>

#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QThread>

namespace {

int fetchEpisodeCount(const QString &dbPath, int feedId)
{
    if (feedId <= 0 || dbPath.isEmpty()) return 0;

    const QString connName = QStringLiteral("casts_episode_count_%1")
        .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));

    if (QSqlDatabase::contains(connName))
        QSqlDatabase::removeDatabase(connName);

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
    db.setDatabaseName(dbPath);
    if (!db.open()) {
        QSqlDatabase::removeDatabase(connName);
        return 0;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM episodes WHERE feed_id=?"));
    q.addBindValue(feedId);
    int count = 0;
    if (q.exec() && q.next())
        count = q.value(0).toInt();

    db.close();
    QSqlDatabase::removeDatabase(connName);
    return count;
}

QVector<EpisodeRow> fetchEpisodeRows(const QString &dbPath, int feedId, int limit)
{
    QVector<EpisodeRow> rows;
    if (feedId <= 0 || dbPath.isEmpty()) return rows;

    const QString connName = QStringLiteral("casts_episodes_%1")
        .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));

    if (QSqlDatabase::contains(connName))
        QSqlDatabase::removeDatabase(connName);

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
    db.setDatabaseName(dbPath);
    if (!db.open()) {
        QSqlDatabase::removeDatabase(connName);
        return rows;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT e.id, e.title, e.duration_sec, e.pub_date,"
        " e.completed, e.local_path, e.position_ms,"
        " (q.episode_id IS NOT NULL)"
        " FROM episodes e"
        " LEFT JOIN queue q ON q.episode_id = e.id"
        " WHERE e.feed_id=?"
        " ORDER BY e.pub_date DESC"
        " LIMIT ?"));
    q.addBindValue(feedId);
    q.addBindValue(limit);

    if (q.exec()) {
        while (q.next()) {
            EpisodeRow r;
            r.id = q.value(0).toInt();
            r.title = q.value(1).toString();
            r.durationSec = q.value(2).toInt();
            r.pubDate = q.value(3).toLongLong();
            r.completed = q.value(4).toInt() != 0;
            const QString local = q.value(5).toString();
            r.downloaded = false;
            if (!local.isEmpty()) {
                QFileInfo info(local);
                // Ignore tiny redirect/error stubs left by older builds.
                r.downloaded = info.isFile() && info.size() >= 64 * 1024;
            }
            r.positionMs = q.value(6).toInt();
            r.inQueue = q.value(7).toInt() != 0;
            rows.append(r);
        }
    }

    db.close();
    QSqlDatabase::removeDatabase(connName);
    return rows;
}

} // namespace

EpisodesModel::EpisodesModel(QObject *parent)
    : QAbstractListModel(parent)
{
    connect(&m_watcher, &QFutureWatcher<EpisodeLoadResult>::finished,
            this, &EpisodesModel::onLoadFinished);
}

int EpisodesModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_rows.size();
}

QVariant EpisodesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size()) return QVariant();
    const EpisodeRow &r = m_rows.at(index.row());
    switch (role) {
    case EpisodeIdRole: return r.id;
    case TitleRole: return r.title;
    case DurationSecRole: return r.durationSec;
    case PubDateRole: return r.pubDate;
    case CompletedRole: return r.completed;
    case DownloadedRole: return r.downloaded;
    case PositionMsRole: return r.positionMs;
    case InQueueRole: return r.inQueue;
    default: return QVariant();
    }
}

QHash<int, QByteArray> EpisodesModel::roleNames() const
{
    return {
        { EpisodeIdRole, "episodeId" },
        { TitleRole, "title" },
        { DurationSecRole, "durationSec" },
        { PubDateRole, "pubDate" },
        { CompletedRole, "completed" },
        { DownloadedRole, "downloaded" },
        { PositionMsRole, "positionMs" },
        { InQueueRole, "inQueue" }
    };
}

void EpisodesModel::setDatabasePath(const QString &path)
{
    m_dbPath = path;
}

void EpisodesModel::setLoading(bool loading)
{
    if (m_loading == loading) return;
    m_loading = loading;
    emit loadingChanged();
}

void EpisodesModel::loadFeed(int feedId)
{
    m_feedId = feedId;
    m_limit = 50;
    startLoad();
}

void EpisodesModel::loadMore()
{
    if (m_loading || !hasMore()) return;
    m_limit += 50;
    startLoad();
}

void EpisodesModel::reload()
{
    if (m_feedId > 0)
        startLoad();
}

void EpisodesModel::startLoad()
{
    if (m_dbPath.isEmpty() || m_feedId <= 0) return;

    beginResetModel();
    m_rows.clear();
    endResetModel();
    setLoading(true);

    const int generation = ++m_loadGeneration;
    const QString dbPath = m_dbPath;
    const int feedId = m_feedId;
    const int limit = m_limit;
    m_watcher.setFuture(QtConcurrent::run([dbPath, feedId, limit, generation]() {
        EpisodeLoadResult result;
        result.generation = generation;
        result.totalCount = fetchEpisodeCount(dbPath, feedId);
        result.rows = fetchEpisodeRows(dbPath, feedId, limit);
        return result;
    }));
}

void EpisodesModel::onLoadFinished()
{
    if (!m_watcher.isFinished()) return;

    const EpisodeLoadResult result = m_watcher.result();
    if (result.generation != m_loadGeneration) return;

    beginResetModel();
    m_rows = result.rows;
    m_totalCount = result.totalCount;
    endResetModel();
    setLoading(false);
    emit metaChanged();
}

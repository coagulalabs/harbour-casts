#include "playercontroller.h"

#include "podcaststore.h"

#include <QDateTime>
#include <QMediaContent>
#include <QSqlQuery>
#include <QUrl>

PlayerController::PlayerController(QObject *parent)
    : QObject(parent)
{
    m_positionTimer.setInterval(3000);
    connect(&m_positionTimer, &QTimer::timeout, this, &PlayerController::onPositionTick);
    connect(&m_player, &QMediaPlayer::positionChanged, this, &PlayerController::positionChanged);
    connect(&m_player, &QMediaPlayer::durationChanged, this, &PlayerController::durationChanged);
    connect(&m_player, &QMediaPlayer::stateChanged, this, &PlayerController::playbackChanged);
    connect(&m_player, &QMediaPlayer::mediaStatusChanged, this, &PlayerController::onMediaStatusChanged);

    m_sleepTimer.setSingleShot(true);
    connect(&m_sleepTimer, &QTimer::timeout, this, &PlayerController::onSleepTimer);

    m_sleepTicker.setInterval(1000);
    connect(&m_sleepTicker, &QTimer::timeout, this, &PlayerController::onSleepTick);
}

void PlayerController::loadEpisodeMetadata(int episodeId)
{
    m_episodeId = episodeId;
    m_title = m_store ? m_store->episodeTitle(episodeId) : QString();
    m_feedTitle.clear();

    QSqlQuery q;
    q.prepare(QStringLiteral(
        "SELECT f.title FROM feeds f"
        " JOIN episodes e ON e.feed_id=f.id WHERE e.id=?"));
    q.addBindValue(episodeId);
    if (q.exec() && q.next())
        m_feedTitle = q.value(0).toString();
}

void PlayerController::applyPlaybackRate()
{
    m_player.setPlaybackRate(m_playbackRate);
}

void PlayerController::playEpisode(int episodeId)
{
    if (!m_store || episodeId <= 0) return;

    saveProgress();

    const QString local = m_store->episodeLocalPath(episodeId);
    const QString remote = m_store->episodeAudioUrl(episodeId);
    const QUrl source = local.isEmpty() ? QUrl(remote) : QUrl::fromLocalFile(local);
    if (!source.isValid()) return;

    loadEpisodeMetadata(episodeId);

    m_player.setMedia(QMediaContent(source));

    const int pos = m_store->episodePositionMs(episodeId);
    m_resumePositionMs = pos;
    if (pos > 0)
        m_player.setPosition(pos);

    m_player.play();
    applyPlaybackRate();

    m_positionTimer.start();
    emit playbackChanged();
}

void PlayerController::togglePlay()
{
    if (m_episodeId <= 0) return;
    if (playing()) {
        pause();
        return;
    }

    if (m_player.mediaStatus() == QMediaPlayer::NoMedia
        || m_player.mediaStatus() == QMediaPlayer::InvalidMedia) {
        playEpisode(m_episodeId);
        return;
    }

    const int resumeMs = m_resumePositionMs > 0
        ? m_resumePositionMs
        : (m_store ? m_store->episodePositionMs(m_episodeId) : 0);

    m_player.play();
    applyPlaybackRate();
    if (resumeMs > 0)
        m_player.setPosition(resumeMs);

    m_positionTimer.start();
    emit playbackChanged();
    emit positionChanged();
}

void PlayerController::pause()
{
    m_player.pause();
    saveProgress();
    emit playbackChanged();
}

void PlayerController::stop()
{
    m_resumePositionMs = m_player.position();
    saveProgress();
    m_player.pause();
    m_positionTimer.stop();
    emit playbackChanged();
    emit positionChanged();
}

void PlayerController::seek(int positionMs)
{
    m_resumePositionMs = positionMs;
    m_player.setPosition(positionMs);
    emit positionChanged();
}

void PlayerController::skipForward()
{
    seek(qMin<qint64>(m_player.position() + 30000, m_player.duration()));
}

void PlayerController::skipBackward()
{
    seek(qMax<qint64>(m_player.position() - 15000, 0));
}

void PlayerController::setPlaybackRate(qreal rate)
{
    rate = qBound<qreal>(0.5, rate, 2.0);
    const bool changed = !qFuzzyCompare(m_playbackRate, rate);
    m_playbackRate = rate;
    m_player.setPlaybackRate(rate);
    if (changed)
        emit playbackRateChanged();
}

int PlayerController::sleepRemainingSec() const
{
    if (m_sleepEndsAtMs <= 0) return 0;
    const qint64 remainingMs = m_sleepEndsAtMs - QDateTime::currentMSecsSinceEpoch();
    return remainingMs > 0 ? static_cast<int>((remainingMs + 999) / 1000) : 0;
}

void PlayerController::setSleepMinutes(int minutes)
{
    minutes = qMax(0, minutes);

    if (minutes == 0) {
        if (m_sleepMinutes == 0 && !m_sleepTimer.isActive()) return;
        m_sleepMinutes = 0;
        m_sleepEndsAtMs = 0;
        m_sleepTimer.stop();
        m_sleepTicker.stop();
        emit sleepMinutesChanged();
        emit sleepRemainingChanged();
        return;
    }

    m_sleepMinutes = minutes;
    m_sleepEndsAtMs = QDateTime::currentMSecsSinceEpoch()
        + static_cast<qint64>(minutes) * 60 * 1000;
    m_sleepTimer.start(static_cast<int>(minutes) * 60 * 1000);
    if (!m_sleepTicker.isActive())
        m_sleepTicker.start();
    emit sleepMinutesChanged();
    emit sleepRemainingChanged();
}

QString PlayerController::formatTime(int ms) const
{
    if (ms < 0) ms = 0;
    const int totalSec = ms / 1000;
    const int h = totalSec / 3600;
    const int m = (totalSec % 3600) / 60;
    const int s = totalSec % 60;
    if (h > 0)
        return QStringLiteral("%1:%2:%3").arg(h).arg(m, 2, 10, QLatin1Char('0')).arg(s, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1:%2").arg(m).arg(s, 2, 10, QLatin1Char('0'));
}

void PlayerController::onPositionTick()
{
    saveProgress();
}

void PlayerController::saveProgress()
{
    if (!m_store || m_episodeId <= 0) return;
    const int pos = m_player.position();
    const int dur = m_player.duration();
    m_resumePositionMs = pos;
    m_store->savePosition(m_episodeId, pos);
    if (dur > 0 && pos > dur - 5000)
        m_store->markCompleted(m_episodeId, true);
}

void PlayerController::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    if (status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::BufferedMedia)
        applyPlaybackRate();
    else if (status == QMediaPlayer::EndOfMedia) {
        saveProgress();
        if (m_store)
            m_store->markCompleted(m_episodeId, true);
        emit episodeFinished();
        playNextInQueue();
    }
}

void PlayerController::playNextInQueue()
{
    if (!m_store || m_episodeId <= 0) return;
    const int next = m_store->nextQueuedEpisode(m_episodeId);
    if (next > 0 && next != m_episodeId)
        playEpisode(next);
    else
        stop();
}

void PlayerController::onSleepTimer()
{
    m_sleepEndsAtMs = 0;
    m_sleepTicker.stop();
    m_sleepMinutes = 0;
    pause();
    emit sleepMinutesChanged();
    emit sleepRemainingChanged();
}

void PlayerController::onSleepTick()
{
    if (m_sleepEndsAtMs <= 0) {
        m_sleepTicker.stop();
        return;
    }
    emit sleepRemainingChanged();
    if (sleepRemainingSec() <= 0)
        onSleepTimer();
}

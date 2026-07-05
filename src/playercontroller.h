#ifndef PLAYERCONTROLLER_H
#define PLAYERCONTROLLER_H

#include <QObject>
#include <QMediaPlayer>
#include <QTimer>

class PodcastStore;

class PlayerController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY playbackChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playbackChanged)
    Q_PROPERTY(int episodeId READ episodeId NOTIFY playbackChanged)
    Q_PROPERTY(QString title READ title NOTIFY playbackChanged)
    Q_PROPERTY(QString feedTitle READ feedTitle NOTIFY playbackChanged)
    Q_PROPERTY(int position READ position NOTIFY positionChanged)
    Q_PROPERTY(int duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(qreal playbackRate READ playbackRate WRITE setPlaybackRate NOTIFY playbackRateChanged)
    Q_PROPERTY(int sleepMinutes READ sleepMinutes WRITE setSleepMinutes NOTIFY sleepMinutesChanged)
    Q_PROPERTY(int sleepRemainingSec READ sleepRemainingSec NOTIFY sleepRemainingChanged)

public:
    explicit PlayerController(QObject *parent = nullptr);

    bool active() const { return m_episodeId > 0; }
    bool playing() const { return m_player.state() == QMediaPlayer::PlayingState; }
    int episodeId() const { return m_episodeId; }
    QString title() const { return m_title; }
    QString feedTitle() const { return m_feedTitle; }
    int position() const { return m_player.position(); }
    int duration() const { return m_player.duration(); }
    qreal playbackRate() const { return m_playbackRate; }
    int sleepMinutes() const { return m_sleepMinutes; }
    int sleepRemainingSec() const;

    void setStore(PodcastStore *store) { m_store = store; }

    Q_INVOKABLE void playEpisode(int episodeId);
    Q_INVOKABLE void togglePlay();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void seek(int positionMs);
    Q_INVOKABLE void skipForward();
    Q_INVOKABLE void skipBackward();
    Q_INVOKABLE void setPlaybackRate(qreal rate);
    Q_INVOKABLE void setSleepMinutes(int minutes);
    Q_INVOKABLE QString formatTime(int ms) const;

public slots:
    void playNextInQueue();

signals:
    void playbackChanged();
    void positionChanged();
    void durationChanged();
    void playbackRateChanged();
    void sleepMinutesChanged();
    void sleepRemainingChanged();
    void episodeFinished();

private slots:
    void onPositionTick();
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void onSleepTimer();
    void onSleepTick();

private:
    void saveProgress();
    void loadEpisodeMetadata(int episodeId);
    void applyPlaybackRate();

    PodcastStore *m_store = nullptr;
    QMediaPlayer m_player;
    QTimer m_positionTimer;
    QTimer m_sleepTimer;
    QTimer m_sleepTicker;
    qint64 m_sleepEndsAtMs = 0;
    int m_episodeId = 0;
    int m_resumePositionMs = 0;
    QString m_title;
    QString m_feedTitle;
    qreal m_playbackRate = 1.0;
    int m_sleepMinutes = 0;
};

#endif // PLAYERCONTROLLER_H

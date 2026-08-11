#ifndef TAGIT_PLAYER_WIDGET_H
#define TAGIT_PLAYER_WIDGET_H

#include <QWidget>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QSlider>

class QPushButton;
class QLabel;

namespace tagit {

/**
 * @brief Enhanced slider supporting direct click-to-seek, mouse dragging,
 *        wheel adjustments, and hover tooltip preview with timestamps or percentages.
 */
class ClickableSlider : public QSlider {
    Q_OBJECT
public:
    enum SliderType {
        TimePosition,
        VolumeLevel
    };

    explicit ClickableSlider(Qt::Orientation orientation, SliderType type = TimePosition, QWidget *parent = nullptr);

    void setSliderType(SliderType type) { m_type = type; }
    SliderType sliderType() const { return m_type; }
    QString formatHoverText(int val) const;

signals:
    void valueHovered(int value);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    SliderType m_type = TimePosition;
};

/**
 * @brief Integrated Music Player widget supporting playback control,
 *        seeking, volume control, shuffle, repeat, and playlist events.
 */
class PlayerWidget : public QWidget {
    Q_OBJECT
public:
    enum class RepeatMode {
        Off = 0,
        All,
        One
    };
    Q_ENUM(RepeatMode)

    explicit PlayerWidget(QWidget *parent = nullptr);
    ~PlayerWidget() override;

    /// Helper for formatting milliseconds into mm:ss or hh:mm:ss.
    static QString formatTime(qint64 ms);

    /// Load and play a song.
    void playSong(const QString &filePath, const QString &title, const QString &artist);

    /// Stop playback and release the file handle.
    void stopAndRelease();

    /// Update file path if the song playing was renamed.
    void updateFilePath(const QString &oldPath, const QString &newPath);

    /// Get current file path.
    QString currentFilePath() const { return m_filePath; }

    /// Playback state & mode getters
    bool isPlaying() const;
    bool isMuted() const;
    bool isShuffleEnabled() const { return m_shuffleEnabled; }
    RepeatMode repeatMode() const { return m_repeatMode; }
    qint64 currentPosition() const;
    qint64 currentDuration() const;
    float currentVolume() const;

    void setShuffleEnabled(bool enabled);
    void setRepeatMode(RepeatMode mode);
    void setHasPrevious(bool hasPrev);
    void setHasNext(bool hasNext);

    // Control widget getters
    QPushButton     *playButton() const { return m_playButton; }
    QPushButton     *stopButton() const { return m_stopButton; }
    QPushButton     *prevButton() const { return m_prevButton; }
    QPushButton     *nextButton() const { return m_nextButton; }
    QPushButton     *repeatButton() const { return m_repeatButton; }
    QPushButton     *shuffleButton() const { return m_shuffleButton; }
    QPushButton     *muteButton() const { return m_muteButton; }
    ClickableSlider *positionSlider() const { return m_slider; }
    ClickableSlider *volumeSlider() const { return m_volumeSlider; }
    QLabel          *currentTimeLabel() const { return m_currentTimeLabel; }
    QLabel          *totalTimeLabel() const { return m_totalTimeLabel; }
    QLabel          *infoLabel() const { return m_infoLabel; }

public slots:
    void play();
    void pause();
    void togglePlayPause();
    void stop();
    void previous();
    void next();
    void seekRelative(qint64 deltaMs);
    void volumeUp(int step = 5);
    void volumeDown(int step = 5);
    void toggleMute();
    void setVolumeLevel(int volumePercent);

signals:
    void playRequested();
    void pauseRequested();
    void stopRequested();
    void previousRequested();
    void nextRequested();
    void shuffleToggled(bool enabled);
    void repeatModeChanged(tagit::PlayerWidget::RepeatMode mode);
    void songFinished();

private slots:
    void onPlayPauseClicked();
    void onStopClicked();
    void onPreviousClicked();
    void onNextClicked();
    void onShuffleClicked();
    void onRepeatClicked();
    void onRewindClicked();
    void onForwardClicked();
    void onMuteClicked();
    void onVolumeSliderChanged(int value);
    void onPositionSliderMoved(int position);
    void onPositionSliderReleased();
    
    // Player status slots
    void onPlaybackStateChanged(QMediaPlayer::PlaybackState state);
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void onPositionChanged(qint64 position);
    void onDurationChanged(qint64 duration);

private:
    void buildUi();
    void updateTimeLabel(qint64 position, qint64 duration);
    void updateVolumeUi(int value, bool isMuted);
    void updateRepeatButtonUi();
    void updateShuffleButtonUi();

    QMediaPlayer *m_player       = nullptr;
    QAudioOutput *m_audioOutput  = nullptr;

    QString       m_filePath;
    QString       m_title;
    QString       m_artist;

    bool          m_shuffleEnabled = false;
    RepeatMode    m_repeatMode     = RepeatMode::Off;

    // Controls
    QPushButton     *m_shuffleButton    = nullptr;
    QPushButton     *m_prevButton       = nullptr;
    QPushButton     *m_rewindButton     = nullptr;
    QPushButton     *m_playButton       = nullptr;
    QPushButton     *m_forwardButton    = nullptr;
    QPushButton     *m_nextButton       = nullptr;
    QPushButton     *m_stopButton       = nullptr;
    QPushButton     *m_repeatButton     = nullptr;

    // Time & Progress
    QLabel          *m_currentTimeLabel = nullptr;
    ClickableSlider *m_slider           = nullptr;
    QLabel          *m_totalTimeLabel   = nullptr;
    QLabel          *m_infoLabel        = nullptr;

    // Volume
    QPushButton     *m_volDownButton    = nullptr;
    QPushButton     *m_muteButton       = nullptr;
    ClickableSlider *m_volumeSlider     = nullptr;
    QPushButton     *m_volUpButton      = nullptr;
    QLabel          *m_volumeLabel      = nullptr;

    bool             m_sliderBeingDragged = false;
    int              m_lastUnmutedVolume  = 70;
};

} // namespace tagit

#endif // TAGIT_PLAYER_WIDGET_H

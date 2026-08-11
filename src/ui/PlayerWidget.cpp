#include "PlayerWidget.h"
#include "../core/Logger.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFileInfo>
#include <QUrl>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QToolTip>
#include <algorithm>

namespace tagit {

// ---------------------------------------------------------------------------
// ClickableSlider Implementation
// ---------------------------------------------------------------------------

ClickableSlider::ClickableSlider(Qt::Orientation orientation, SliderType type, QWidget *parent)
    : QSlider(orientation, parent)
    , m_type(type)
{
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
}

void ClickableSlider::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        int val = minimum();
        if (orientation() == Qt::Horizontal) {
            const int w = width();
            if (w > 0) {
                const double ratio = static_cast<double>(event->position().x()) / static_cast<double>(w);
                val = minimum() + static_cast<int>(ratio * (maximum() - minimum()));
            }
        } else {
            const int h = height();
            if (h > 0) {
                const double ratio = 1.0 - (static_cast<double>(event->position().y()) / static_cast<double>(h));
                val = minimum() + static_cast<int>(ratio * (maximum() - minimum()));
            }
        }
        val = std::clamp(val, minimum(), maximum());
        setValue(val);
        emit sliderMoved(val);
        emit sliderReleased();
        event->accept();
        return;
    }
    QSlider::mousePressEvent(event);
}

void ClickableSlider::mouseMoveEvent(QMouseEvent *event)
{
    if (maximum() > minimum()) {
        int hoverVal = minimum();
        if (orientation() == Qt::Horizontal) {
            const int w = width();
            if (w > 0) {
                const double ratio = static_cast<double>(event->position().x()) / static_cast<double>(w);
                hoverVal = minimum() + static_cast<int>(ratio * (maximum() - minimum()));
            }
        } else {
            const int h = height();
            if (h > 0) {
                const double ratio = 1.0 - (static_cast<double>(event->position().y()) / static_cast<double>(h));
                hoverVal = minimum() + static_cast<int>(ratio * (maximum() - minimum()));
            }
        }
        hoverVal = std::clamp(hoverVal, minimum(), maximum());
        emit valueHovered(hoverVal);
        QToolTip::showText(event->globalPosition().toPoint(), formatHoverText(hoverVal), this);
    }
    QSlider::mouseMoveEvent(event);
}

void ClickableSlider::wheelEvent(QWheelEvent *event)
{
    const int numDegrees = event->angleDelta().y() / 8;
    const int numSteps = numDegrees / 15;
    if (numSteps != 0) {
        const int delta = (m_type == VolumeLevel) ? (numSteps * 2) : (numSteps * 5000);
        const int newVal = std::clamp(value() + delta, minimum(), maximum());
        setValue(newVal);
        emit sliderMoved(newVal);
        emit sliderReleased();
        event->accept();
        return;
    }
    QSlider::wheelEvent(event);
}

QString ClickableSlider::formatHoverText(int val) const
{
    if (m_type == VolumeLevel) {
        return QString("%1%").arg(val);
    }
    return PlayerWidget::formatTime(static_cast<qint64>(val));
}

// ---------------------------------------------------------------------------
// PlayerWidget Implementation
// ---------------------------------------------------------------------------

PlayerWidget::PlayerWidget(QWidget *parent)
    : QWidget(parent)
{
    m_player = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_player->setAudioOutput(m_audioOutput);

    // Initial audio settings
    m_audioOutput->setVolume(0.70f);

    buildUi();
    updateVolumeUi(70, false);
    updateRepeatButtonUi();
    updateShuffleButtonUi();

    // Connect player signals
    connect(m_player, &QMediaPlayer::playbackStateChanged, this, &PlayerWidget::onPlaybackStateChanged);
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, &PlayerWidget::onMediaStatusChanged);
    connect(m_player, &QMediaPlayer::positionChanged, this, &PlayerWidget::onPositionChanged);
    connect(m_player, &QMediaPlayer::durationChanged, this, &PlayerWidget::onDurationChanged);

    // Connect UI control signals
    connect(m_playButton, &QPushButton::clicked, this, &PlayerWidget::onPlayPauseClicked);
    connect(m_stopButton, &QPushButton::clicked, this, &PlayerWidget::onStopClicked);
    connect(m_prevButton, &QPushButton::clicked, this, &PlayerWidget::onPreviousClicked);
    connect(m_nextButton, &QPushButton::clicked, this, &PlayerWidget::onNextClicked);
    connect(m_shuffleButton, &QPushButton::clicked, this, &PlayerWidget::onShuffleClicked);
    connect(m_repeatButton, &QPushButton::clicked, this, &PlayerWidget::onRepeatClicked);
    connect(m_rewindButton, &QPushButton::clicked, this, &PlayerWidget::onRewindClicked);
    connect(m_forwardButton, &QPushButton::clicked, this, &PlayerWidget::onForwardClicked);

    connect(m_volDownButton, &QPushButton::clicked, this, [this]() { volumeDown(5); });
    connect(m_volUpButton, &QPushButton::clicked, this, [this]() { volumeUp(5); });
    connect(m_muteButton, &QPushButton::clicked, this, &PlayerWidget::onMuteClicked);
    connect(m_volumeSlider, &QSlider::valueChanged, this, &PlayerWidget::onVolumeSliderChanged);

    connect(m_slider, &QSlider::sliderPressed, this, [this]() { m_sliderBeingDragged = true; });
    connect(m_slider, &QSlider::sliderReleased, this, &PlayerWidget::onPositionSliderReleased);
    connect(m_slider, &QSlider::sliderMoved, this, &PlayerWidget::onPositionSliderMoved);
}

PlayerWidget::~PlayerWidget()
{
    stopAndRelease();
}

void PlayerWidget::playSong(const QString &filePath, const QString &title, const QString &artist)
{
    m_filePath = filePath;
    m_title = title;
    m_artist = artist;

    m_player->stop();
    m_player->setSource(QUrl::fromLocalFile(filePath));

    if (!title.isEmpty()) {
        m_infoLabel->setText(QString("<b>%1</b><br><span style='color: #94a3b8; font-size: 11px;'>%2</span>")
                             .arg(title)
                             .arg(artist.isEmpty() ? tr("Unknown Artist") : artist));
    } else {
        m_infoLabel->setText(QString("<b>%1</b>")
                             .arg(QFileInfo(filePath).fileName()));
    }

    m_player->play();
    Logger::info("Playing: " + filePath);
}

void PlayerWidget::stopAndRelease()
{
    if (m_player) {
        m_player->stop();
        m_player->setSource(QUrl());
    }
    m_filePath.clear();
    m_title.clear();
    m_artist.clear();
    m_infoLabel->setText(tr("No song playing"));
    m_slider->setValue(0);
    m_slider->setRange(0, 0);
    updateTimeLabel(0, 0);
}

void PlayerWidget::updateFilePath(const QString &oldPath, const QString &newPath)
{
    if (m_filePath == oldPath) {
        m_filePath = newPath;
        const auto state = m_player->playbackState();
        const auto pos = m_player->position();
        
        m_player->stop();
        m_player->setSource(QUrl::fromLocalFile(newPath));
        
        if (state == QMediaPlayer::PlayingState) {
            m_player->setPosition(pos);
            m_player->play();
        } else if (state == QMediaPlayer::PausedState) {
            m_player->setPosition(pos);
            m_player->pause();
        }
    }
}

bool PlayerWidget::isPlaying() const
{
    return m_player && m_player->playbackState() == QMediaPlayer::PlayingState;
}

qint64 PlayerWidget::currentPosition() const
{
    return m_player ? m_player->position() : 0;
}

qint64 PlayerWidget::currentDuration() const
{
    return m_player ? m_player->duration() : 0;
}

bool PlayerWidget::isMuted() const
{
    return m_audioOutput ? m_audioOutput->isMuted() : false;
}

float PlayerWidget::currentVolume() const
{
    return m_audioOutput ? m_audioOutput->volume() : 0.0f;
}

void PlayerWidget::setShuffleEnabled(bool enabled)
{
    if (m_shuffleEnabled != enabled) {
        m_shuffleEnabled = enabled;
        updateShuffleButtonUi();
        emit shuffleToggled(m_shuffleEnabled);
    }
}

void PlayerWidget::setRepeatMode(RepeatMode mode)
{
    if (m_repeatMode != mode) {
        m_repeatMode = mode;
        updateRepeatButtonUi();
        emit repeatModeChanged(m_repeatMode);
    }
}

void PlayerWidget::setHasPrevious(bool hasPrev)
{
    if (m_prevButton) {
        m_prevButton->setEnabled(hasPrev);
    }
}

void PlayerWidget::setHasNext(bool hasNext)
{
    if (m_nextButton) {
        m_nextButton->setEnabled(hasNext);
    }
}

void PlayerWidget::play()
{
    if (m_player) m_player->play();
}

void PlayerWidget::pause()
{
    if (m_player) m_player->pause();
}

void PlayerWidget::togglePlayPause()
{
    onPlayPauseClicked();
}

void PlayerWidget::stop()
{
    if (m_player) {
        m_player->stop();
        m_player->setPosition(0);
    }
    if (m_slider) {
        m_slider->setValue(0);
    }
    updateTimeLabel(0, m_player ? m_player->duration() : 0);
    if (m_playButton) {
        m_playButton->setText("▶");
        m_playButton->setToolTip(tr("Play (Space)"));
    }
}

void PlayerWidget::previous()
{
    onPreviousClicked();
}

void PlayerWidget::next()
{
    onNextClicked();
}

void PlayerWidget::seekRelative(qint64 deltaMs)
{
    if (!m_player || m_player->duration() <= 0) return;
    const qint64 newPos = std::clamp<qint64>(m_player->position() + deltaMs, 0, m_player->duration());
    m_player->setPosition(newPos);
    m_slider->setValue(static_cast<int>(newPos));
    updateTimeLabel(newPos, m_player->duration());
}

void PlayerWidget::volumeUp(int step)
{
    const int cur = m_volumeSlider ? m_volumeSlider->value() : 70;
    const int nextVal = std::clamp(cur + step, 0, 100);
    if (m_volumeSlider) m_volumeSlider->setValue(nextVal);
}

void PlayerWidget::volumeDown(int step)
{
    const int cur = m_volumeSlider ? m_volumeSlider->value() : 70;
    const int nextVal = std::clamp(cur - step, 0, 100);
    if (m_volumeSlider) m_volumeSlider->setValue(nextVal);
}

void PlayerWidget::toggleMute()
{
    onMuteClicked();
}

void PlayerWidget::setVolumeLevel(int volumePercent)
{
    if (m_volumeSlider) {
        m_volumeSlider->setValue(std::clamp(volumePercent, 0, 100));
    }
}

void PlayerWidget::onPlayPauseClicked()
{
    if (m_filePath.isEmpty()) {
        emit playRequested();
        return;
    }

    if (m_player->playbackState() == QMediaPlayer::PlayingState) {
        m_player->pause();
        emit pauseRequested();
    } else {
        m_player->play();
        emit playRequested();
    }
}

void PlayerWidget::onStopClicked()
{
    stop();
    emit stopRequested();
}

void PlayerWidget::onPreviousClicked()
{
    // If more than 3 seconds in, restart current track; otherwise ask for previous
    if (m_player && m_player->position() > 3000) {
        m_player->setPosition(0);
        m_slider->setValue(0);
        updateTimeLabel(0, m_player->duration());
    } else {
        emit previousRequested();
    }
}

void PlayerWidget::onNextClicked()
{
    emit nextRequested();
}

void PlayerWidget::onShuffleClicked()
{
    m_shuffleEnabled = !m_shuffleEnabled;
    updateShuffleButtonUi();
    emit shuffleToggled(m_shuffleEnabled);
}

void PlayerWidget::onRepeatClicked()
{
    switch (m_repeatMode) {
    case RepeatMode::Off:
        m_repeatMode = RepeatMode::All;
        break;
    case RepeatMode::All:
        m_repeatMode = RepeatMode::One;
        break;
    case RepeatMode::One:
    default:
        m_repeatMode = RepeatMode::Off;
        break;
    }
    updateRepeatButtonUi();
    emit repeatModeChanged(m_repeatMode);
}

void PlayerWidget::onRewindClicked()
{
    seekRelative(-5000);
}

void PlayerWidget::onForwardClicked()
{
    seekRelative(5000);
}

void PlayerWidget::onMuteClicked()
{
    const bool currentlyMuted = m_audioOutput->isMuted();
    const bool newMuted = !currentlyMuted;
    
    if (newMuted) {
        m_lastUnmutedVolume = m_volumeSlider->value();
        m_audioOutput->setMuted(true);
    } else {
        m_audioOutput->setMuted(false);
        if (m_volumeSlider->value() == 0 && m_lastUnmutedVolume > 0) {
            m_volumeSlider->setValue(m_lastUnmutedVolume);
        }
    }
    updateVolumeUi(m_volumeSlider->value(), newMuted);
}

void PlayerWidget::onVolumeSliderChanged(int value)
{
    const float volFactor = static_cast<float>(value) / 100.0f;
    m_audioOutput->setVolume(volFactor);
    
    if (m_audioOutput->isMuted() && value > 0) {
        m_audioOutput->setMuted(false);
    }
    if (value > 0) {
        m_lastUnmutedVolume = value;
    }
    updateVolumeUi(value, m_audioOutput->isMuted());
}

void PlayerWidget::onPositionSliderMoved(int position)
{
    updateTimeLabel(position, m_player->duration());
}

void PlayerWidget::onPositionSliderReleased()
{
    m_player->setPosition(m_slider->value());
    m_sliderBeingDragged = false;
}

void PlayerWidget::onPlaybackStateChanged(QMediaPlayer::PlaybackState state)
{
    if (state == QMediaPlayer::PlayingState) {
        m_playButton->setText("⏸");
        m_playButton->setToolTip(tr("Pause (Space)"));
    } else {
        m_playButton->setText("▶");
        m_playButton->setToolTip(tr("Play (Space)"));
    }
}

void PlayerWidget::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    if (status == QMediaPlayer::EndOfMedia) {
        if (m_repeatMode == RepeatMode::One) {
            m_player->setPosition(0);
            m_player->play();
        } else {
            emit songFinished();
            emit nextRequested();
        }
    }
}

void PlayerWidget::onPositionChanged(qint64 position)
{
    if (!m_sliderBeingDragged) {
        m_slider->setValue(static_cast<int>(position));
        updateTimeLabel(position, m_player->duration());
    }
}

void PlayerWidget::onDurationChanged(qint64 duration)
{
    m_slider->setRange(0, static_cast<int>(duration));
    updateTimeLabel(m_player->position(), duration);
}

void PlayerWidget::updateTimeLabel(qint64 position, qint64 duration)
{
    if (m_currentTimeLabel) {
        m_currentTimeLabel->setText(formatTime(position));
    }
    if (m_totalTimeLabel) {
        m_totalTimeLabel->setText(formatTime(duration));
    }
}

void PlayerWidget::updateVolumeUi(int value, bool isMuted)
{
    if (isMuted || value == 0) {
        m_muteButton->setText("🔇");
        m_muteButton->setToolTip(tr("Unmute (Ctrl+M)"));
        m_volumeLabel->setText(tr("Muted"));
    } else if (value < 34) {
        m_muteButton->setText("🔈");
        m_muteButton->setToolTip(tr("Mute (Ctrl+M)"));
        m_volumeLabel->setText(QString("%1%").arg(value));
    } else if (value < 67) {
        m_muteButton->setText("🔉");
        m_muteButton->setToolTip(tr("Mute (Ctrl+M)"));
        m_volumeLabel->setText(QString("%1%").arg(value));
    } else {
        m_muteButton->setText("🔊");
        m_muteButton->setToolTip(tr("Mute (Ctrl+M)"));
        m_volumeLabel->setText(QString("%1%").arg(value));
    }
}

void PlayerWidget::updateRepeatButtonUi()
{
    switch (m_repeatMode) {
    case RepeatMode::Off:
        m_repeatButton->setText("🔁");
        m_repeatButton->setToolTip(tr("Repeat: Off (Click to repeat all)"));
        m_repeatButton->setStyleSheet(
            "QPushButton { background: transparent; color: #64748b; border: none; border-radius: 14px; width: 28px; height: 28px; font-size: 13px; }"
            "QPushButton:hover { background: #1e293b; color: #94a3b8; }");
        break;
    case RepeatMode::All:
        m_repeatButton->setText("🔁");
        m_repeatButton->setToolTip(tr("Repeat: All Tracks (Click to repeat one)"));
        m_repeatButton->setStyleSheet(
            "QPushButton { background: #1e3a8a; color: #60a5fa; border: 1px solid #3b82f6; border-radius: 14px; width: 28px; height: 28px; font-size: 13px; font-weight: bold; }"
            "QPushButton:hover { background: #2563eb; color: white; }");
        break;
    case RepeatMode::One:
        m_repeatButton->setText("🔂");
        m_repeatButton->setToolTip(tr("Repeat: Current Track (Click to disable)"));
        m_repeatButton->setStyleSheet(
            "QPushButton { background: #3b82f6; color: white; border: none; border-radius: 14px; width: 28px; height: 28px; font-size: 13px; font-weight: bold; }"
            "QPushButton:hover { background: #60a5fa; }");
        break;
    }
}

void PlayerWidget::updateShuffleButtonUi()
{
    if (m_shuffleEnabled) {
        m_shuffleButton->setToolTip(tr("Shuffle: ON (Click to disable)"));
        m_shuffleButton->setStyleSheet(
            "QPushButton { background: #1e3a8a; color: #60a5fa; border: 1px solid #3b82f6; border-radius: 14px; width: 28px; height: 28px; font-size: 13px; font-weight: bold; }"
            "QPushButton:hover { background: #2563eb; color: white; }");
    } else {
        m_shuffleButton->setToolTip(tr("Shuffle: OFF (Click to enable)"));
        m_shuffleButton->setStyleSheet(
            "QPushButton { background: transparent; color: #64748b; border: none; border-radius: 14px; width: 28px; height: 28px; font-size: 13px; }"
            "QPushButton:hover { background: #1e293b; color: #94a3b8; }");
    }
}

QString PlayerWidget::formatTime(qint64 ms)
{
    if (ms <= 0) {
        return QStringLiteral("0:00");
    }
    const qint64 totalSeconds = ms / 1000;
    const qint64 hours = totalSeconds / 3600;
    const qint64 minutes = (totalSeconds % 3600) / 60;
    const qint64 seconds = totalSeconds % 60;
    if (hours > 0) {
        return QString("%1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
    }
    return QString("%1:%2")
        .arg(minutes)
        .arg(seconds, 2, 10, QChar('0'));
}

void PlayerWidget::buildUi()
{
    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(14, 6, 14, 6);
    mainLayout->setSpacing(16);

    // ---- Left: Info Widget ----
    auto *infoLayout = new QVBoxLayout();
    infoLayout->setSpacing(2);
    infoLayout->setContentsMargins(0, 0, 0, 0);
    m_infoLabel = new QLabel(tr("No song playing"), this);
    m_infoLabel->setStyleSheet("font-weight: bold; color: #f8fafc; font-size: 12px; line-height: 1.2;");
    m_infoLabel->setWordWrap(true);
    infoLayout->addWidget(m_infoLabel);
    infoLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    
    auto *infoWidget = new QWidget(this);
    infoWidget->setLayout(infoLayout);
    infoWidget->setFixedWidth(210);
    mainLayout->addWidget(infoWidget);

    // ---- Center: Playback Controls & Progress ----
    auto *centerLayout = new QVBoxLayout();
    centerLayout->setSpacing(3);
    centerLayout->setContentsMargins(0, 0, 0, 0);

    // Top: Buttons Layout
    auto *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(8);
    btnLayout->addStretch();

    const QString roundBtnStyle =
        "QPushButton { background: #1e293b; color: #cbd5e1; border: 1px solid #334155; border-radius: 14px; width: 28px; height: 28px; font-size: 12px; }"
        "QPushButton:hover { background: #334155; color: #f8fafc; border-color: #475569; }"
        "QPushButton:pressed { background: #0f172a; }"
        "QPushButton:disabled { color: #475569; border-color: #1e293b; background: transparent; }";

    // Shuffle Button
    m_shuffleButton = new QPushButton("🔀", this);
    m_shuffleButton->setCursor(Qt::PointingHandCursor);
    btnLayout->addWidget(m_shuffleButton);

    // Previous Button
    m_prevButton = new QPushButton("⏮", this);
    m_prevButton->setCursor(Qt::PointingHandCursor);
    m_prevButton->setToolTip(tr("Previous Track (Ctrl+Left)"));
    m_prevButton->setStyleSheet(roundBtnStyle);
    btnLayout->addWidget(m_prevButton);

    // Rewind 5s Button
    m_rewindButton = new QPushButton("⏪", this);
    m_rewindButton->setCursor(Qt::PointingHandCursor);
    m_rewindButton->setToolTip(tr("Rewind 5s (Shift+Left)"));
    m_rewindButton->setStyleSheet(
        "QPushButton { background: transparent; color: #94a3b8; border: none; border-radius: 12px; width: 24px; height: 24px; font-size: 11px; }"
        "QPushButton:hover { background: #1e293b; color: #f8fafc; }");
    btnLayout->addWidget(m_rewindButton);

    // Play/Pause Button (Prominent Center)
    m_playButton = new QPushButton("▶", this);
    m_playButton->setCursor(Qt::PointingHandCursor);
    m_playButton->setToolTip(tr("Play / Pause (Space)"));
    m_playButton->setStyleSheet(
        "QPushButton { background: #3b82f6; color: white; border: none; border-radius: 16px; width: 32px; height: 32px; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background: #60a5fa; }"
        "QPushButton:pressed { background: #2563eb; }");
    btnLayout->addWidget(m_playButton);

    // Fast Forward 5s Button
    m_forwardButton = new QPushButton("⏩", this);
    m_forwardButton->setCursor(Qt::PointingHandCursor);
    m_forwardButton->setToolTip(tr("Forward 5s (Shift+Right)"));
    m_forwardButton->setStyleSheet(
        "QPushButton { background: transparent; color: #94a3b8; border: none; border-radius: 12px; width: 24px; height: 24px; font-size: 11px; }"
        "QPushButton:hover { background: #1e293b; color: #f8fafc; }");
    btnLayout->addWidget(m_forwardButton);

    // Next Button
    m_nextButton = new QPushButton("⏭", this);
    m_nextButton->setCursor(Qt::PointingHandCursor);
    m_nextButton->setToolTip(tr("Next Track (Ctrl+Right)"));
    m_nextButton->setStyleSheet(roundBtnStyle);
    btnLayout->addWidget(m_nextButton);

    // Stop Button
    m_stopButton = new QPushButton("⏹", this);
    m_stopButton->setCursor(Qt::PointingHandCursor);
    m_stopButton->setToolTip(tr("Stop Playback"));
    m_stopButton->setStyleSheet(
        "QPushButton { background: transparent; color: #94a3b8; border: none; border-radius: 14px; width: 28px; height: 28px; font-size: 12px; }"
        "QPushButton:hover { background: #1e293b; color: #ef4444; }");
    btnLayout->addWidget(m_stopButton);

    // Repeat Button
    m_repeatButton = new QPushButton("🔁", this);
    m_repeatButton->setCursor(Qt::PointingHandCursor);
    btnLayout->addWidget(m_repeatButton);

    btnLayout->addStretch();
    centerLayout->addLayout(btnLayout);

    // Bottom: Seek Slider & Timestamps
    auto *sliderLayout = new QHBoxLayout();
    sliderLayout->setSpacing(8);
    sliderLayout->setContentsMargins(0, 0, 0, 0);

    m_currentTimeLabel = new QLabel("0:00", this);
    m_currentTimeLabel->setStyleSheet("color: #94a3b8; font-family: monospace; font-size: 11px; min-width: 32px; text-align: right;");
    m_currentTimeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    
    m_slider = new ClickableSlider(Qt::Horizontal, ClickableSlider::TimePosition, this);
    m_slider->setRange(0, 0);
    m_slider->setStyleSheet(
        "QSlider::groove:horizontal { height: 4px; background: #334155; border-radius: 2px; }"
        "QSlider::sub-page:horizontal { background: #3b82f6; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: #f8fafc; width: 10px; height: 10px; margin-top: -3px; margin-bottom: -3px; border-radius: 5px; }"
        "QSlider::handle:horizontal:hover { background: #60a5fa; width: 12px; height: 12px; margin-top: -4px; margin-bottom: -4px; border-radius: 6px; }");

    m_totalTimeLabel = new QLabel("0:00", this);
    m_totalTimeLabel->setStyleSheet("color: #64748b; font-family: monospace; font-size: 11px; min-width: 32px;");
    m_totalTimeLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    sliderLayout->addWidget(m_currentTimeLabel);
    sliderLayout->addWidget(m_slider, 1);
    sliderLayout->addWidget(m_totalTimeLabel);
    centerLayout->addLayout(sliderLayout);
    
    mainLayout->addLayout(centerLayout, 1);

    // ---- Right: Volume Controls ----
    auto *volLayout = new QHBoxLayout();
    volLayout->setSpacing(4);
    volLayout->setContentsMargins(0, 0, 0, 0);

    m_volDownButton = new QPushButton("−", this);
    m_volDownButton->setCursor(Qt::PointingHandCursor);
    m_volDownButton->setToolTip(tr("Volume Down 5% (Ctrl+Down)"));
    m_volDownButton->setStyleSheet(
        "QPushButton { background: transparent; color: #94a3b8; border: none; width: 16px; height: 22px; font-size: 14px; font-weight: bold; }"
        "QPushButton:hover { color: #f8fafc; }");

    m_muteButton = new QPushButton("🔊", this);
    m_muteButton->setCursor(Qt::PointingHandCursor);
    m_muteButton->setStyleSheet(
        "QPushButton { background: transparent; color: #94a3b8; border: none; width: 24px; height: 24px; font-size: 14px; }"
        "QPushButton:hover { color: #f8fafc; }");

    m_volumeSlider = new ClickableSlider(Qt::Horizontal, ClickableSlider::VolumeLevel, this);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(70);
    m_volumeSlider->setFixedWidth(75);
    m_volumeSlider->setStyleSheet(
        "QSlider::groove:horizontal { height: 4px; background: #334155; border-radius: 2px; }"
        "QSlider::sub-page:horizontal { background: #38bdf8; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: #f8fafc; width: 8px; height: 8px; margin-top: -2px; margin-bottom: -2px; border-radius: 4px; }"
        "QSlider::handle:horizontal:hover { background: #38bdf8; width: 10px; height: 10px; margin-top: -3px; margin-bottom: -3px; border-radius: 5px; }");

    m_volUpButton = new QPushButton("+", this);
    m_volUpButton->setCursor(Qt::PointingHandCursor);
    m_volUpButton->setToolTip(tr("Volume Up 5% (Ctrl+Up)"));
    m_volUpButton->setStyleSheet(
        "QPushButton { background: transparent; color: #94a3b8; border: none; width: 16px; height: 22px; font-size: 14px; font-weight: bold; }"
        "QPushButton:hover { color: #f8fafc; }");

    m_volumeLabel = new QLabel("70%", this);
    m_volumeLabel->setStyleSheet("color: #94a3b8; font-family: monospace; font-size: 11px; min-width: 28px; text-align: left;");
    m_volumeLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    volLayout->addWidget(m_volDownButton);
    volLayout->addWidget(m_muteButton);
    volLayout->addWidget(m_volumeSlider);
    volLayout->addWidget(m_volUpButton);
    volLayout->addWidget(m_volumeLabel);

    auto *volWidget = new QWidget(this);
    volWidget->setLayout(volLayout);
    mainLayout->addWidget(volWidget);

    // Style the main player widget container
    setObjectName("PlayerWidget");
    setStyleSheet("#PlayerWidget { background: #0b1120; border-top: 1px solid #1e293b; }");
    setFixedHeight(66);
}

} // namespace tagit

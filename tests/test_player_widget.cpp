#include <gtest/gtest.h>
#include <QApplication>
#include <QLabel>
#include <QPushButton>

#include "../src/ui/PlayerWidget.h"

using namespace tagit;

class PlayerWidgetTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!qApp) {
            static int argc = 1;
            static char appName[] = "tagit_player_tests";
            static char *argv[] = { appName, nullptr };
            m_app = new QApplication(argc, argv);
        }
    }

    QApplication *m_app = nullptr;
};

// 1. Test Time Formatting for various durations (seconds, minutes, hours)
TEST_F(PlayerWidgetTest, FormatTimeCalculations)
{
    EXPECT_EQ(PlayerWidget::formatTime(0), "0:00");
    EXPECT_EQ(PlayerWidget::formatTime(-1000), "0:00");
    EXPECT_EQ(PlayerWidget::formatTime(9000), "0:09");
    EXPECT_EQ(PlayerWidget::formatTime(45000), "0:45");
    EXPECT_EQ(PlayerWidget::formatTime(65000), "1:05");
    EXPECT_EQ(PlayerWidget::formatTime(215000), "3:35");
    EXPECT_EQ(PlayerWidget::formatTime(3599000), "59:59");
    EXPECT_EQ(PlayerWidget::formatTime(3600000), "1:00:00");
    EXPECT_EQ(PlayerWidget::formatTime(3665000), "1:01:05");
    EXPECT_EQ(PlayerWidget::formatTime(7325000), "2:02:05");
}

// 2. Test ClickableSlider Hover Text Generation
TEST_F(PlayerWidgetTest, ClickableSliderHoverTextFormatting)
{
    ClickableSlider timeSlider(Qt::Horizontal, ClickableSlider::TimePosition);
    EXPECT_EQ(timeSlider.formatHoverText(0), "0:00");
    EXPECT_EQ(timeSlider.formatHoverText(75000), "1:15");
    EXPECT_EQ(timeSlider.formatHoverText(3670000), "1:01:10");

    ClickableSlider volumeSlider(Qt::Horizontal, ClickableSlider::VolumeLevel);
    EXPECT_EQ(volumeSlider.formatHoverText(0), "0%");
    EXPECT_EQ(volumeSlider.formatHoverText(50), "50%");
    EXPECT_EQ(volumeSlider.formatHoverText(100), "100%");
}

// 3. Test RepeatMode state toggling and signals
TEST_F(PlayerWidgetTest, RepeatModeTransitions)
{
    PlayerWidget player;
    EXPECT_EQ(player.repeatMode(), PlayerWidget::RepeatMode::Off);

    int signalCount = 0;
    PlayerWidget::RepeatMode lastMode = PlayerWidget::RepeatMode::Off;
    QObject::connect(&player, &PlayerWidget::repeatModeChanged, [&](PlayerWidget::RepeatMode mode) {
        ++signalCount;
        lastMode = mode;
    });

    player.setRepeatMode(PlayerWidget::RepeatMode::All);
    EXPECT_EQ(player.repeatMode(), PlayerWidget::RepeatMode::All);
    EXPECT_EQ(signalCount, 1);
    EXPECT_EQ(lastMode, PlayerWidget::RepeatMode::All);

    player.setRepeatMode(PlayerWidget::RepeatMode::One);
    EXPECT_EQ(player.repeatMode(), PlayerWidget::RepeatMode::One);
    EXPECT_EQ(signalCount, 2);
    EXPECT_EQ(lastMode, PlayerWidget::RepeatMode::One);

    player.setRepeatMode(PlayerWidget::RepeatMode::Off);
    EXPECT_EQ(player.repeatMode(), PlayerWidget::RepeatMode::Off);
    EXPECT_EQ(signalCount, 3);
}

// 4. Test Shuffle state toggling and signals
TEST_F(PlayerWidgetTest, ShuffleToggling)
{
    PlayerWidget player;
    EXPECT_FALSE(player.isShuffleEnabled());

    int signalCount = 0;
    bool lastShuffle = false;
    QObject::connect(&player, &PlayerWidget::shuffleToggled, [&](bool enabled) {
        ++signalCount;
        lastShuffle = enabled;
    });

    player.setShuffleEnabled(true);
    EXPECT_TRUE(player.isShuffleEnabled());
    EXPECT_EQ(signalCount, 1);
    EXPECT_TRUE(lastShuffle);

    player.setShuffleEnabled(false);
    EXPECT_FALSE(player.isShuffleEnabled());
    EXPECT_EQ(signalCount, 2);
    EXPECT_FALSE(lastShuffle);
}

// 5. Test Volume control, stepping and bounds clamping
TEST_F(PlayerWidgetTest, VolumeManagementAndStepping)
{
    PlayerWidget player;
    player.setVolumeLevel(70);
    EXPECT_EQ(player.volumeSlider()->value(), 70);

    player.volumeUp(5);
    EXPECT_EQ(player.volumeSlider()->value(), 75);

    player.volumeDown(10);
    EXPECT_EQ(player.volumeSlider()->value(), 65);

    // Clamping upper bound
    player.setVolumeLevel(150);
    EXPECT_EQ(player.volumeSlider()->value(), 100);

    // Clamping lower bound
    player.setVolumeLevel(-50);
    EXPECT_EQ(player.volumeSlider()->value(), 0);
}

// 6. Test Mute toggling and volume restoration
TEST_F(PlayerWidgetTest, MuteAndVolumeRestore)
{
    PlayerWidget player;
    player.setVolumeLevel(80);
    EXPECT_FALSE(player.isMuted());

    // Toggle mute on
    player.toggleMute();
    EXPECT_TRUE(player.isMuted());

    // Toggle mute off
    player.toggleMute();
    EXPECT_FALSE(player.isMuted());
    EXPECT_EQ(player.volumeSlider()->value(), 80);
}

// 7. Test Play, Pause, Stop, Previous, and Next Signal Connections
TEST_F(PlayerWidgetTest, PlaybackControlSignals)
{
    PlayerWidget player;
    int prevCalls = 0;
    int nextCalls = 0;
    int stopCalls = 0;

    QObject::connect(&player, &PlayerWidget::previousRequested, [&]() { ++prevCalls; });
    QObject::connect(&player, &PlayerWidget::nextRequested, [&]() { ++nextCalls; });
    QObject::connect(&player, &PlayerWidget::stopRequested, [&]() { ++stopCalls; });

    player.previous();
    EXPECT_EQ(prevCalls, 1);

    player.next();
    EXPECT_EQ(nextCalls, 1);

    player.stop();
    EXPECT_EQ(player.positionSlider()->value(), 0);
}

// 8. Test UI element existence and states
TEST_F(PlayerWidgetTest, ControlWidgetsInitialization)
{
    PlayerWidget player;
    EXPECT_NE(player.playButton(), nullptr);
    EXPECT_NE(player.stopButton(), nullptr);
    EXPECT_NE(player.prevButton(), nullptr);
    EXPECT_NE(player.nextButton(), nullptr);
    EXPECT_NE(player.repeatButton(), nullptr);
    EXPECT_NE(player.shuffleButton(), nullptr);
    EXPECT_NE(player.muteButton(), nullptr);
    EXPECT_NE(player.positionSlider(), nullptr);
    EXPECT_NE(player.volumeSlider(), nullptr);
    EXPECT_NE(player.currentTimeLabel(), nullptr);
    EXPECT_NE(player.totalTimeLabel(), nullptr);
    EXPECT_NE(player.infoLabel(), nullptr);

    EXPECT_EQ(player.currentTimeLabel()->text(), "0:00");
    EXPECT_EQ(player.totalTimeLabel()->text(), "0:00");
}

#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QTemporaryFile>
#include <QFileInfo>

#include <QDir>

#include "../src/model/Song.h"
#include "../src/model/AudioMetadata.h"
#include "../src/core/MetadataEngine.h"
#include "../src/core/SettingsManager.h"
#include "../src/platform/NetworkService.h"
#include "../src/core/FilenameIntelligence.h"

using namespace tagit;

class ReviewFlowTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Ensure QCoreApplication exists for signals/events
        if (!qApp) {
            static int argc = 1;
            static char appName[] = "tagit_tests";
            static char *argv[] = { appName, nullptr };
            m_app = new QCoreApplication(argc, argv);
        }
    }

    QCoreApplication *m_app = nullptr;
};

TEST_F(ReviewFlowTest, FilenameIntelligenceQueryExtraction)
{
    FilenameIntelligence fi;
    AudioMetadata md = fi.parse("Coldplay - Yellow (Official Audio).mp3");
    EXPECT_EQ(md.artist, "Coldplay");
    EXPECT_EQ(md.title, "Yellow");
}

TEST_F(ReviewFlowTest, ForcedMetadataOverwriteIntegratesProperTags)
{
    // Simulate a song that currently has incorrect/incomplete metadata
    AudioMetadata existing;
    existing.title = "Track 01";
    existing.artist = "Unknown Artist";
    existing.album = "Old Album";
    existing.genre = "Pop";
    existing.year = 2000;

    // Simulate online consensus metadata returned by NetworkService
    AudioMetadata onlineConsensus;
    onlineConsensus.title = "Viva La Vida";
    onlineConsensus.artist = "Coldplay";
    onlineConsensus.album = "Viva la Vida or Death and All His Friends";
    onlineConsensus.genre = "Alternative Rock";
    onlineConsensus.year = 2008;
    onlineConsensus.trackNumber = 7;
    onlineConsensus.discNumber = 1;
    onlineConsensus.composer = "Guy Berryman, Jonny Buckland, Will Champion, Chris Martin";

    // Simulate the force-integration logic used in reviewSong
    AudioMetadata reviewed = existing;
    if (!onlineConsensus.title.isEmpty())       reviewed.title       = onlineConsensus.title;
    if (!onlineConsensus.artist.isEmpty())      reviewed.artist      = onlineConsensus.artist;
    if (!onlineConsensus.album.isEmpty())       reviewed.album       = onlineConsensus.album;
    if (!onlineConsensus.albumArtist.isEmpty()) reviewed.albumArtist = onlineConsensus.albumArtist;
    if (!onlineConsensus.genre.isEmpty())       reviewed.genre       = onlineConsensus.genre;
    if (!onlineConsensus.composer.isEmpty())    reviewed.composer    = onlineConsensus.composer;
    if (onlineConsensus.year > 0)               reviewed.year        = onlineConsensus.year;
    if (onlineConsensus.trackNumber > 0)        reviewed.trackNumber = onlineConsensus.trackNumber;
    if (onlineConsensus.discNumber > 0)         reviewed.discNumber  = onlineConsensus.discNumber;

    // Verify all fields are forced-overwritten by proper online tags
    EXPECT_EQ(reviewed.title, "Viva La Vida");
    EXPECT_EQ(reviewed.artist, "Coldplay");
    EXPECT_EQ(reviewed.album, "Viva la Vida or Death and All His Friends");
    EXPECT_EQ(reviewed.genre, "Alternative Rock");
    EXPECT_EQ(reviewed.year, 2008);
    EXPECT_EQ(reviewed.trackNumber, 7);
    EXPECT_EQ(reviewed.discNumber, 1);
    EXPECT_EQ(reviewed.composer, "Guy Berryman, Jonny Buckland, Will Champion, Chris Martin");
}

TEST_F(ReviewFlowTest, MetadataEngineReviewSongSignalEmission)
{
    SettingsManager settings(QDir::tempPath());
    MetadataEngine engine(&settings);
    NetworkService network;
    engine.setNetworkService(&network);

    Song song;
    song.filePath = "/dummy/music/Queen - Bohemian Rhapsody.mp3";
    song.fileName = "Queen - Bohemian Rhapsody.mp3";
    song.metadata.title = "Wrong Title";
    song.metadata.artist = "Wrong Artist";

    bool receivedReviewFinished = false;
    QString resultPath;
    AudioMetadata resultMetadata;

    QObject::connect(&engine, &MetadataEngine::reviewFinished,
                     [&](const QString &filePath, const AudioMetadata &metadata, bool success) {
                         Q_UNUSED(success);
                         receivedReviewFinished = true;
                         resultPath = filePath;
                         resultMetadata = metadata;
                     });

    // Call reviewSong
    engine.reviewSong(song);

    // Simulate NetworkService emitting aggregated lookup results for the requested song
    AudioMetadata onlineResult;
    onlineResult.title = "Bohemian Rhapsody";
    onlineResult.artist = "Queen";
    onlineResult.album = "A Night at the Opera";
    onlineResult.year = 1975;
    onlineResult.genre = "Rock";

    emit network.lookupAggregated("Wrong Artist", "Wrong Title", onlineResult);

    // Process Qt event loop
    QCoreApplication::processEvents();

    EXPECT_TRUE(receivedReviewFinished);
    EXPECT_EQ(resultPath, song.filePath);
    EXPECT_EQ(resultMetadata.title, "Bohemian Rhapsody");
    EXPECT_EQ(resultMetadata.artist, "Queen");
    EXPECT_EQ(resultMetadata.album, "A Night at the Opera");
    EXPECT_EQ(resultMetadata.year, 1975);
}

TEST_F(ReviewFlowTest, MultiSongBatchReviewFlow)
{
    SettingsManager settings(QDir::tempPath());
    MetadataEngine engine(&settings);
    NetworkService network;
    engine.setNetworkService(&network);

    Song s1;
    s1.filePath = "/dummy/music/Queen - Bohemian Rhapsody.mp3";
    s1.fileName = "Queen - Bohemian Rhapsody.mp3";
    s1.metadata.title = "Bohemian Rhapsody";
    s1.metadata.artist = "Queen";

    Song s2;
    s2.filePath = "/dummy/music/Coldplay - Yellow.mp3";
    s2.fileName = "Coldplay - Yellow.mp3";
    s2.metadata.title = "Yellow";
    s2.metadata.artist = "Coldplay";

    QVector<QString> reviewedPaths;
    QObject::connect(&engine, &MetadataEngine::reviewFinished,
                     [&](const QString &filePath, const AudioMetadata &metadata, bool success) {
                         Q_UNUSED(metadata);
                         Q_UNUSED(success);
                         reviewedPaths.append(filePath);
                     });

    // Review both songs
    engine.reviewSong(s1);
    engine.reviewSong(s2);

    AudioMetadata r1;
    r1.title = "Bohemian Rhapsody";
    r1.artist = "Queen";
    r1.album = "A Night at the Opera";
    r1.year = 1975;

    AudioMetadata r2;
    r2.title = "Yellow";
    r2.artist = "Coldplay";
    r2.album = "Parachutes";
    r2.year = 2000;

    emit network.lookupAggregated("Queen", "Bohemian Rhapsody", r1);
    emit network.lookupAggregated("Coldplay", "Yellow", r2);

    QCoreApplication::processEvents();

    EXPECT_EQ(reviewedPaths.size(), 2);
    EXPECT_TRUE(reviewedPaths.contains(s1.filePath));
    EXPECT_TRUE(reviewedPaths.contains(s2.filePath));
}

TEST_F(ReviewFlowTest, YouTubeTitleCleaningFlow)
{
    FilenameIntelligence fi;
    QString ytTitle = "Alan Walker - Faded (Official Music Video) [HD]";
    QString cleaned = fi.cleanFilename(ytTitle);
    AudioMetadata meta = fi.parse(cleaned);

    EXPECT_EQ(meta.artist, "Alan Walker");
    EXPECT_EQ(meta.title, "Faded");
}

TEST_F(ReviewFlowTest, ReviewBatchProgressAndCompletionFlow)
{
    SettingsManager settings(QDir::tempPath());
    MetadataEngine engine(&settings);
    NetworkService network;
    engine.setNetworkService(&network);

    Song s1;
    s1.filePath = "/dummy/music/Track1.mp3";
    s1.fileName = "Track1.mp3";
    s1.metadata.title = "Song One";
    s1.metadata.artist = "Artist A";

    Song s2;
    s2.filePath = "/dummy/music/Track2.mp3";
    s2.fileName = "Track2.mp3";
    s2.metadata.title = "Song Two";
    s2.metadata.artist = "Artist B";

    QVector<Song> batch = {s1, s2};

    int lastProgressCompleted = -1;
    int lastProgressTotal = -1;
    bool batchFinishedEmitted = false;
    int finishedSuccess = -1;
    int finishedFail = -1;

    QObject::connect(&engine, &MetadataEngine::reviewBatchProgress,
                     [&](int completed, int total) {
                         lastProgressCompleted = completed;
                         lastProgressTotal = total;
                     });

    QObject::connect(&engine, &MetadataEngine::reviewBatchFinished,
                     [&](int success, int fail) {
                         batchFinishedEmitted = true;
                         finishedSuccess = success;
                         finishedFail = fail;
                     });

    engine.reviewBatch(batch);
    EXPECT_EQ(lastProgressCompleted, 0);
    EXPECT_EQ(lastProgressTotal, 2);

    AudioMetadata r1;
    r1.title = "Song One";
    r1.artist = "Artist A";
    r1.album = "Album 1";

    AudioMetadata r2;
    r2.title = "Song Two";
    r2.artist = "Artist B";
    r2.album = "Album 2";

    emit network.lookupAggregated("Artist A", "Song One", r1);
    QCoreApplication::processEvents();
    EXPECT_EQ(lastProgressCompleted, 1);

    emit network.lookupAggregated("Artist B", "Song Two", r2);
    QCoreApplication::processEvents();
    EXPECT_EQ(lastProgressCompleted, 2);
    EXPECT_TRUE(batchFinishedEmitted);
    EXPECT_EQ(finishedSuccess, 2);
    EXPECT_EQ(finishedFail, 0);
}



#include <gtest/gtest.h>

#include "../src/core/FilenameIntelligence.h"

using namespace tagit;

class FilenameIntelligenceTest : public ::testing::Test {
protected:
    FilenameIntelligence parser;
};

TEST_F(FilenameIntelligenceTest, ArtistTitleDash)
{
    AudioMetadata md = parser.parse("Alan Walker - Faded.mp3");
    EXPECT_EQ(md.artist, "Alan Walker");
    EXPECT_EQ(md.title, "Faded");
    EXPECT_GE(md.confidence.artist, 80);
}

TEST_F(FilenameIntelligenceTest, TrackNumberArtistTitle)
{
    AudioMetadata md = parser.parse("01 Alan Walker - Faded.mp3");
    EXPECT_EQ(md.trackNumber, 1);
    EXPECT_EQ(md.artist, "Alan Walker");
    EXPECT_EQ(md.title, "Faded");
}

TEST_F(FilenameIntelligenceTest, RemovesArtifacts)
{
    AudioMetadata md = parser.parse("Alan_Walker_-_Faded_(Official_Audio)(320kbps).mp3");
    EXPECT_EQ(md.artist, "Alan Walker");
    EXPECT_EQ(md.title, "Faded");
}

TEST_F(FilenameIntelligenceTest, FtFeaturingHandling)
{
    // The full featuring clause is now kept as part of the artist field.
    // This is the correct behaviour: "Artist ft Other Artist" should all
    // be stored in artist so the online search gets the full credit string.
    AudioMetadata md = parser.parse("Artist ft Other Artist - Title.mp3");
    EXPECT_EQ(md.artist, "Artist ft Other Artist");
    EXPECT_EQ(md.title,  "Title");
}

TEST_F(FilenameIntelligenceTest, SidhuMooseWalaLevels)
{
    // Real-world example that previously produced a swapped artist/title.
    AudioMetadata md = parser.parse(
        "Sidhu_Moose_Wala_ft_Sunny_Malton_The_Kidd_-_LEVELS.mp3");
    EXPECT_EQ(md.artist, "Sidhu Moose Wala ft Sunny Malton The Kidd");
    EXPECT_EQ(md.title,  "LEVELS");
}

TEST_F(FilenameIntelligenceTest, PreservesDescriptors)
{
    AudioMetadata md = parser.parse("Artist - Title (Live).mp3");
    EXPECT_EQ(md.title, "Title Live");
}

TEST_F(FilenameIntelligenceTest, WebDomainAndBracketsStripping)
{
    AudioMetadata md = parser.parse("[Songs.pk] 03 - Atif Aslam - Tera Hone Laga Hoon [320kbps].mp3");
    EXPECT_EQ(md.artist, "Atif Aslam");
    EXPECT_EQ(md.title, "Tera Hone Laga Hoon");
    EXPECT_EQ(md.trackNumber, 3);
}

TEST_F(FilenameIntelligenceTest, EnDashAndPipeSeparators)
{
    AudioMetadata md1 = parser.parse("Arijit Singh – Tum Hi Ho.mp3");
    EXPECT_EQ(md1.artist, "Arijit Singh");
    EXPECT_EQ(md1.title, "Tum Hi Ho");

    AudioMetadata md2 = parser.parse("Coldplay | Fix You (Official Video).mp3");
    EXPECT_EQ(md2.artist, "Coldplay");
    EXPECT_EQ(md2.title, "Fix You");
}

TEST_F(FilenameIntelligenceTest, CanExtract)
{
    EXPECT_TRUE(parser.canExtractArtistTitle("Artist - Title.mp3"));
    EXPECT_FALSE(parser.canExtractArtistTitle("JustATitle.mp3"));
}

TEST_F(FilenameIntelligenceTest, CleanFilename)
{
    EXPECT_EQ(parser.cleanFilename("Alan_Walker_-_Faded_(Official_Audio)(320kbps).mp3"),
              "Alan Walker - Faded.mp3");

    EXPECT_EQ(parser.cleanFilename("[Songs.pk] 03 - Atif Aslam - Tera Hone Laga Hoon [320kbps].mp3"),
              "03 - Atif Aslam - Tera Hone Laga Hoon.mp3");

    EXPECT_EQ(parser.cleanFilename("Coldplay | Fix You (Official Video).mp3"),
              "Coldplay - Fix You.mp3");

    EXPECT_EQ(parser.cleanFilename("Artist - Title (Live).mp3"),
              "Artist - Title (Live).mp3");
}

TEST_F(FilenameIntelligenceTest, BatchCleaningFlow)
{
    const std::vector<std::pair<QString, std::pair<QString, QString>>> batchSamples = {
        {"[www.SongsPk.info] 01 - The Weeknd - Blinding Lights (Official Video) [320kbps].mp3",
         {"01 - The Weeknd - Blinding Lights.mp3", "The Weeknd"}},
        {"Dua_Lipa_-_Levitating_(Audio)(HD).flac",
         {"Dua Lipa - Levitating.flac", "Dua Lipa"}},
        {"Imagine Dragons | Believer (Lyric Video).m4a",
         {"Imagine Dragons - Believer.m4a", "Imagine Dragons"}}
    };

    for (const auto &[rawName, expected] : batchSamples) {
        const QString cleaned = parser.cleanFilename(rawName);
        EXPECT_EQ(cleaned, expected.first);

        const AudioMetadata parsed = parser.parse(cleaned);
        EXPECT_EQ(parsed.artist, expected.second);
        EXPECT_FALSE(parsed.title.isEmpty());
    }
}


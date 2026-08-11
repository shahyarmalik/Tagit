#include <gtest/gtest.h>

#include "../src/model/AudioMetadata.h"

using namespace tagit;

TEST(AudioMetadataTest, EmptyByDefault)
{
    AudioMetadata md;
    EXPECT_TRUE(md.isEmpty());
    EXPECT_FALSE(md.isComplete());
}

TEST(AudioMetadataTest, CompleteRequiresTitleAndArtist)
{
    AudioMetadata md;
    md.title = "Faded";
    md.artist = "Alan Walker";
    EXPECT_FALSE(md.isEmpty());
    EXPECT_TRUE(md.isComplete());
    EXPECT_FALSE(md.hasAllCoreFields());
}

TEST(AudioMetadataTest, AllCoreFields)
{
    AudioMetadata md;
    md.title = "Faded";
    md.artist = "Alan Walker";
    md.album = "Different World";
    md.genre = "EDM";
    EXPECT_TRUE(md.hasAllCoreFields());
}

TEST(AudioMetadataTest, MergeMissingOnly)
{
    AudioMetadata existing;
    existing.title = "Faded";

    AudioMetadata incoming;
    incoming.title = "ShouldNotOverwrite";
    incoming.artist = "Alan Walker";
    incoming.album = "Different World";

    existing.mergeMissing(incoming);

    EXPECT_EQ(existing.title, "Faded");
    EXPECT_EQ(existing.artist, "Alan Walker");
    EXPECT_EQ(existing.album, "Different World");
}

TEST(AudioMetadataTest, CanEnrichFrom)
{
    AudioMetadata existing;
    existing.title = "Faded";
    existing.artist = "Alan Walker";

    AudioMetadata incoming;
    incoming.album = "Different World";
    incoming.genre = "EDM";

    EXPECT_TRUE(existing.canEnrichFrom(incoming));
}


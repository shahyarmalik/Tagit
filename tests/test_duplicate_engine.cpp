#include <gtest/gtest.h>

#include "../src/core/DuplicateEngine.h"
#include "../src/model/Song.h"

using namespace tagit;

TEST(DuplicateEngineTest, StrongDuplicates)
{
    Song a, b;
    a.id = 1;
    a.fileSize = 4096;
    a.metadata.durationMs = 240000;
    a.metadata.bitrate = 320;

    b.id = 2;
    b.fileSize = 4096;
    b.metadata.durationMs = 240000;
    b.metadata.bitrate = 320;

    DuplicateEngine engine;
    EXPECT_TRUE(engine.areStrongDuplicates(a, b));
}

TEST(DuplicateEngineTest, NotDuplicatesWhenSizeDiffers)
{
    Song a, b;
    a.id = 1;
    a.fileSize = 4096;
    a.metadata.durationMs = 240000;
    a.metadata.bitrate = 320;

    b.id = 2;
    b.fileSize = 8192;
    b.metadata.durationMs = 240000;
    b.metadata.bitrate = 320;

    DuplicateEngine engine;
    EXPECT_FALSE(engine.areStrongDuplicates(a, b));
}

TEST(DuplicateEngineTest, MetadataSimilar)
{
    Song a, b;
    a.id = 1;
    a.metadata.title = "Faded";
    a.metadata.artist = "Alan Walker";
    a.metadata.durationMs = 240000;

    b.id = 2;
    b.metadata.title = "Faded";
    b.metadata.artist = "Alan Walker";
    b.metadata.durationMs = 241000;

    DuplicateEngine engine;
    const auto candidates = engine.findCandidates({a, b});
    EXPECT_EQ(candidates.size(), 1);
}


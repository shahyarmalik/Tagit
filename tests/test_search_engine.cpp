#include <gtest/gtest.h>

#include "../src/core/SearchEngine.h"
#include "../src/model/Song.h"

using namespace tagit;

TEST(SearchEngineTest, EmptyQueryReturnsAll)
{
    SearchEngine engine;
    QVector<Song> songs(3);
    EXPECT_EQ(engine.search(songs, "").size(), 3);
}

TEST(SearchEngineTest, MatchesTitle)
{
    SearchEngine engine;

    Song song;
    song.id = 1;
    song.metadata.title = "Faded";
    song.metadata.artist = "Alan Walker";

    QVector<Song> songs{song};
    const auto results = engine.search(songs, "faded");
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].id, 1);
}

TEST(SearchEngineTest, RanksExactPrefixHigher)
{
    SearchEngine engine;

    Song exact;
    exact.id = 1;
    exact.metadata.title = "Hello";

    Song contains;
    contains.id = 2;
    contains.metadata.title = "Hello World";

    QVector<Song> songs{contains, exact};
    const auto results = engine.search(songs, "hello");
    ASSERT_EQ(results.size(), 2);
    EXPECT_EQ(results[0].id, 1); // exact prefix ranks higher
}


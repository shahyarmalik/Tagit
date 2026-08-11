#include <gtest/gtest.h>
#include "../src/platform/DatabaseService.h"
#include "../src/model/Song.h"

#include <QTemporaryDir>
#include <thread>
#include <vector>

using namespace tagit;

TEST(DatabaseServiceTest, BasicOpenAndTransactions)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const QString dbPath = tempDir.path() + "/test_library.db";

    DatabaseService db;
    ASSERT_TRUE(db.open(dbPath));
    EXPECT_TRUE(db.isOpen());

    ASSERT_TRUE(db.beginTransaction());
    for (int i = 0; i < 50; ++i) {
        Song s;
        s.filePath = QString("/path/to/song_%1.mp3").arg(i);
        s.fileName = QString("song_%1.mp3").arg(i);
        s.metadata.title = QString("Title %1").arg(i);
        s.metadata.artist = "Artist";
        qint64 id = db.upsertSong(s);
        EXPECT_GT(id, 0);
    }
    ASSERT_TRUE(db.commitTransaction());

    auto all = db.allSongs();
    EXPECT_EQ(all.size(), 50);
}

TEST(DatabaseServiceTest, BulkUpsert)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const QString dbPath = tempDir.path() + "/test_bulk.db";

    DatabaseService db;
    ASSERT_TRUE(db.open(dbPath));

    QVector<Song> songs;
    for (int i = 0; i < 100; ++i) {
        Song s;
        s.filePath = QString("/music/track_%1.flac").arg(i);
        s.fileName = QString("track_%1.flac").arg(i);
        s.metadata.title = QString("Track %1").arg(i);
        songs.append(s);
    }

    EXPECT_TRUE(db.upsertSongs(songs));
    EXPECT_EQ(db.allSongs().size(), 100);
}

TEST(DatabaseServiceTest, ThreadSafeConcurrentAccess)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const QString dbPath = tempDir.path() + "/test_concurrent.db";

    DatabaseService db;
    ASSERT_TRUE(db.open(dbPath));

    std::vector<std::thread> workers;
    for (int t = 0; t < 4; ++t) {
        workers.emplace_back([&db, t]() {
            for (int i = 0; i < 25; ++i) {
                Song s;
                s.filePath = QString("/music/thread_%1_song_%2.mp3").arg(t).arg(i);
                s.fileName = QString("thread_%1_song_%2.mp3").arg(t).arg(i);
                s.metadata.title = QString("Thread %1 Song %2").arg(t).arg(i);
                db.upsertSong(s);
                db.allSongs();
            }
        });
    }

    for (auto &w : workers) {
        w.join();
    }

    EXPECT_EQ(db.allSongs().size(), 100);
}

TEST(DatabaseServiceTest, UpdateSongPath)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const QString dbPath = tempDir.path() + "/test_update_path.db";

    DatabaseService db;
    ASSERT_TRUE(db.open(dbPath));

    Song s;
    s.filePath = "/path/to/old_name.mp3";
    s.fileName = "old_name.mp3";
    s.metadata.title = "Old Name Title";
    
    qint64 id = db.upsertSong(s);
    ASSERT_GT(id, 0);

    EXPECT_TRUE(db.updateSongPath("/path/to/old_name.mp3", "/path/to/new_name.mp3", "new_name.mp3"));

    auto all = db.allSongs();
    ASSERT_EQ(all.size(), 1);
    EXPECT_EQ(all[0].filePath, "/path/to/new_name.mp3");
    EXPECT_EQ(all[0].fileName, "new_name.mp3");
}

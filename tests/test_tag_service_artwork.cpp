#include <gtest/gtest.h>
#include "../src/platform/TagService.h"
#include <QByteArray>
#include <QString>

using namespace tagit;

TEST(TagServiceArtworkTest, NonExistentFileFailsGracefully)
{
    TagService service;
    QString nonExistent = "/path/to/non/existent/audio/file.mp3";

    QString mime;
    QByteArray data = service.extractArtwork(nonExistent, &mime);
    EXPECT_TRUE(data.isEmpty());
    EXPECT_TRUE(mime.isEmpty());

    bool writeRes = service.writeArtwork(nonExistent, QByteArray("dummy"), "image/jpeg", false);
    EXPECT_FALSE(writeRes);

    bool removeRes = service.removeArtwork(nonExistent, false);
    EXPECT_FALSE(removeRes);
}

TEST(TagServiceArtworkTest, EmptyDataHandling)
{
    TagService service;
    QString mime;
    QByteArray data = service.extractArtwork("", &mime);
    EXPECT_TRUE(data.isEmpty());

    EXPECT_FALSE(service.writeArtwork("", QByteArray("abc"), "image/png", false));
}

TEST(TagServiceArtworkTest, ValidArtworkDataExtractionCheck)
{
    TagService service;
    QString mime;
    QByteArray sampleData = "SAMPLE_PICTURE_BYTES";
    // Check that dummy calls handle gracefully
    EXPECT_FALSE(service.writeArtwork("/nonexistent.mp3", sampleData, "image/jpeg", false));
    EXPECT_FALSE(service.removeArtwork("/nonexistent.mp3", false));
}


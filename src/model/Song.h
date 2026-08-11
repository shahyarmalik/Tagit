#ifndef TAGIT_SONG_H
#define TAGIT_SONG_H

#include <QString>
#include <QDateTime>
#include <QMetaType>

#include "AudioMetadata.h"

namespace tagit {

/**
 * @brief A single audio file as indexed in the library.
 */
struct Song {
    qint64 id = -1;
    QString filePath;
    QString fileName;
    QString format;
    qint64 fileSize = 0;
    QDateTime modifiedTime;

    AudioMetadata metadata;

    bool hasCompleteMetadata() const
    {
        return metadata.isComplete();
    }
};

} // namespace tagit

Q_DECLARE_METATYPE(tagit::Song)

#endif // TAGIT_SONG_H


#ifndef TAGIT_FILE_ORGANIZER_H
#define TAGIT_FILE_ORGANIZER_H

#include <QString>
#include <QPair>
#include <QVector>

#include "../model/Song.h"
#include "../platform/FilesystemService.h"

namespace tagit {

/**
 * @brief Bulk folder organization.
 *
 * Given a target root and a template pattern, moves files into an
 * organized folder structure such as:
 *   <root>/<Artist>/<Album>/01 - Title.mp3
 *
 * Non-destructive by default: files are only moved, never deleted, and
 * destination collisions are avoided via unique naming.
 */
class FileOrganizer {
public:
    explicit FileOrganizer(const QString &targetRoot);

    /// Build the destination path for @p song using @p pattern.
    QString destinationPath(const Song &song, const QString &pattern) const;

    /// Move @p song to its destination. Returns the new path (or empty on failure).
    QString organizeSong(const Song &song, const QString &pattern) const;

    /// Preview: compute where each song would go without moving anything.
    QVector<QPair<Song, QString>> preview(const QVector<Song> &songs,
                                          const QString &pattern) const;

    QString targetRoot() const;

private:
    QString m_targetRoot;
    FilesystemService m_fs;
};

} // namespace tagit

#endif // TAGIT_FILE_ORGANIZER_H


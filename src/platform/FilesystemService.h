#ifndef TAGIT_FILESYSTEM_SERVICE_H
#define TAGIT_FILESYSTEM_SERVICE_H

#include <QString>
#include <QStringList>

namespace tagit {

/**
 * @brief Filesystem abstraction: scanning, format detection, safe renames.
 */
class FilesystemService {
public:
    FilesystemService() = default;

    /// Recursively find audio files under @p rootPath.
    QStringList scanForAudioFiles(const QString &rootPath, const QStringList &extensions) const;

    /// Returns the lower-case file extension including the dot (e.g. ".mp3").
    static QString fileExtension(const QString &filePath);

    /// Returns true if the file path has a supported audio extension.
    bool isAudioFile(const QString &filePath, const QStringList &extensions) const;

    /// Rename/move a file safely. Returns true on success.
    static bool safeRename(const QString &oldPath, const QString &newPath);

    /// Create a unique file path by appending " (1)", " (2)" when needed.
    static QString uniqueFilePath(const QString &desiredPath);
};

} // namespace tagit

#endif // TAGIT_FILESYSTEM_SERVICE_H


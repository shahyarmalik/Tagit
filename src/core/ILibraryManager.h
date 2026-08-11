#ifndef TAGIT_ILIBRARY_MANAGER_H
#define TAGIT_ILIBRARY_MANAGER_H

#include <QObject>
#include <QVector>

#include "../model/Song.h"

namespace tagit {

struct ScanProgress {
    int filesScanned = 0;
    int totalFiles = 0;
    QString currentFile;
};

/**
 * @brief Interface for library scanning and indexing.
 */
class ILibraryManager : public QObject {
    Q_OBJECT
public:
    explicit ILibraryManager(QObject *parent = nullptr) : QObject(parent) {}
    ~ILibraryManager() override = default;

    /// Scan a directory recursively for audio files.
    virtual void scanDirectory(const QString &path) = 0;

    /// Cancel an ongoing scan.
    virtual void cancelScan() = 0;

    /// Returns all indexed tracks.
    virtual QVector<Song> allTracks() const = 0;

    /// Returns the count of indexed tracks.
    virtual int trackCount() const = 0;

    /// Clear the library index.
    virtual void clearLibrary() = 0;

signals:
    void scanStarted(const QString &path);
    void scanProgress(const ScanProgress &progress);
    void scanFinished(bool success, const QString &message);
    void trackAdded(const Song &track);
    void libraryCleared();
};

} // namespace tagit

#endif // TAGIT_ILIBRARY_MANAGER_H


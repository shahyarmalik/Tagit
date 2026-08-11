#ifndef TAGIT_DATABASE_SERVICE_H
#define TAGIT_DATABASE_SERVICE_H

#include <QString>
#include <QStringList>
#include <QVector>
#include <memory>

namespace tagit {

struct Song;

/**
 * @brief SQLite-backed persistence for the library index.
 *
 * The service is implemented with the SQLite C API directly (or via a small
 * internal helper) and is guarded by TAGIT_HAS_SQLITE. When SQLite is not
 * available the service degrades to a no-op in-memory stub so the rest of the
 * application remains fully functional.
 */
class DatabaseService {
public:
    DatabaseService();
    ~DatabaseService();

    /// Open (or create) the database at @p dbPath. Returns true on success.
    bool open(const QString &dbPath);

    /// Close the database and release all resources.
    void close();

    bool isOpen() const;

    /// Insert or update a song row. Returns the row id (>= 0) on success.
    qint64 upsertSong(const Song &song);

    /// Bulk insert or update songs in a single transaction.
    bool upsertSongs(const QVector<Song> &songs);

    /// Transaction control helpers for batch operations.
    bool beginTransaction();
    bool commitTransaction();
    bool rollbackTransaction();

    /// Remove a song by id.
    bool removeSong(qint64 id);

    /// Update the file path and file name of a song.
    bool updateSongPath(const QString &oldPath, const QString &newPath, const QString &newFileName);

    /// Return all songs currently in the database.
    QVector<Song> allSongs() const;

    /// Delete all rows.
    bool clear();

    /// Run a simple one-shot query returning the first column as strings.
    QStringList queryStrings(const QString &sql) const;

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace tagit

#endif // TAGIT_DATABASE_SERVICE_H


#include "DatabaseService.h"
#include "../model/Song.h"

#ifdef TAGIT_HAS_SQLITE
#include <sqlite3.h>
#endif

#include <QDir>
#include <QDateTime>
#include <QFileInfo>
#include <QVariant>
#include <mutex>

namespace tagit {

struct DatabaseService::Impl {
#ifdef TAGIT_HAS_SQLITE
    sqlite3 *db = nullptr;
#endif
    bool open = false;
    mutable std::recursive_mutex mutex;
};

DatabaseService::DatabaseService()
    : d(std::make_unique<Impl>())
{
}

DatabaseService::~DatabaseService()
{
    close();
}

bool DatabaseService::open(const QString &dbPath)
{
    std::lock_guard<std::recursive_mutex> lock(d->mutex);
#ifdef TAGIT_HAS_SQLITE
    if (d->db) {
        return true;
    }
    QFileInfo info(dbPath);
    QDir().mkpath(info.absolutePath());

    if (sqlite3_open(dbPath.toUtf8().constData(), &d->db) != SQLITE_OK) {
        if (d->db) {
            sqlite3_close(d->db);
            d->db = nullptr;
        }
        return false;
    }

    // Configure SQLite performance and concurrency settings
    sqlite3_exec(d->db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(d->db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(d->db, "PRAGMA busy_timeout=5000;", nullptr, nullptr, nullptr);

    const char *schema =
        "CREATE TABLE IF NOT EXISTS songs ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  path TEXT NOT NULL UNIQUE,"
        "  file_name TEXT,"
        "  format TEXT,"
        "  file_size INTEGER DEFAULT 0,"
        "  modified_time TEXT,"
        "  title TEXT,"
        "  artist TEXT,"
        "  album TEXT,"
        "  genre TEXT,"
        "  comment TEXT,"
        "  track_number INTEGER DEFAULT 0,"
        "  disc_number INTEGER DEFAULT 0,"
        "  year INTEGER DEFAULT 0,"
        "  duration_ms INTEGER DEFAULT 0,"
        "  bitrate INTEGER DEFAULT 0,"
        "  has_artwork INTEGER DEFAULT 0"
        ");";

    char *errMsg = nullptr;
    if (sqlite3_exec(d->db, schema, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        sqlite3_free(errMsg);
        sqlite3_close(d->db);
        d->db = nullptr;
        return false;
    }
    d->open = true;
    return true;
#else
    Q_UNUSED(dbPath)
    d->open = false;
    return false;
#endif
}

void DatabaseService::close()
{
    std::lock_guard<std::recursive_mutex> lock(d->mutex);
#ifdef TAGIT_HAS_SQLITE
    if (d->db) {
        sqlite3_close(d->db);
        d->db = nullptr;
    }
#endif
    d->open = false;
}

bool DatabaseService::isOpen() const
{
    std::lock_guard<std::recursive_mutex> lock(d->mutex);
    return d->open;
}

bool DatabaseService::beginTransaction()
{
    std::lock_guard<std::recursive_mutex> lock(d->mutex);
#ifdef TAGIT_HAS_SQLITE
    if (!d->db) return false;
    char *errMsg = nullptr;
    const int rc = sqlite3_exec(d->db, "BEGIN TRANSACTION;", nullptr, nullptr, &errMsg);
    if (errMsg) sqlite3_free(errMsg);
    return rc == SQLITE_OK;
#else
    return false;
#endif
}

bool DatabaseService::commitTransaction()
{
    std::lock_guard<std::recursive_mutex> lock(d->mutex);
#ifdef TAGIT_HAS_SQLITE
    if (!d->db) return false;
    char *errMsg = nullptr;
    const int rc = sqlite3_exec(d->db, "COMMIT;", nullptr, nullptr, &errMsg);
    if (errMsg) sqlite3_free(errMsg);
    return rc == SQLITE_OK;
#else
    return false;
#endif
}

bool DatabaseService::rollbackTransaction()
{
    std::lock_guard<std::recursive_mutex> lock(d->mutex);
#ifdef TAGIT_HAS_SQLITE
    if (!d->db) return false;
    char *errMsg = nullptr;
    const int rc = sqlite3_exec(d->db, "ROLLBACK;", nullptr, nullptr, &errMsg);
    if (errMsg) sqlite3_free(errMsg);
    return rc == SQLITE_OK;
#else
    return false;
#endif
}

qint64 DatabaseService::upsertSong(const Song &song)
{
    std::lock_guard<std::recursive_mutex> lock(d->mutex);
#ifdef TAGIT_HAS_SQLITE
    if (!d->db) {
        return -1;
    }
    sqlite3_stmt *stmt = nullptr;
    const char *sql =
        "INSERT INTO songs (path,file_name,format,file_size,modified_time,"
        "title,artist,album,genre,comment,track_number,disc_number,year,"
        "duration_ms,bitrate,has_artwork) "
        "VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16) "
        "ON CONFLICT(path) DO UPDATE SET "
        "file_name=excluded.file_name, format=excluded.format, "
        "file_size=excluded.file_size, modified_time=excluded.modified_time, "
        "title=excluded.title, artist=excluded.artist, album=excluded.album, "
        "genre=excluded.genre, comment=excluded.comment, "
        "track_number=excluded.track_number, disc_number=excluded.disc_number, "
        "year=excluded.year, duration_ms=excluded.duration_ms, "
        "bitrate=excluded.bitrate, has_artwork=excluded.has_artwork;";

    if (sqlite3_prepare_v2(d->db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, song.filePath.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, song.fileName.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, song.format.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, song.fileSize);
    sqlite3_bind_text(stmt, 5, song.modifiedTime.toString(Qt::ISODate).toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, song.metadata.title.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, song.metadata.artist.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, song.metadata.album.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 9, song.metadata.genre.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 10, song.metadata.comment.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 11, song.metadata.trackNumber);
    sqlite3_bind_int(stmt, 12, song.metadata.discNumber);
    sqlite3_bind_int(stmt, 13, song.metadata.year);
    sqlite3_bind_int64(stmt, 14, song.metadata.durationMs);
    sqlite3_bind_int(stmt, 15, song.metadata.bitrate);
    sqlite3_bind_int(stmt, 16, song.metadata.hasEmbeddedArtwork ? 1 : 0);

    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return -1;
    }

    if (song.id >= 0) {
        return song.id;
    }
    return sqlite3_last_insert_rowid(d->db);
#else
    Q_UNUSED(song)
    return -1;
#endif
}

bool DatabaseService::upsertSongs(const QVector<Song> &songs)
{
    if (songs.isEmpty()) return true;
    std::lock_guard<std::recursive_mutex> lock(d->mutex);
#ifdef TAGIT_HAS_SQLITE
    if (!d->db) return false;

    beginTransaction();
    for (const Song &s : songs) {
        upsertSong(s);
    }
    return commitTransaction();
#else
    return false;
#endif
}

bool DatabaseService::removeSong(qint64 id)
{
    std::lock_guard<std::recursive_mutex> lock(d->mutex);
#ifdef TAGIT_HAS_SQLITE
    if (!d->db) {
        return false;
    }
    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(d->db, "DELETE FROM songs WHERE id = ?1;", -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt, 1, id);
    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
#else
    Q_UNUSED(id)
    return false;
#endif
}

bool DatabaseService::updateSongPath(const QString &oldPath, const QString &newPath, const QString &newFileName)
{
    std::lock_guard<std::recursive_mutex> lock(d->mutex);
#ifdef TAGIT_HAS_SQLITE
    if (!d->db) {
        return false;
    }
    sqlite3_stmt *stmt = nullptr;
    const char *sql = "UPDATE songs SET path = ?1, file_name = ?2 WHERE path = ?3;";
    if (sqlite3_prepare_v2(d->db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_text(stmt, 1, newPath.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, newFileName.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, oldPath.toUtf8().constData(), -1, SQLITE_TRANSIENT);

    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
#else
    Q_UNUSED(oldPath)
    Q_UNUSED(newPath)
    Q_UNUSED(newFileName)
    return false;
#endif
}

QVector<Song> DatabaseService::allSongs() const
{
    std::lock_guard<std::recursive_mutex> lock(d->mutex);
    QVector<Song> songs;
#ifdef TAGIT_HAS_SQLITE
    if (!d->db) {
        return songs;
    }
    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(d->db,
        "SELECT id,path,file_name,format,file_size,modified_time,"
        "title,artist,album,genre,comment,track_number,disc_number,year,"
        "duration_ms,bitrate,has_artwork FROM songs;",
        -1, &stmt, nullptr);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Song song;
        song.id = sqlite3_column_int64(stmt, 0);
        song.filePath = QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1)));
        song.fileName = QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2)));
        song.format = QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3)));
        song.fileSize = sqlite3_column_int64(stmt, 4);
        song.modifiedTime = QDateTime::fromString(
            QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 5))),
            Qt::ISODate);
        song.metadata.title = QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6)));
        song.metadata.artist = QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 7)));
        song.metadata.album = QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 8)));
        song.metadata.genre = QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 9)));
        song.metadata.comment = QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 10)));
        song.metadata.trackNumber = sqlite3_column_int(stmt, 11);
        song.metadata.discNumber = sqlite3_column_int(stmt, 12);
        song.metadata.year = sqlite3_column_int(stmt, 13);
        song.metadata.durationMs = sqlite3_column_int64(stmt, 14);
        song.metadata.bitrate = sqlite3_column_int(stmt, 15);
        song.metadata.hasEmbeddedArtwork = sqlite3_column_int(stmt, 16) != 0;
        songs.append(song);
    }
    sqlite3_finalize(stmt);
#endif
    return songs;
}

bool DatabaseService::clear()
{
    std::lock_guard<std::recursive_mutex> lock(d->mutex);
#ifdef TAGIT_HAS_SQLITE
    if (!d->db) {
        return false;
    }
    char *errMsg = nullptr;
    const int rc = sqlite3_exec(d->db, "DELETE FROM songs;", nullptr, nullptr, &errMsg);
    if (errMsg) {
        sqlite3_free(errMsg);
    }
    return rc == SQLITE_OK;
#else
    return false;
#endif
}

QStringList DatabaseService::queryStrings(const QString &sql) const
{
    std::lock_guard<std::recursive_mutex> lock(d->mutex);
    QStringList results;
#ifdef TAGIT_HAS_SQLITE
    if (!d->db) {
        return results;
    }
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(d->db, sql.toUtf8().constData(), -1, &stmt, nullptr) != SQLITE_OK) {
        return results;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results << QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)));
    }
    sqlite3_finalize(stmt);
#endif
    return results;
}

} // namespace tagit


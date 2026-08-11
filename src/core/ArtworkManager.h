#ifndef TAGIT_ARTWORK_MANAGER_H
#define TAGIT_ARTWORK_MANAGER_H

#include <QString>
#include <QByteArray>
#include <QObject>

#include "../model/Song.h"

namespace tagit {

/**
 * @brief Preserves embedded artwork and downloads artwork only when missing.
 *
 * Downloaded artwork is cached in the app cache directory and can be embedded
 * into supported file formats via TagService (Phase 5 integration).
 */
class ArtworkManager : public QObject {
    Q_OBJECT
public:
    explicit ArtworkManager(const QString &cacheDir, QObject *parent = nullptr);

    /// Return the cached artwork bytes for @p song (if any).
    QByteArray cachedArtwork(const Song &song) const;

    /// Cache the given artwork bytes for @p song.
    bool cacheArtwork(const Song &song, const QByteArray &data);

    /// True if artwork is already present (embedded or cached).
    bool hasArtwork(const Song &song) const;

    /// Clear all cached artwork.
    void clearCache();

private:
    QString artworkCachePath(const Song &song) const;

    QString m_cacheDir;
};

} // namespace tagit

#endif // TAGIT_ARTWORK_MANAGER_H


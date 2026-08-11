#ifndef TAGIT_TAG_SERVICE_H
#define TAGIT_TAG_SERVICE_H

#include <QString>
#include <QByteArray>
#include <QStringList>

#include "../model/AudioMetadata.h"

namespace tagit {

/**
 * @brief Reads and writes embedded audio metadata via TagLib.
 *
 * Two write modes:
 *
 *  writeMissingTags()   — auto-enrichment path.
 *                         Only fills fields that are currently empty.
 *                         Never overwrites anything already in the file.
 *
 *  writeSelectedTags()  — explicit user-save path (Inspector panel).
 *                         Writes exactly the fields listed in @p fields,
 *                         overwriting whatever is currently in the file.
 *                         Fields NOT in @p fields are left completely alone.
 */
class TagService {
public:
    TagService() = default;

    /// Read all embedded metadata from @p filePath.
    AudioMetadata readTags(const QString &filePath) const;

    /**
     * @brief Non-destructive auto-enrich write.
     * Only fills fields that are currently empty in the file.
     */
    bool writeMissingTags(const QString       &filePath,
                          const AudioMetadata &metadata,
                          bool                 backupOriginal = true) const;

    /**
     * @brief Explicit user-save write.
     *
     * Writes only the fields whose name is in @p fields, overwriting
     * whatever value is currently embedded.  All other fields are untouched.
     *
     * Field name strings:
     *   "title", "artist", "album", "albumArtist", "genre", "composer",
     *   "year", "trackNumber", "discNumber", "lyrics", "comment"
     *
     * A .bak sidecar is always created before the first write.
     *
     * @return true if the file was saved successfully.
     */
    bool writeSelectedTags(const QString       &filePath,
                           const AudioMetadata &metadata,
                           const QStringList   &fields) const;

    /// Extract embedded artwork bytes from @p filePath (if any).
    QByteArray extractArtwork(const QString &filePath, QString *outMimeType = nullptr) const;

    /**
     * @brief Write artwork image bytes directly to @p filePath.
     * Supports MP3 (ID3v2 APIC), FLAC, MP4 (covr), and Ogg Vorbis.
     */
    bool writeArtwork(const QString    &filePath,
                      const QByteArray &artworkData,
                      const QString    &mimeType = "image/jpeg",
                      bool              backupOriginal = true) const;

    /**
     * @brief Remove embedded artwork from @p filePath.
     */
    bool removeArtwork(const QString &filePath, bool backupOriginal = true) const;

    bool isAvailable() const;
};

} // namespace tagit

#endif // TAGIT_TAG_SERVICE_H

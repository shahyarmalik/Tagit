#ifndef TAGIT_AUDIO_METADATA_H
#define TAGIT_AUDIO_METADATA_H

#include <QString>
#include <QByteArray>
#include <QStringList>
#include <QMetaType>

namespace tagit {

/**
 * @brief Per-field confidence scores (0-100) for generated metadata.
 */
struct FieldConfidence {
    int title       = 0;
    int artist      = 0;
    int album       = 0;
    int genre       = 0;
    int trackNumber = 0;
    int discNumber  = 0;
    int year        = 0;
    int albumArtist = 0;
    int composer    = 0;
    int lyrics      = 0;

    int overall() const {
        if (title == 0 && artist == 0) return 0;
        if (title  == 0) return artist;
        if (artist == 0) return title;
        return (title + artist) / 2;
    }
};

/**
 * @brief The canonical audio metadata model used throughout TagIt.
 */
struct AudioMetadata {
    QString title;
    QString artist;
    QString album;
    QString albumArtist;
    QString genre;
    QString composer;
    QString comment;
    QString lyrics;
    QString isrc;
    QString copyright;
    QString publisher;

    int trackNumber = 0;
    int discNumber  = 0;
    int year        = 0;

    qint64 durationMs         = 0;
    int    bitrate            = 0;
    bool   hasEmbeddedArtwork = false;

    QByteArray artworkData;

    FieldConfidence confidence;

    // ---- Queries ----
    bool isEmpty()          const;
    bool hasTitle()         const;
    bool hasArtist()        const;
    bool isComplete()       const;       ///< title + artist present
    bool hasAllCoreFields() const;       ///< title + artist + album + genre

    // ---- Merge ----
    /**
     * @brief Non-destructive merge: only fill fields that are currently empty.
     * Existing values are NEVER overwritten.
     */
    void mergeMissing(const AudioMetadata &other);

    /**
     * @brief Filtered non-destructive merge.
     */
    void mergeMissingFiltered(const AudioMetadata &other,
                              const QStringList   &allowedFields);

    /**
     * @brief Consensus overwrite merge.
     *
     * Unlike mergeMissing(), this WILL overwrite existing fields when:
     *   1. The consensus field has confidence >= @p minConfidence, AND
     *   2. The consensus value differs meaningfully from the embedded value
     *      (normalised similarity < 80%).
     *
     * Fields where the existing value already closely matches the consensus
     * are left alone (no unnecessary writes).
     *
     * Used after the cross-provider vote when we have high agreement.
     */
    void mergeWithConsensus(const AudioMetadata &consensus,
                            int                  minConfidence,
                            const QStringList   &allowedFields);

    bool canEnrichFrom(const AudioMetadata &other) const;
};

} // namespace tagit

Q_DECLARE_METATYPE(tagit::AudioMetadata)

#endif // TAGIT_AUDIO_METADATA_H

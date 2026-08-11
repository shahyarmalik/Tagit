#ifndef TAGIT_DUPLICATE_ENGINE_H
#define TAGIT_DUPLICATE_ENGINE_H

#include <QVector>
#include <QPair>

#include "../model/Song.h"

namespace tagit {

/**
 * @brief Detects duplicate tracks using metadata, duration, bitrate and file hashes.
 *
 * Audio fingerprinting (Chromaprint) is integrated when TAGIT_HAS_CHROMAPRINT
 * is defined; otherwise detection relies on the cheaper signals below.
 */
class DuplicateEngine {
public:
    DuplicateEngine() = default;

    /// Group IDs of songs that appear to be duplicates (by metadata similarity).
    QVector<QPair<qint64, qint64>> findCandidates(const QVector<Song> &songs) const;

    /// Strong duplicate check: identical size + duration + bitrate.
    bool areStrongDuplicates(const Song &a, const Song &b) const;

    /// Compute a quick file hash (first/last 64KB) for duplicate detection.
    static QString quickFileHash(const QString &filePath);

private:
    bool metadataSimilar(const Song &a, const Song &b) const;
};

} // namespace tagit

#endif // TAGIT_DUPLICATE_ENGINE_H


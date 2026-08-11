#ifndef TAGIT_FILENAME_INTELLIGENCE_H
#define TAGIT_FILENAME_INTELLIGENCE_H

#include <QString>
#include <QStringList>

#include "../model/AudioMetadata.h"

namespace tagit {

/**
 * @brief Rule-based filename parser — one of TagIt's signature features.
 *
 * Recognizes patterns such as:
 *   - "Artist - Title"
 *   - "Title - Artist"
 *   - "Artist ft Artist - Title"
 *   - "Artist x Artist - Title"
 *   - "01 Artist - Title"
 *   - "Title (Official Audio)", "(Lyrics)", "(Visualizer)",
 *     "(Nightcore)", "(Slowed + Reverb)", "(AMV)", "(OST)", "(Cover)"
 *
 * It removes download artifacts ("Official Video", "HD", "(320kbps)", ...)
 * while preserving meaningful descriptors ("Live", "Acoustic", "Seeb Remix").
 */
class FilenameIntelligence {
public:
    FilenameIntelligence();

    /**
     * @brief Parse @p fileName into AudioMetadata.
     *
     * The resulting metadata carries per-field confidence scores.
     */
    AudioMetadata parse(const QString &fileName) const;

    /// Returns true when the parser believes it can extract artist + title.
    bool canExtractArtistTitle(const QString &fileName) const;

    /// Clean a filename into proper readable form by removing irrelevant content.
    QString cleanFilename(const QString &fileName) const;

private:
    QString stripExtension(const QString &fileName) const;
    QStringList tokenize(QString name) const;
    QStringList removeArtifacts(const QStringList &tokens) const;
    AudioMetadata extractArtistTitle(const QStringList &tokens) const;
    AudioMetadata extractTrackNumber(QString &name) const;
    QStringList extractDescriptorList(const QStringList &tokens) const;

    QStringList m_artifactTokens;
    QStringList m_descriptorTokens;
};

} // namespace tagit

#endif // TAGIT_FILENAME_INTELLIGENCE_H


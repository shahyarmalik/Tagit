#include "AudioMetadata.h"
#include <algorithm>

namespace tagit {

bool AudioMetadata::isEmpty() const
{
    return title.isEmpty() && artist.isEmpty() && album.isEmpty()
        && genre.isEmpty() && comment.isEmpty() && lyrics.isEmpty()
        && trackNumber == 0 && discNumber == 0 && year == 0;
}

bool AudioMetadata::hasTitle()  const { return !title.trimmed().isEmpty(); }
bool AudioMetadata::hasArtist() const { return !artist.trimmed().isEmpty(); }

bool AudioMetadata::isComplete() const
{
    return hasTitle() && hasArtist();
}

bool AudioMetadata::hasAllCoreFields() const
{
    return hasTitle() && hasArtist()
        && !album.trimmed().isEmpty()
        && !genre.trimmed().isEmpty();
}

// ---------------------------------------------------------------------------
// mergeMissing — unconditional (all fields)
// ---------------------------------------------------------------------------

void AudioMetadata::mergeMissing(const AudioMetadata &other)
{
    auto ms = [](QString &dst, const QString &src, int &dc, int sc) {
        if (dst.trimmed().isEmpty() && !src.trimmed().isEmpty()) {
            dst = src;  dc = sc;
        }
    };

    ms(title,       other.title,       confidence.title,       other.confidence.title);
    ms(artist,      other.artist,      confidence.artist,      other.confidence.artist);
    ms(album,       other.album,       confidence.album,       other.confidence.album);
    ms(albumArtist, other.albumArtist, confidence.albumArtist, other.confidence.albumArtist);
    ms(genre,       other.genre,       confidence.genre,       other.confidence.genre);
    ms(composer,    other.composer,    confidence.composer,    other.confidence.composer);
    ms(comment,     other.comment,     confidence.title,       other.confidence.title);
    ms(lyrics,      other.lyrics,      confidence.lyrics,      other.confidence.lyrics);
    ms(isrc,        other.isrc,        confidence.title,       other.confidence.title);
    ms(copyright,   other.copyright,   confidence.title,       other.confidence.title);
    ms(publisher,   other.publisher,   confidence.title,       other.confidence.title);

    if (trackNumber == 0 && other.trackNumber > 0) {
        trackNumber            = other.trackNumber;
        confidence.trackNumber = other.confidence.trackNumber;
    }
    if (discNumber == 0 && other.discNumber > 0) {
        discNumber            = other.discNumber;
        confidence.discNumber = other.confidence.discNumber;
    }
    if (year == 0 && other.year > 0) {
        year            = other.year;
        confidence.year = other.confidence.year;
    }
    if (artworkData.isEmpty() && !other.artworkData.isEmpty()) {
        artworkData = other.artworkData;
    }
}

// ---------------------------------------------------------------------------
// mergeMissingFiltered — only touch fields the user has allowed
// ---------------------------------------------------------------------------

void AudioMetadata::mergeMissingFiltered(const AudioMetadata &other,
                                          const QStringList   &allowed)
{
    // Helper: merge one string field only when its key is in allowed list
    auto ms = [&](const QString &key,
                  QString &dst, const QString &src,
                  int &dc, int sc) {
        if (!allowed.contains(key)) return;
        if (dst.trimmed().isEmpty() && !src.trimmed().isEmpty()) {
            dst = src;  dc = sc;
        }
    };

    ms("title",       title,       other.title,       confidence.title,       other.confidence.title);
    ms("artist",      artist,      other.artist,      confidence.artist,      other.confidence.artist);
    ms("album",       album,       other.album,       confidence.album,       other.confidence.album);
    ms("albumArtist", albumArtist, other.albumArtist, confidence.albumArtist, other.confidence.albumArtist);
    ms("genre",       genre,       other.genre,       confidence.genre,       other.confidence.genre);
    ms("composer",    composer,    other.composer,    confidence.composer,    other.confidence.composer);
    ms("lyrics",      lyrics,      other.lyrics,      confidence.lyrics,      other.confidence.lyrics);

    if (allowed.contains("year") && year == 0 && other.year > 0) {
        year            = other.year;
        confidence.year = other.confidence.year;
    }
    if (allowed.contains("trackNumber") && trackNumber == 0 && other.trackNumber > 0) {
        trackNumber            = other.trackNumber;
        confidence.trackNumber = other.confidence.trackNumber;
    }
    if (allowed.contains("discNumber") && discNumber == 0 && other.discNumber > 0) {
        discNumber            = other.discNumber;
        confidence.discNumber = other.confidence.discNumber;
    }
    if (allowed.contains("artwork") && artworkData.isEmpty() && !other.artworkData.isEmpty()) {
        artworkData = other.artworkData;
    }
}

// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// mergeWithConsensus — overwrite wrong fields when consensus is confident
// ---------------------------------------------------------------------------

static int levenshtein(const QString &a, const QString &b)
{
    const int la = static_cast<int>(a.size());
    const int lb = static_cast<int>(b.size());
    if (la == 0) return lb;
    if (lb == 0) return la;
    QVector<int> prev(lb + 1), curr(lb + 1);
    for (int j = 0; j <= lb; ++j) prev[j] = j;
    for (int i = 1; i <= la; ++i) {
        curr[0] = i;
        for (int j = 1; j <= lb; ++j) {
            const int cost = (a[i-1] == b[j-1]) ? 0 : 1;
            curr[j] = std::min({curr[j-1]+1, prev[j]+1, prev[j-1]+cost});
        }
        prev = curr;
    }
    return prev[lb];
}

/// Simple normalised similarity [0,100] for comparing two strings.
/// Used to decide if two values are "essentially the same".
static int stringSimilarity(const QString &a, const QString &b)
{
    const QString na = a.trimmed().toLower().simplified();
    const QString nb = b.trimmed().toLower().simplified();
    if (na.isEmpty() && nb.isEmpty()) return 100;
    if (na.isEmpty() || nb.isEmpty()) return 0;
    if (na == nb) return 100;
    // Prefix check (one contains the other)
    if (na.contains(nb) || nb.contains(na)) return 75;
    
    const int maxLen = std::max(static_cast<int>(na.size()),
                                static_cast<int>(nb.size()));
    const int dist = levenshtein(na, nb);
    return std::max(0, 100 - (dist * 100 / maxLen));
}

void AudioMetadata::mergeWithConsensus(const AudioMetadata &consensus,
                                        int                  minConfidence,
                                        const QStringList   &allowedFields)
{
    const bool anyAllowed = allowedFields.isEmpty();   // empty = all allowed

    // For a string field: overwrite if:
    //   (a) the field is in allowedFields, AND
    //   (b) the consensus value has confidence >= minConfidence, AND
    //   (c) the existing value differs significantly from the consensus
    //       (similarity < 75%) OR the field is currently empty.
    auto over = [&](const QString &key,
                    QString       &dst,
                    const QString &src,
                    int            srcConf,
                    int           &dstConf) {
        if (!anyAllowed && !allowedFields.contains(key)) return;
        if (src.trimmed().isEmpty()) return;
        if (srcConf < minConfidence) return;

        const int sim = stringSimilarity(dst, src);
        if (dst.trimmed().isEmpty() || sim < 75) {
            dst     = src;
            dstConf = srcConf;
        }
    };

    auto overInt = [&](const QString &key,
                       int           &dst,
                       int            src,
                       int            srcConf,
                       int           &dstConf) {
        if (!anyAllowed && !allowedFields.contains(key)) return;
        if (src == 0) return;
        if (srcConf < minConfidence) return;
        // For integers: overwrite if empty or different
        if (dst == 0 || dst != src) {
            dst     = src;
            dstConf = srcConf;
        }
    };

    over("title",       title,       consensus.title,       consensus.confidence.title,       confidence.title);
    over("artist",      artist,      consensus.artist,      consensus.confidence.artist,       confidence.artist);
    over("album",       album,       consensus.album,       consensus.confidence.album,        confidence.album);
    over("albumArtist", albumArtist, consensus.albumArtist, consensus.confidence.albumArtist,  confidence.albumArtist);
    over("genre",       genre,       consensus.genre,       consensus.confidence.genre,        confidence.genre);
    over("composer",    composer,    consensus.composer,    consensus.confidence.composer,     confidence.composer);
    over("lyrics",      lyrics,      consensus.lyrics,      consensus.confidence.lyrics,       confidence.lyrics);

    overInt("year",        year,        consensus.year,        consensus.confidence.year,        confidence.year);
    overInt("trackNumber", trackNumber, consensus.trackNumber, consensus.confidence.trackNumber, confidence.trackNumber);
    overInt("discNumber",  discNumber,  consensus.discNumber,  consensus.confidence.discNumber,  confidence.discNumber);

    if ((anyAllowed || allowedFields.contains("artwork"))
        && artworkData.isEmpty() && !consensus.artworkData.isEmpty()) {
        artworkData = consensus.artworkData;
    }
}

bool AudioMetadata::canEnrichFrom(const AudioMetadata &other) const
{
    return (!hasTitle()                && other.hasTitle())
        || (!hasArtist()               && other.hasArtist())
        || (album.trimmed().isEmpty()  && !other.album.trimmed().isEmpty())
        || (genre.trimmed().isEmpty()  && !other.genre.trimmed().isEmpty())
        || (trackNumber == 0           && other.trackNumber > 0)
        || (discNumber  == 0           && other.discNumber  > 0)
        || (year        == 0           && other.year        > 0)
        || (artworkData.isEmpty()      && !other.artworkData.isEmpty())
        || (lyrics.trimmed().isEmpty() && !other.lyrics.trimmed().isEmpty());
}

} // namespace tagit

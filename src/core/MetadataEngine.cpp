#include "MetadataEngine.h"
#include "SettingsManager.h"
#include "Logger.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QThreadPool>
#include <QRunnable>

namespace tagit {

// ===========================================================================
// LocalEnrichWorker  — background thread
// ===========================================================================

QString LocalEnrichWorker::cleanForSearch(const QString &raw)
{
    QString s = raw;

    // Replace underscores/dots with spaces first so we can work with words
    s.replace('_', ' ');
    s.replace('.', ' ');

    // Step 1: Strip everything inside ( ) and [ ] — these always contain
    // download junk like (256k), (Official Audio), [Lyrics], etc.
    static const QRegularExpression parenRe(R"(\([^)]*\)|\[[^\]]*\])");
    s.remove(parenRe);

    // Step 2: Strip bitrate patterns that appear without brackets
    static const QRegularExpression bitrateRe(
        R"(\b\d{2,4}\s*kbps?\b)",
        QRegularExpression::CaseInsensitiveOption);
    s.remove(bitrateRe);

    // Step 3: Strip ONLY well-known download site names and file extensions
    // that appear as standalone words. Do NOT strip generic words like
    // "video", "audio", "lyrics", "hd", "hq" — these appear in real titles.
    static const QRegularExpression siteRe(
        R"(\b(downloadming|pagalworld|mr\.?jatt|djpunjab|songspk|bestwap|wapking|raagjatt|djjohal|youtube)\b)",
        QRegularExpression::CaseInsensitiveOption);
    s.remove(siteRe);

    // Step 4: Clean up resulting artefacts — multiple spaces, leading/trailing
    // punctuation left over after bracket removal.
    s = s.simplified();

    // Remove leading non-alphanumeric characters (leftover dashes, apostrophes, etc.)
    static const QRegularExpression leadingJunk(R"(^[^\w]+)");
    s.remove(leadingJunk);

    // Remove trailing exclamation/question marks that are download noise
    // but keep them if they look intentional (e.g. "What?!" as a song title
    // is fine, we only remove trailing lone punctuation).
    static const QRegularExpression trailingPunct(R"([!?]+$)");
    const QString trimmed = s.trimmed();
    // Only strip if the title is long enough that losing ! won't kill it
    if (trimmed.length() > 6) s = trimmed;

    // Truncate to 80 chars at a word boundary
    s = s.trimmed();
    if (s.length() > 80) {
        s = s.left(80);
        const int lastSpace = static_cast<int>(s.lastIndexOf(' '));
        if (lastSpace > 20) s = s.left(lastSpace);
    }
    return s.trimmed();
}

/// Returns true when the value looks like it came directly from the filename
/// rather than real embedded metadata — e.g. "Cruel_World_by_Tommee_Profitt"
/// or "This Song Will Make You Feel Like A Warrior!" (no artist set, or the
/// title contains underscores/parentheses typical of download filenames).
static bool looksLikeFilenameTag(const QString &value, const QString &fileStem)
{
    if (value.trimmed().isEmpty()) return true;

    // If the value is nearly identical to the file stem it was never tagged
    const QString normVal  = value.trimmed().toLower().replace('_', ' ').simplified();
    const QString normStem = fileStem.trimmed().toLower().replace('_', ' ').simplified();
    if (normVal == normStem) return true;

    // If the title contains underscores it was likely never cleaned
    if (value.contains('_')) return true;

    // If the title still contains a bitrate pattern it wasn't cleaned
    static const QRegularExpression bitrateInTitle(R"(\d{2,4}\s*k(bps?)?\b)",
        QRegularExpression::CaseInsensitiveOption);
    if (bitrateInTitle.match(value).hasMatch()) return true;

    return false;
}

class SongEnrichTask : public QRunnable {
public:
    SongEnrichTask(const Song &song, TagService *tagService, FilenameIntelligence *fi, LocalEnrichWorker *worker)
        : m_song(song), m_tagService(tagService), m_filenameIntelligence(fi), m_worker(worker) {}

    void run() override {
        AudioMetadata embedded = m_tagService->readTags(m_song.filePath);
        const AudioMetadata fromFilename = m_filenameIntelligence->parse(m_song.fileName);
        embedded.mergeMissing(fromFilename);

        Song result = m_song;
        result.metadata = embedded;

        const QString stem          = QFileInfo(m_song.filePath).completeBaseName();
        const QString cleanedStem   = LocalEnrichWorker::cleanForSearch(stem);

        QString searchArtist = embedded.artist.trimmed();
        QString searchTitle  = embedded.title.trimmed();

        const bool titleIsFilename  = looksLikeFilenameTag(searchTitle,  stem);
        const bool artistIsFilename = looksLikeFilenameTag(searchArtist, stem);
        const bool needsSearch      = !embedded.hasAllCoreFields() || titleIsFilename || artistIsFilename;

        if (!needsSearch) {
            QMetaObject::invokeMethod(m_worker, "songLocallyEnriched", Qt::QueuedConnection,
                                      Q_ARG(tagit::Song, result), Q_ARG(QString, QString()), Q_ARG(QString, QString()));
        } else {
            if (searchTitle.isEmpty() || titleIsFilename) {
                searchTitle = cleanedStem;
                searchArtist.clear();
            }

            if (!searchTitle.isEmpty() && searchArtist.isEmpty()) {
                static const QRegularExpression byRe(R"(^(.+?)\s+by\s+(.+)$)", QRegularExpression::CaseInsensitiveOption);
                const QRegularExpressionMatch m = byRe.match(searchTitle);
                if (m.hasMatch()) {
                    searchTitle  = m.captured(1).trimmed();
                    searchArtist = m.captured(2).trimmed();
                }
            }

            if (!searchTitle.isEmpty() && searchArtist.isEmpty()) {
                static const QRegularExpression pipeRe(R"(^(.+?)\s*[\|:]\s*(.+)$)");
                const QRegularExpressionMatch m = pipeRe.match(searchTitle);
                if (m.hasMatch()) {
                    searchArtist = m.captured(1).trimmed();
                    searchTitle  = m.captured(2).trimmed();
                }
            }

            if (searchTitle.isEmpty() && !cleanedStem.isEmpty()) searchTitle = cleanedStem;

            Logger::debug(QStringLiteral("Search query for '%1': artist='%2' title='%3'")
                              .arg(m_song.fileName, searchArtist, searchTitle));
            QMetaObject::invokeMethod(m_worker, "songLocallyEnriched", Qt::QueuedConnection,
                                      Q_ARG(tagit::Song, result), Q_ARG(QString, searchArtist), Q_ARG(QString, searchTitle));
        }
        QMetaObject::invokeMethod(m_worker, "taskDone", Qt::QueuedConnection);
    }
private:
    Song m_song;
    TagService *m_tagService;
    FilenameIntelligence *m_filenameIntelligence;
    LocalEnrichWorker *m_worker;
};

void LocalEnrichWorker::processBatch(const QVector<Song> &songs)
{
    m_tasksPending = static_cast<int>(songs.size());
    if (m_tasksPending == 0) {
        emit done();
        return;
    }
    
    for (const Song &song : songs) {
        auto *task = new SongEnrichTask(song, &m_tagService, &m_filenameIntelligence, this);
        QThreadPool::globalInstance()->start(task);
    }
}

void LocalEnrichWorker::taskDone()
{
    if (--m_tasksPending == 0) {
        emit done();
    }
}
// taskDone takes over the emission of done()

void LocalEnrichWorker::writeTags(const Song &song, bool doBackup)
{
    if (song.filePath.isEmpty()) return;

    const bool ok = m_tagService.writeMissingTags(
        song.filePath, song.metadata, doBackup);

    if (ok) {
        Logger::info(QStringLiteral("Tags written → %1").arg(song.filePath));
    } else {
        Logger::debug(QStringLiteral("No tag changes needed: %1").arg(song.filePath));
    }

    emit writeFinished(song.filePath, ok);
}

void LocalEnrichWorker::writeConsensusTags(const Song        &song,
                                            const QStringList &allowedFields,
                                            bool               doBackup)
{
    if (song.filePath.isEmpty()) return;

    // For consensus overwrites we use writeSelectedTags which WILL overwrite
    // fields that already have values (exactly what we want for wrong tags).
    const QStringList fields = allowedFields.isEmpty()
        ? QStringList{"title","artist","album","albumArtist","genre",
                      "composer","year","trackNumber","discNumber","lyrics"}
        : allowedFields;

    // Remove "artwork" from the list — writeSelectedTags handles text fields;
    // artwork embedding is handled separately via writeMissingTags.
    QStringList textFields;
    for (const QString &f : fields) {
        if (f != "artwork") textFields << f;
    }

    if (textFields.isEmpty()) { emit writeFinished(song.filePath, false); return; }

    if (doBackup) {
        const QString bak = song.filePath + ".bak";
        if (!QFile::exists(bak)) QFile::copy(song.filePath, bak);
    }

    const bool ok = m_tagService.writeSelectedTags(song.filePath,
                                                    song.metadata,
                                                    textFields);

    // Also embed artwork if present and not already embedded
    if (!song.metadata.artworkData.isEmpty()
        && !song.metadata.hasEmbeddedArtwork
        && (allowedFields.isEmpty() || allowedFields.contains("artwork"))) {
        m_tagService.writeMissingTags(song.filePath, song.metadata, false);
    }

    if (ok) {
        Logger::info(QStringLiteral("Consensus tags written → %1").arg(song.filePath));
    } else {
        Logger::debug(QStringLiteral("No consensus tag changes for: %1").arg(song.filePath));
    }

    emit writeFinished(song.filePath, ok);
}

// ===========================================================================
// MetadataEngine  — main thread
// ===========================================================================

MetadataEngine::MetadataEngine(SettingsManager *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    m_localWorker = new LocalEnrichWorker(nullptr);
    m_localWorker->moveToThread(&m_workerThread);

    connect(&m_workerThread, &QThread::finished,
            m_localWorker,   &QObject::deleteLater);
    connect(m_localWorker, &LocalEnrichWorker::songLocallyEnriched,
            this,          &MetadataEngine::onLocallyEnriched);
    connect(m_localWorker, &LocalEnrichWorker::done,
            this,          &MetadataEngine::onWorkerDone);
    connect(m_localWorker, &LocalEnrichWorker::writeFinished,
            this, [](const QString &, bool) {});  // just consume the signal

    m_workerThread.start();
}

MetadataEngine::~MetadataEngine()
{
    m_workerThread.quit();
    m_workerThread.wait(3000);
}

void MetadataEngine::setNetworkService(NetworkService *network)
{
    if (m_network) disconnect(m_network, nullptr, this, nullptr);
    m_network = network;
    if (m_network) {
        // Use the new aggregated signal (all providers voted)
        connect(m_network, &NetworkService::lookupAggregated,
                this,      &MetadataEngine::onLookupFinished);
        connect(m_network, &NetworkService::lookupFailed,
                this,      &MetadataEngine::onLookupFailed);
    }
}

void MetadataEngine::enrichSong(const Song &song) { enrichBatch({song}); }

void MetadataEngine::enrichBatch(const QVector<Song> &songs)
{
    if (songs.isEmpty()) { emit batchFinished(); return; }

    m_inBatch        = true;
    m_workerDone     = false;
    m_batchTotal     = static_cast<int>(songs.size());
    m_batchCompleted = 0;
    m_pending.clear();

    QMetaObject::invokeMethod(m_localWorker, "processBatch",
                              Qt::QueuedConnection,
                              Q_ARG(QVector<tagit::Song>, songs));
}

void MetadataEngine::reviewSong(const Song &song)
{
    if (!m_network) {
        emit reviewFailed(song.filePath, "Network service is unavailable.");
        return;
    }

    const QString stem = QFileInfo(song.filePath).completeBaseName();
    const QString cleanedStem = LocalEnrichWorker::cleanForSearch(stem);

    QString searchArtist = song.metadata.artist.trimmed();
    QString searchTitle  = song.metadata.title.trimmed();

    FilenameIntelligence fi;
    const AudioMetadata fromFn = fi.parse(song.fileName.isEmpty() ? QFileInfo(song.filePath).fileName() : song.fileName);

    if (searchArtist.isEmpty() && !fromFn.artist.isEmpty()) {
        searchArtist = fromFn.artist.trimmed();
    }
    if (searchTitle.isEmpty() && !fromFn.title.isEmpty()) {
        searchTitle = fromFn.title.trimmed();
    }

    if (searchTitle.isEmpty()) {
        searchTitle = cleanedStem;
    }

    // Try extracting "Title by Artist" or "Artist - Title"
    if (!searchTitle.isEmpty() && searchArtist.isEmpty()) {
        static const QRegularExpression byRe(R"(^(.+?)\s+by\s+(.+)$)", QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = byRe.match(searchTitle);
        if (m.hasMatch()) {
            searchTitle  = m.captured(1).trimmed();
            searchArtist = m.captured(2).trimmed();
        }
    }
    if (!searchTitle.isEmpty() && searchArtist.isEmpty()) {
        static const QRegularExpression pipeRe(R"(^(.+?)\s*[\|:\-–—~•]\s*(.+)$)");
        const QRegularExpressionMatch m = pipeRe.match(searchTitle);
        if (m.hasMatch()) {
            searchArtist = m.captured(1).trimmed();
            searchTitle  = m.captured(2).trimmed();
        }
    }

    // If artist is still empty, check parent folder (often "Artist Name - Album" or "Artist Name")
    if (searchArtist.isEmpty()) {
        const QDir parentDir = QFileInfo(song.filePath).dir();
        const QString parentName = parentDir.dirName();
        if (!parentName.isEmpty() && parentName.toLower() != "music" && parentName.toLower() != "downloads"
            && parentName.toLower() != "desktop" && parentName.toLower() != "songs") {
            const AudioMetadata parentMeta = fi.parse(parentName);
            if (!parentMeta.artist.isEmpty()) {
                searchArtist = parentMeta.artist.trimmed();
            }
        }
    }

    if (searchTitle.isEmpty()) searchTitle = stem;

    const QString key = searchArtist.trimmed() + QChar(0x1F) + searchTitle.trimmed();
    m_pendingReviews.insert(key, song);

    Logger::info(QStringLiteral("Online Review requested for: '%1' query: '%2 – %3'")
                     .arg(song.filePath, searchArtist, searchTitle));

    m_network->lookup(searchArtist, searchTitle);
}

void MetadataEngine::reviewSong(const QString &filePath, const AudioMetadata &metadata)
{
    Song song;
    song.filePath = filePath;
    song.fileName = QFileInfo(filePath).fileName();
    song.metadata = metadata;
    reviewSong(song);
}

void MetadataEngine::reviewBatch(const QVector<Song> &songs)
{
    if (songs.isEmpty()) {
        emit reviewBatchFinished(0, 0);
        return;
    }

    m_reviewBatchTotal     = static_cast<int>(songs.size());
    m_reviewBatchCompleted = 0;
    m_reviewBatchSuccess   = 0;
    m_reviewBatchFail      = 0;
    m_inReviewBatch        = true;

    emit reviewBatchProgress(0, m_reviewBatchTotal);

    for (const Song &song : songs) {
        reviewSong(song);
    }
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void MetadataEngine::onLocallyEnriched(const Song    &song,
                                        const QString &searchArtist,
                                        const QString &searchTitle)
{
    const bool onlineOn = m_network && m_network->isEnabled()
                          && m_settings && m_settings->allowOnlineLookup();

    // processBatch sends empty strings when no search is needed.
    // We also skip if online lookup is disabled.
    if (!onlineOn || (searchArtist.isEmpty() && searchTitle.isEmpty())) {
        finalise(song);
        return;
    }

    const QString key = searchArtist.trimmed() + QChar(0x1F) + searchTitle.trimmed();
    // Deduplicate: two songs with the same query → the second gets the
    // locally-enriched version; the first gets the online result.
    if (m_pending.contains(key)) {
        finalise(song);
        return;
    }

    m_pending.insert(key, song);
    m_network->lookup(searchArtist, searchTitle);
}

void MetadataEngine::onWorkerDone()
{
    m_workerDone = true;
    checkBatchComplete();
}

void MetadataEngine::onLookupFinished(const QString       &artist,
                                       const QString       &title,
                                       const AudioMetadata &onlineResult)
{
    const QString key = artist.trimmed() + QChar(0x1F) + title.trimmed();

    // Check if this was a user-requested force review
    if (m_pendingReviews.contains(key)) {
        const QList<Song> songs = m_pendingReviews.values(key);
        m_pendingReviews.remove(key);

        const QString artworkUrl = onlineResult.comment;
        AudioMetadata merged = onlineResult;
        merged.comment.clear();

        // If artwork was found and not yet downloaded, download it
        if (!artworkUrl.isEmpty() && merged.artworkData.isEmpty()) {
            for (const Song &s : songs) {
                m_pendingReviews.insert(key, s);
            }
            m_network->downloadArtwork(artworkUrl, artist, title, merged);
            return;
        }

        for (Song result : songs) {
            // Force overwrite non-empty fields onto result.metadata
            if (!merged.title.isEmpty())       result.metadata.title       = merged.title;
            if (!merged.artist.isEmpty())      result.metadata.artist      = merged.artist;
            if (!merged.album.isEmpty())       result.metadata.album       = merged.album;
            if (!merged.albumArtist.isEmpty()) result.metadata.albumArtist = merged.albumArtist;
            if (!merged.genre.isEmpty())       result.metadata.genre       = merged.genre;
            if (!merged.composer.isEmpty())    result.metadata.composer    = merged.composer;
            if (merged.year > 0)               result.metadata.year        = merged.year;
            if (merged.trackNumber > 0)        result.metadata.trackNumber = merged.trackNumber;
            if (merged.discNumber > 0)         result.metadata.discNumber  = merged.discNumber;
            if (!merged.lyrics.isEmpty())      result.metadata.lyrics      = merged.lyrics;
            if (!merged.isrc.isEmpty())        result.metadata.isrc        = merged.isrc;
            if (!merged.artworkData.isEmpty()) result.metadata.artworkData = merged.artworkData;
            result.metadata.confidence = merged.confidence;

            // Force write to disk (audio tags)
            TagService tagService;
            const QStringList allFields{"title","artist","album","albumArtist","genre",
                                        "composer","year","trackNumber","discNumber","lyrics"};
            const bool doBackup = m_settings && m_settings->createBackupsBeforeWrite();
            if (doBackup) {
                const QString bak = result.filePath + ".bak";
                if (!QFile::exists(bak)) QFile::copy(result.filePath, bak);
            }
            tagService.writeSelectedTags(result.filePath, result.metadata, allFields);

            if (!result.metadata.artworkData.isEmpty()) {
                tagService.writeArtwork(result.filePath, result.metadata.artworkData, "image/jpeg", false);
                result.metadata.hasEmbeddedArtwork = true;
            }

            Logger::info(QStringLiteral("Online Review successfully integrated: '%1 – %2' for %3")
                             .arg(result.metadata.artist, result.metadata.title, result.filePath));

            emit metadataUpdated(result.filePath, result.metadata);
            emit reviewFinished(result.filePath, result.metadata, true);

            m_reviewBatchSuccess++;
            m_reviewBatchCompleted++;
            if (m_inReviewBatch) {
                emit reviewBatchProgress(m_reviewBatchCompleted, m_reviewBatchTotal);
                if (m_reviewBatchCompleted >= m_reviewBatchTotal) {
                    m_inReviewBatch = false;
                    emit reviewBatchFinished(m_reviewBatchSuccess, m_reviewBatchFail);
                }
            }
        }
        return;
    }

    if (!m_pending.contains(key)) {
        Logger::warn(QStringLiteral("Unexpected lookup result for '%1 – %2'")
                         .arg(artist, title));
        if (m_inBatch) checkBatchComplete();
        return;
    }

    const QList<Song> songs = m_pending.values(key);
    m_pending.remove(key);

    // artworkUrl may be stashed in comment by one of the providers.
    // Clear it from the metadata before merging so it doesn't pollute tags.
    const QString artworkUrl = onlineResult.comment;
    AudioMetadata merged     = onlineResult;
    merged.comment.clear();

    const QStringList allowed   = m_settings ? m_settings->allowedTagFields()
                                              : QStringList{};
    const int         threshold = m_settings ? m_settings->metadataConfidenceThreshold()
                                              : 60;

    // If artwork was found and is needed, download it now
    if (!artworkUrl.isEmpty()
        && (allowed.isEmpty() || allowed.contains("artwork"))) {
        bool anyNeedsArtwork = false;
        for (const Song &s : songs) {
            if (!s.metadata.hasEmbeddedArtwork && s.metadata.artworkData.isEmpty()) {
                anyNeedsArtwork = true;
                break;
            }
        }
        if (anyNeedsArtwork && onlineResult.artworkData.isEmpty()) {
            for (const Song &s : songs) {
                m_pending.insert(key, s);
            }
            m_network->downloadArtwork(artworkUrl, artist, title, onlineResult);
            return;
        }
    }

    for (Song result : songs) {
        result.metadata.mergeWithConsensus(merged, threshold, allowed);

        Logger::info(QStringLiteral("Merged: '%1 – %2' album='%3' genre='%4' conf=%5")
                         .arg(result.metadata.artist, result.metadata.title,
                              result.metadata.album,  result.metadata.genre)
                         .arg(result.metadata.confidence.overall()));

        finalise(result);
    }
}

void MetadataEngine::onLookupFailed(const QString &artist,
                                     const QString &title,
                                     const QString &error)
{
    const QString key = artist.trimmed() + QChar(0x1F) + title.trimmed();

    if (m_pendingReviews.contains(key)) {
        const QList<Song> songs = m_pendingReviews.values(key);
        m_pendingReviews.remove(key);
        for (const Song &result : songs) {
            Logger::warn(QStringLiteral("Online Review failed for '%1 – %2' (%3): %4")
                             .arg(artist, title, result.filePath, error));
            emit reviewFailed(result.filePath, error);

            m_reviewBatchFail++;
            m_reviewBatchCompleted++;
            if (m_inReviewBatch) {
                emit reviewBatchProgress(m_reviewBatchCompleted, m_reviewBatchTotal);
                if (m_reviewBatchCompleted >= m_reviewBatchTotal) {
                    m_inReviewBatch = false;
                    emit reviewBatchFinished(m_reviewBatchSuccess, m_reviewBatchFail);
                }
            }
        }
        return;
    }

    if (!m_pending.contains(key)) {
        Logger::warn(QStringLiteral("Unexpected lookup failure for '%1 – %2': %3")
                         .arg(artist, title, error));
        if (m_inBatch) checkBatchComplete();
        return;
    }

    const QList<Song> songs = m_pending.values(key);
    m_pending.remove(key);

    for (Song result : songs) {
        Logger::debug(QStringLiteral("Lookup failed '%1 – %2': %3")
                          .arg(artist, title, error));
        emit enrichmentFailed(result.filePath, error);
        finalise(result);
    }
}

// ---------------------------------------------------------------------------
// finalise — confidence gate + write + emit
// ---------------------------------------------------------------------------

void MetadataEngine::finalise(const Song &song)
{
    // Write threshold: only write to disk when confidence >= this value.
    // Default 30 = "write if any single provider returned a confident match".
    // Raise this in Settings if you want stricter (e.g. 67 = 2/3 providers agree).
    const int threshold = m_settings
                          ? m_settings->metadataConfidenceThreshold()
                          : 30;
    const int conf = song.metadata.confidence.overall();

    const bool hasAnyOnlineResult = conf > 0;

    if (hasAnyOnlineResult && conf < threshold) {
        // Below threshold — show enriched data in the app but don't touch the file.
        Logger::info(QStringLiteral(
            "Shown only (confidence %1 < %2): %3 – %4")
                         .arg(conf).arg(threshold)
                         .arg(song.metadata.artist, song.metadata.title));
        emit songSkipped(song, conf, threshold);
    } else {
        // No online result (conf==0) → write whatever local steps found (non-destructive).
        // Has online result above threshold → write consensus (may overwrite wrong tags).
        const bool        doBackup = m_settings && m_settings->createBackupsBeforeWrite();
        const QStringList allowed  = m_settings ? m_settings->allowedTagFields()
                                                 : QStringList{};
        if (hasAnyOnlineResult) {
            // Consensus overwrite: correct wrong fields
            QMetaObject::invokeMethod(m_localWorker, "writeConsensusTags",
                                      Qt::QueuedConnection,
                                      Q_ARG(tagit::Song,   song),
                                      Q_ARG(QStringList,   allowed),
                                      Q_ARG(bool,          doBackup));
        } else {
            // No network result: non-destructive fill of missing fields only
            QMetaObject::invokeMethod(m_localWorker, "writeTags",
                                      Qt::QueuedConnection,
                                      Q_ARG(tagit::Song, song),
                                      Q_ARG(bool,        doBackup));
        }
    }

    // Always emit so the song appears in the table with whatever data we have.
    emit songEnriched(song);
    emit metadataUpdated(song.filePath, song.metadata);

    if (m_inBatch) { ++m_batchCompleted; checkBatchComplete(); }
}

void MetadataEngine::checkBatchComplete()
{
    if (!m_inBatch) return;
    if (m_workerDone && m_batchCompleted >= m_batchTotal && m_pending.isEmpty()) {
        m_inBatch    = false;
        m_batchTotal = 0;
        emit batchFinished();
    }
}

} // namespace tagit

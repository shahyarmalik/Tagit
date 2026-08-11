#ifndef TAGIT_METADATA_ENGINE_H
#define TAGIT_METADATA_ENGINE_H

#include <QObject>
#include <QHash>
#include <QThread>
#include <QVector>

#include "../model/AudioMetadata.h"
#include "../model/Song.h"
#include "../platform/TagService.h"
#include "../platform/NetworkService.h"
#include "../core/FilenameIntelligence.h"

namespace tagit {

class SettingsManager;

// ---------------------------------------------------------------------------
// LocalEnrichWorker — runs on a background thread.
//
// Responsibilities:
//   1. Read embedded tags + parse filename  (processBatch)
//   2. Write enriched tags back to files    (writeTags)
//      — both are synchronous disk I/O that must NOT block the GUI thread.
// ---------------------------------------------------------------------------
class LocalEnrichWorker : public QObject {
    Q_OBJECT
public:
    explicit LocalEnrichWorker(QObject *parent = nullptr) : QObject(parent) {}

    /// Produce a clean search query from a raw filename stem.
    /// Strips underscores, bitrate/quality tags, bracketed artefacts, etc.
    static QString cleanForSearch(const QString &raw);

public slots:
    /// Step 1/2 (local): reads tags + parses filename for every song in batch.
    void processBatch(const QVector<tagit::Song> &songs);

    /// Write-back for auto-enrichment: only fills fields that are currently empty.
    void writeTags(const tagit::Song &song, bool doBackup);

    /// Write-back for consensus overwrites: writes ALL fields in the metadata
    /// (overwriting whatever was there). Used after cross-provider vote.
    void writeConsensusTags(const tagit::Song &song,
                            const QStringList &allowedFields,
                            bool               doBackup);

    void taskDone();

signals:
    void songLocallyEnriched(const tagit::Song &song,
                             const QString &searchArtist,
                             const QString &searchTitle);
    void done();
    void writeFinished(const QString &filePath, bool ok);

private:
    int m_tasksPending = 0;

    TagService           m_tagService;
    FilenameIntelligence m_filenameIntelligence;
};

// ---------------------------------------------------------------------------
// MetadataEngine — lives on the main thread, owns QNetworkAccessManager.
// ---------------------------------------------------------------------------
class MetadataEngine : public QObject {
    Q_OBJECT
public:
    explicit MetadataEngine(SettingsManager *settings, QObject *parent = nullptr);
    ~MetadataEngine() override;

    void setNetworkService(NetworkService *network);

    void enrichSong(const Song &song);
    void enrichBatch(const QVector<Song> &songs);

    /// Force review a song over the internet and overwrite tags regardless of existing metadata.
    void reviewSong(const Song &song);
    void reviewSong(const QString &filePath, const AudioMetadata &metadata);
    void reviewBatch(const QVector<Song> &songs);

signals:
    void songEnriched(const tagit::Song &enriched);
    void songSkipped(const tagit::Song &song, int confidence, int threshold);
    void batchFinished();
    void metadataUpdated(const QString &filePath, const tagit::AudioMetadata &metadata);
    void enrichmentFailed(const QString &filePath, const QString &error);
    void reviewFinished(const QString &filePath, const tagit::AudioMetadata &metadata, bool success);
    void reviewFailed(const QString &filePath, const QString &error);
    void reviewBatchProgress(int completed, int total);
    void reviewBatchFinished(int successCount, int failCount);

private slots:
    void onLocallyEnriched(const Song &song,
                           const QString &searchArtist,
                           const QString &searchTitle);
    void onWorkerDone();
    void onLookupFinished(const QString &artist, const QString &title,
                          const AudioMetadata &onlineResult);
    void onLookupFailed(const QString &artist, const QString &title,
                        const QString &error);

private:
    void finalise(const Song &song);   // merge done → write tags → emit
    void checkBatchComplete();

    SettingsManager      *m_settings    = nullptr;
    NetworkService       *m_network     = nullptr;

    QThread               m_workerThread;
    LocalEnrichWorker    *m_localWorker = nullptr;

    QMultiHash<QString, Song>  m_pending;        // key = "artist\x1ftitle" for batch enrichment
    QMultiHash<QString, Song>  m_pendingReviews; // key = "artist\x1ftitle" for force-reviews

    int  m_batchTotal     = 0;
    int  m_batchCompleted = 0;
    bool m_inBatch        = false;
    bool m_workerDone     = false;

    int  m_reviewBatchTotal     = 0;
    int  m_reviewBatchCompleted = 0;
    int  m_reviewBatchSuccess   = 0;
    int  m_reviewBatchFail      = 0;
    bool m_inReviewBatch        = false;
};

} // namespace tagit

#endif // TAGIT_METADATA_ENGINE_H

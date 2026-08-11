#include "LibraryManager.h"
#include "Logger.h"
#include "MetadataEngine.h"
#include "SettingsManager.h"
#include "../platform/DatabaseService.h"
#include "../platform/FilesystemService.h"

#include <QFileInfo>

namespace tagit {

// ---------------------------------------------------------------------------
// LibraryScanWorker
// ---------------------------------------------------------------------------

void LibraryScanWorker::scan(const QString &path)
{
    const QStringList extensions{
        ".mp3", ".flac", ".ogg", ".opus", ".m4a",
        ".aac", ".wav",  ".wma", ".aiff", ".dsf",
        ".ape", ".mpc",  ".wv",  ".tta"
    };

    FilesystemService fs;
    const QStringList files = fs.scanForAudioFiles(path, extensions);

    const int total   = static_cast<int>(files.size());
    int       scanned = 0;
    int       batchCount = 0;

    if (m_db) {
        m_db->beginTransaction();
    }

    for (const QString &filePath : files) {
        if (m_cancelled && m_cancelled->load()) {
            break;
        }

        Song song;
        song.filePath     = filePath;
        song.fileName     = QFileInfo(filePath).fileName();
        song.format       = FilesystemService::fileExtension(filePath);
        song.fileSize     = QFileInfo(filePath).size();
        song.modifiedTime = QFileInfo(filePath).lastModified();

        if (m_db) {
            song.id = m_db->upsertSong(song);
        }

        ++scanned;
        ++batchCount;
        if (batchCount >= 100 && m_db) {
            m_db->commitTransaction();
            m_db->beginTransaction();
            batchCount = 0;
        }

        emit trackFound(song);

        ScanProgress p;
        p.filesScanned = scanned;
        p.totalFiles   = total;
        p.currentFile  = filePath;
        emit progress(p);
    }

    if (m_db) {
        m_db->commitTransaction();
    }

    emit finished(m_cancelled && m_cancelled->load());
}

// ---------------------------------------------------------------------------
// LibraryManager
// ---------------------------------------------------------------------------

LibraryManager::LibraryManager(SettingsManager *settings,
                                DatabaseService *db,
                                QObject         *parent)
    : ILibraryManager(parent)
    , m_settings(settings)
    , m_db(db)
{
    m_worker = new LibraryScanWorker(nullptr);
    m_worker->setDatabase(m_db);
    m_worker->setCancelled(&m_cancelled);
    m_worker->moveToThread(&m_thread);

    connect(m_worker, &LibraryScanWorker::trackFound,
            this,     &LibraryManager::trackAdded);

    // Also collect every scanned song so we can batch-enrich after the scan.
    connect(m_worker, &LibraryScanWorker::trackFound,
            this, [this](const Song &song) {
                m_scannedSongs.append(song);
            });

    connect(m_worker, &LibraryScanWorker::progress,
            this,     &ILibraryManager::scanProgress);

    connect(m_worker, &LibraryScanWorker::finished,
            this,     &LibraryManager::onWorkerFinished);

    connect(&m_thread, &QThread::finished,
            m_worker,  &QObject::deleteLater);

    m_thread.start();
}

LibraryManager::~LibraryManager()
{
    m_cancelled = true;
    m_thread.quit();
    m_thread.wait(5000);
}

void LibraryManager::setMetadataEngine(MetadataEngine *engine)
{
    if (m_metadataEngine) {
        disconnect(m_metadataEngine, nullptr, this, nullptr);
    }
    m_metadataEngine = engine;
    if (m_metadataEngine) {
        connect(m_metadataEngine, &MetadataEngine::songEnriched,
                this, &LibraryManager::onSongEnriched);
        connect(m_metadataEngine, &MetadataEngine::batchFinished,
                this, &LibraryManager::onEnrichmentBatchFinished);
    }
}

void LibraryManager::scanDirectory(const QString &path)
{
    if (m_scanning) {
        Logger::warn("Scan already running; ignoring request");
        return;
    }
    if (path.isEmpty()) {
        emit scanFinished(false, QStringLiteral("No folder selected"));
        return;
    }

    m_scannedSongs.clear();
    m_scanning  = true;
    m_cancelled = false;
    emit scanStarted(path);

    QMetaObject::invokeMethod(m_worker, "scan",
                              Qt::QueuedConnection,
                              Q_ARG(QString, path));
}

void LibraryManager::cancelScan()
{
    m_cancelled = true;
}

void LibraryManager::onWorkerFinished(bool cancelled)
{
    m_scanning = false;

    if (cancelled) {
        m_scannedSongs.clear();
        emit scanFinished(false, QStringLiteral("Cancelled"));
        return;
    }

    Logger::info(QStringLiteral("Scan complete — %1 tracks found")
                     .arg(m_scannedSongs.size()));

    emit scanFinished(true, QStringLiteral("Complete"));

    // Kick off background enrichment if a MetadataEngine is wired in.
    if (m_metadataEngine && !m_scannedSongs.isEmpty()) {
        Logger::info("Starting metadata enrichment batch…");
        m_metadataEngine->enrichBatch(m_scannedSongs);
    }
}

void LibraryManager::onSongEnriched(const Song &enriched)
{
    // Persist updated metadata to the database and notify the UI.
    if (m_db) {
        m_db->upsertSong(enriched);
    }
    // Re-emit as trackAdded so the song table refreshes in real time.
    emit trackAdded(enriched);
}

void LibraryManager::onEnrichmentBatchFinished()
{
    Logger::info("Enrichment batch complete");
}

QVector<Song> LibraryManager::allTracks() const
{
    if (m_db) {
        return m_db->allSongs();
    }
    return {};
}

int LibraryManager::trackCount() const
{
    return static_cast<int>(allTracks().size());
}

void LibraryManager::clearLibrary()
{
    if (m_db) {
        m_db->clear();
    }
    emit libraryCleared();
}

} // namespace tagit

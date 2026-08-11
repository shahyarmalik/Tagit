#ifndef TAGIT_LIBRARY_MANAGER_H
#define TAGIT_LIBRARY_MANAGER_H

#include <atomic>

#include <QThread>
#include <QVector>

#include "../model/Song.h"
#include "ILibraryManager.h"

namespace tagit {

class SettingsManager;
class DatabaseService;
class MetadataEngine;

// ---------------------------------------------------------------------------
// Internal worker — lives on its own QThread, communicates via signals.
// ---------------------------------------------------------------------------
class LibraryScanWorker : public QObject {
    Q_OBJECT
public:
    explicit LibraryScanWorker(QObject *parent = nullptr) : QObject(parent) {}

    void setDatabase(DatabaseService *db)    { m_db        = db; }
    void setCancelled(std::atomic<bool> *f)  { m_cancelled = f;  }

public slots:
    void scan(const QString &path);

signals:
    void trackFound(const tagit::Song &track);
    void progress(const tagit::ScanProgress &p);
    void finished(bool cancelled);

private:
    DatabaseService   *m_db        = nullptr;
    std::atomic<bool> *m_cancelled = nullptr;
};

// ---------------------------------------------------------------------------
// LibraryManager — public API, owns the worker thread.
// ---------------------------------------------------------------------------
class LibraryManager : public ILibraryManager {
    Q_OBJECT
public:
    LibraryManager(SettingsManager *settings, DatabaseService *db,
                   QObject *parent = nullptr);
    ~LibraryManager() override;

    /// Attach the MetadataEngine so scanned tracks are auto-enriched.
    void setMetadataEngine(MetadataEngine *engine);

    void scanDirectory(const QString &path) override;
    void cancelScan()                        override;
    QVector<Song> allTracks()          const override;
    int           trackCount()         const override;
    void          clearLibrary()             override;

private slots:
    void onWorkerFinished(bool cancelled);
    void onSongEnriched(const Song &enriched);
    void onEnrichmentBatchFinished();

private:
    SettingsManager   *m_settings       = nullptr;
    DatabaseService   *m_db             = nullptr;
    MetadataEngine    *m_metadataEngine = nullptr;

    QThread            m_thread;
    LibraryScanWorker *m_worker         = nullptr;

    std::atomic<bool>  m_scanning{false};
    std::atomic<bool>  m_cancelled{false};

    // Songs collected during a scan, waiting to be enriched.
    QVector<Song>      m_scannedSongs;
};

} // namespace tagit

#endif // TAGIT_LIBRARY_MANAGER_H

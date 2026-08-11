#ifndef TAGIT_MAIN_WINDOW_H
#define TAGIT_MAIN_WINDOW_H

#include <QMainWindow>
#include <QMap>
#include <QString>
#include <QStringList>

class QCheckBox;
class QDockWidget;
class QFrame;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QAction;
class QMenu;
class QTableView;
class QTabWidget;
class QTimer;
class QElapsedTimer;
class QTreeView;
class QWidget;

namespace tagit {

class IPlatformServices;
class SettingsManager;
class ILibraryManager;
class ApplicationCore;
class SongTableModel;
class MetadataInspector;
class ArtworkInspector;
class ActivityLogView;
class TagService;
class PlayerWidget;
class DatabaseService;
class MetadataEngine;
struct Song;
struct AudioMetadata;

/**
 * @brief Primary application window.
 *
 * Central widget layout (top → bottom):
 *   ① Search bar    — live filter
 *   ② Song table    — sortable, multi-select
 *
 * Docks:
 *   Left   — Library Browser
 *   Right  — Inspector (Metadata + Artwork tabs)
 *   Bottom — Activity Log
 */
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(IPlatformServices *platform,
                        SettingsManager   *settings,
                        QWidget           *parent = nullptr);
    ~MainWindow() override;

    void setLibraryManager(ILibraryManager *libraryManager);
    void setApplicationCore(ApplicationCore *core);
    void showWelcomeMessage();

public slots:
    void onOpenFolder();

private slots:
    void onAbout();
    void onSettings();
    void onScanProgress(const QString &currentFile, int scanned, int total);
    void onScanFinished(bool success, const QString &message);
    void onTrackAdded(const Song &track);
    void onSearchChanged(const QString &query);
    void flushPendingSongs();

    // Enrichment progress tracking
    void onSongEnrichedResult(const tagit::Song &song);
    void onSongSkipped(const tagit::Song &song, int conf, int thresh);
    void onEnrichmentFailed(const QString &filePath, const QString &error);
    void updateProgressUI(const QString &currentFile);

    /// Save from the inspector panel (single song).
    void onInspectorSave(const QString       &filePath,
                         const QString       &editedFileName,
                         const AudioMetadata &metadata,
                         const QStringList   &fields);

    /// Force review for a single song requested from the inspector or context menu.
    void onReviewSong(const QString &filePath, const AudioMetadata &metadata);
    void onReviewSelectedRequested();
    void onBulkReviewAllRequested();
    void onReviewBatchProgress(int completed, int total);
    void onReviewBatchFinished(int successCount, int failCount);
    void onReviewFinished(const QString &filePath, const tagit::AudioMetadata &metadata, bool success);
    void onReviewFailed(const QString &filePath, const QString &error);
    void onBulkCleanAllRequested();
    void onBulkCleanSelectedRequested();
    void onSongSelectionChanged();
    void onSongTableContextMenu(const QPoint &pos);
    void onSongDoubleClicked(const QModelIndex &index);

    // Player slots
    void onPlayerPreviousRequested();
    void onPlayerNextRequested();
    void playTrackAtIndex(int row);

    /// Called when artwork has been added, changed, or removed in the inspector.
    void onArtworkChanged(const QString &filePath, bool hasArtwork);

private:
    void setupUi();
    void setupMenuBar();
    void setupAppToolBar();    // top toolbar (open, scan, settings)
    void setupStatusBar();
    void setupCentralWidget();
    void setupConnections();
    void setupPlayerShortcuts();
    void refreshSongTable();

    /// Clean filename and synchronize metadata tags for a single song.
    bool cleanSong(Song &song);

    /// Write @p fields from @p metadata to @p filePath, update DB + table.
    bool saveTagsToFile(const QString       &filePath,
                        const AudioMetadata &metadata,
                        const QStringList   &fields);

    // ---- Dock widgets ----
    QDockWidget *m_folderDock    = nullptr;
    QDockWidget *m_inspectorDock = nullptr;
    QDockWidget *m_logDock       = nullptr;

    // ---- Inner widgets ----
    QTreeView         *m_folderTree    = nullptr;
    QTableView        *m_songTable     = nullptr;
    QTabWidget        *m_inspectorTabs = nullptr;
    QLabel            *m_statusLabel   = nullptr;
    QProgressBar      *m_progressBar   = nullptr;
    QLineEdit         *m_searchBar     = nullptr;

    SongTableModel    *m_songModel        = nullptr;
    MetadataInspector *m_inspector        = nullptr;
    ArtworkInspector  *m_artworkInspector  = nullptr;
    ActivityLogView   *m_logView          = nullptr;
    PlayerWidget      *m_playerWidget     = nullptr;

    // ---- Top toolbar / Menu actions ----
    QAction *m_openFolderAction          = nullptr;
    QAction *m_settingsAction            = nullptr;
    QAction *m_aboutAction               = nullptr;
    QAction *m_quitAction                = nullptr;
    QAction *m_scanAction                = nullptr;
    QAction *m_bulkCleanAllAction        = nullptr;
    QAction *m_bulkCleanSelectedAction   = nullptr;
    QAction *m_reviewSelectedAction      = nullptr;
    QAction *m_bulkReviewAllAction       = nullptr;

    // ---- Core services ----
    IPlatformServices *m_platform       = nullptr;
    SettingsManager   *m_settings       = nullptr;
    ILibraryManager   *m_libraryManager = nullptr;
    TagService        *m_tagService     = nullptr;
    DatabaseService   *m_db             = nullptr;
    MetadataEngine    *m_metadataEngine = nullptr;

    // ---- Enrichment state ----
    int            m_enrichedCount = 0;
    int            m_successCount  = 0;
    int            m_skipCount     = 0;
    int            m_failCount     = 0;
    int            m_totalCount    = 0;
    QElapsedTimer *m_timeTracker   = nullptr;
    QTimer        *m_refreshTimer  = nullptr;
    QVector<Song>  m_pendingSongs;

    // ---- Player state ----
    int            m_currentPlayingRow    = -1;
    QVector<int>   m_shuffleHistory;
    int            m_shuffleHistoryIndex  = -1;
};

} // namespace tagit

#endif // TAGIT_MAIN_WINDOW_H

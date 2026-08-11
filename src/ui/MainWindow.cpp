#include "MainWindow.h"
#include "../core/ILibraryManager.h"
#include "../core/SettingsManager.h"
#include "../core/ApplicationCore.h"
#include "../core/Logger.h"
#include "../core/SearchEngine.h"
#include "../core/MetadataEngine.h"
#include "../model/Song.h"
#include "../platform/IPlatformServices.h"
#include "../platform/TagService.h"
#include "../platform/DatabaseService.h"
#include "SongTableModel.h"
#include "MetadataInspector.h"
#include "ArtworkInspector.h"
#include "ActivityLogView.h"
#include "SettingsDialog.h"
#include "PlayerWidget.h"
#include "../core/FilenameIntelligence.h"

#include <QElapsedTimer>
#include <QApplication>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QRandomGenerator>
#include <QShortcut>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableView>
#include <QTimer>
#include <QToolBar>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWidget>

namespace tagit {

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

MainWindow::MainWindow(IPlatformServices *platform,
                       SettingsManager   *settings,
                       QWidget           *parent)
    : QMainWindow(parent)
    , m_platform(platform)
    , m_settings(settings)
{
    m_timeTracker = new QElapsedTimer();
    Logger::info("Initializing main window");
    setWindowTitle("TagIt \u2014 Music Library Manager");
    resize(1280, 800);

    setupUi();
    setupMenuBar();
    setupAppToolBar();
    setupStatusBar();
    setupCentralWidget();   // builds m_saveToolbar + song table inside central
    setupConnections();

    Logger::info("Main window initialized");
}

MainWindow::~MainWindow()
{
    delete m_timeTracker;
    Logger::info("Main window destroyed");
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void MainWindow::setApplicationCore(ApplicationCore *core)
{
    if (!core) return;
    m_tagService = core->tagService();
    m_db         = core->database();
    if (m_inspector) {
        connect(m_inspector, &MetadataInspector::saveRequested,
                this, &MainWindow::onInspectorSave, Qt::UniqueConnection);
        connect(m_inspector, &MetadataInspector::reviewRequested,
                this, &MainWindow::onReviewSong, Qt::UniqueConnection);
        connect(m_inspector, &MetadataInspector::multiReviewRequested,
                this, &MainWindow::onReviewSelectedRequested, Qt::UniqueConnection);
        connect(m_inspector, &MetadataInspector::multiCleanRequested,
                this, &MainWindow::onBulkCleanSelectedRequested, Qt::UniqueConnection);
    }
    if (m_artworkInspector) {
        m_artworkInspector->setTagService(m_tagService);
    }
    MetadataEngine *engine = core->metadataEngine();
    m_metadataEngine = engine;
    if (engine) {
        connect(engine, &MetadataEngine::songEnriched,
                this, &MainWindow::onSongEnrichedResult);
        connect(engine, &MetadataEngine::songSkipped,
                this, &MainWindow::onSongSkipped);
        connect(engine, &MetadataEngine::enrichmentFailed,
                this, &MainWindow::onEnrichmentFailed);
        connect(engine, &MetadataEngine::reviewFinished,
                this, &MainWindow::onReviewFinished);
        connect(engine, &MetadataEngine::reviewFailed,
                this, &MainWindow::onReviewFailed);
        connect(engine, &MetadataEngine::reviewBatchProgress,
                this, &MainWindow::onReviewBatchProgress);
        connect(engine, &MetadataEngine::reviewBatchFinished,
                this, &MainWindow::onReviewBatchFinished);
        connect(engine, &MetadataEngine::batchFinished,
                this, [this]() {
                    if (m_totalCount > 0 && m_enrichedCount >= m_totalCount) {
                        m_progressBar->setVisible(false);
                        m_statusLabel->setText(QString("%1 tracks ready").arg(m_totalCount));
                    } else {
                        m_statusLabel->setText("Tagging complete");
                        m_progressBar->setVisible(false);
                    }
                });
    }
}

void MainWindow::setLibraryManager(ILibraryManager *mgr)
{
    m_libraryManager = mgr;
    if (!m_libraryManager) return;
    connect(m_libraryManager, &ILibraryManager::scanProgress,
            this, [this](const ScanProgress &p){
                onScanProgress(p.currentFile, p.filesScanned, p.totalFiles);
            });
    connect(m_libraryManager, &ILibraryManager::scanFinished,
            this, &MainWindow::onScanFinished);
    connect(m_libraryManager, &ILibraryManager::trackAdded,
            this, &MainWindow::onTrackAdded);
}

void MainWindow::showWelcomeMessage()
{
    m_statusLabel->setText(
        "Welcome to TagIt! Use File \u2192 Open Music Folder to get started.");
    Logger::info("Welcome message shown");
}

bool MainWindow::saveTagsToFile(const QString       &filePath,
                                 const AudioMetadata &metadata,
                                 const QStringList   &fields)
{
    if (!m_tagService || filePath.isEmpty() || fields.isEmpty()) return false;

    const bool ok = m_tagService->writeSelectedTags(filePath, metadata, fields);
    if (ok) {
        Logger::info("Tags saved: " + filePath);
        m_logView->appendEntry(
            QString("\u270F [%1] \u2192 %2")
                .arg(fields.join(", "), QFileInfo(filePath).fileName()));

        // Refresh the DB row from disk so the table shows the updated tags
        if (m_db) {
            const AudioMetadata fresh = m_tagService->readTags(filePath);
            if (m_libraryManager) {
                const QVector<Song> all = m_libraryManager->allTracks();
                for (Song s : all) {
                    if (s.filePath == filePath) {
                        s.metadata = fresh;
                        m_db->upsertSong(s);
                        break;
                    }
                }
            }
        }
    } else {
        Logger::warn("Tag save failed (nothing changed?) for: " + filePath);
        m_logView->appendEntry(
            QString("\u26A0 Save failed: %1").arg(QFileInfo(filePath).fileName()));
    }
    return ok;
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void MainWindow::onOpenFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, "Select Music Library",
        m_platform ? m_platform->defaultMusicPath() : QString());
    if (dir.isEmpty()) return;

    Logger::info("User selected library folder: " + dir);
    m_statusLabel->setText("Scanning: " + dir);
    m_searchBar->clear();
    m_enrichedCount = 0;
    m_successCount  = 0;
    m_skipCount     = 0;
    m_failCount     = 0;
    m_totalCount    = 0;
    m_pendingSongs.clear();
    if (m_refreshTimer) m_refreshTimer->stop();

    if (m_libraryManager) {
        m_libraryManager->scanDirectory(dir);
        m_progressBar->setMaximum(0);
        m_progressBar->setVisible(true);
        m_progressBar->setValue(0);
    }
}

void MainWindow::onAbout()
{
    QMessageBox::about(this, "About TagIt",
        "<h2>TagIt " TAGIT_VERSION "</h2>"
        "<p><b>Intelligent Cross-Platform Music Library Manager</b></p>"
        "<p>A native desktop application that intelligently organises, "
        "enriches, and manages local music libraries.</p>"
        "<p>Built with C++20, Qt 6, TagLib, SQLite and spdlog.</p>");
}

void MainWindow::onSettings()
{
    SettingsDialog dialog(m_settings, this);
    dialog.exec();
}

void MainWindow::onScanProgress(const QString &currentFile, int scanned, int total)
{
    m_statusLabel->setText("Scanning: " + currentFile);
    if (total > 0) {
        m_totalCount = total;
        m_progressBar->setMaximum(total);
        m_progressBar->setValue(scanned);
    }
}

void MainWindow::onScanFinished(bool success, const QString &message)
{
    if (!success) {
        m_statusLabel->setText("Scan failed: " + message);
        m_progressBar->setVisible(false);
        refreshSongTable();
        return;
    }
    const int count = m_libraryManager ? m_libraryManager->trackCount() : 0;
    m_statusLabel->setText(QString("Starting tagging of %1 tracks\u2026").arg(count));
    m_progressBar->setVisible(true);
    m_progressBar->setMaximum(count);
    m_progressBar->setValue(0);
    m_enrichedCount = 0;
    m_successCount  = 0;
    m_skipCount     = 0;
    m_failCount     = 0;
    m_totalCount    = count;
    if (m_totalCount > 0) {
        m_timeTracker->start();
    }
    refreshSongTable();
}

void MainWindow::onTrackAdded(const Song &track)
{
    m_pendingSongs.append(track);
    if (m_refreshTimer && !m_refreshTimer->isActive()) m_refreshTimer->start();
}

void MainWindow::onSongEnrichedResult(const tagit::Song &song)
{
    ++m_successCount;
    m_logView->appendEntry(QString("\u2713 %1 \u2014 %2").arg(song.metadata.artist, song.metadata.title));
    onTrackAdded(song);
    updateProgressUI(song.fileName);
}

void MainWindow::onSongSkipped(const tagit::Song &song, int /*conf*/, int /*thresh*/)
{
    ++m_skipCount;
    m_logView->appendEntry(QString("\u21BA Skipped: %1").arg(song.fileName));
    updateProgressUI(song.fileName);
}

void MainWindow::onEnrichmentFailed(const QString &filePath, const QString &error)
{
    ++m_failCount;
    m_logView->appendEntry(QString("\u2717 Failed: %1 (%2)").arg(QFileInfo(filePath).fileName(), error));
    updateProgressUI(QFileInfo(filePath).fileName());
}

void MainWindow::updateProgressUI(const QString &currentFile)
{
    m_enrichedCount = m_successCount + m_skipCount + m_failCount;
    if (m_totalCount > 0) {
        m_progressBar->setVisible(true);
        m_progressBar->setMaximum(m_totalCount);
        m_progressBar->setValue(m_enrichedCount);

        QString etaStr = "Calculating...";
        if (m_enrichedCount > 0) {
            qint64 elapsedMs = m_timeTracker->elapsed();
            double msPerItem = static_cast<double>(elapsedMs) / m_enrichedCount;
            qint64 remainingMs = static_cast<qint64>(msPerItem * (m_totalCount - m_enrichedCount));
            int seconds = static_cast<int>((remainingMs / 1000) % 60);
            int minutes = static_cast<int>(remainingMs / 60000);
            etaStr = QString("%1m %2s").arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
        }

        int percent = (m_totalCount > 0) ? (m_enrichedCount * 100 / m_totalCount) : 0;
        m_statusLabel->setText(
            QString("Tagging: %1% (Success: %2 | Skipped: %3 | Failed: %4) - %5 - ETA: %6")
                .arg(percent)
                .arg(m_successCount)
                .arg(m_skipCount)
                .arg(m_failCount)
                .arg(currentFile)
                .arg(etaStr));
    }

    if (m_totalCount > 0 && m_enrichedCount >= m_totalCount) {
        flushPendingSongs();
        m_progressBar->setVisible(false);
        m_statusLabel->setText(QString("%1 tracks ready").arg(m_totalCount));
    }
}

void MainWindow::flushPendingSongs()
{
    if (m_pendingSongs.isEmpty()) return;
    const QString query = m_searchBar ? m_searchBar->text() : QString();
    if (query.trimmed().isEmpty()) {
        for (const Song &s : m_pendingSongs) {
            m_songModel->addOrUpdateSong(s);
        }
    } else {
        if (m_libraryManager) {
            SearchEngine e;
            m_songModel->setSongs(e.search(m_libraryManager->allTracks(), query));
        }
    }
    m_pendingSongs.clear();
}

void MainWindow::onSearchChanged(const QString &query)
{
    if (!m_libraryManager) return;
    const QVector<Song> all = m_libraryManager->allTracks();
    if (query.trimmed().isEmpty()) {
        m_songModel->setSongs(all);
        m_statusLabel->setText(QString::number(all.size()) + " tracks");
    } else {
        SearchEngine e;
        const QVector<Song> r = e.search(all, query);
        m_songModel->setSongs(r);
        m_statusLabel->setText(
            QString::number(r.size()) + " of " + QString::number(all.size()) + " tracks match");
    }
}

void MainWindow::onArtworkChanged(const QString &filePath, bool hasArtwork)
{
    m_logView->appendEntry(
        QString(hasArtwork ? "\U0001F5BC Artwork updated: %1" : "\U0001F5BC Artwork removed: %1")
            .arg(QFileInfo(filePath).fileName()));

    if (m_tagService && m_db && m_libraryManager) {
        const AudioMetadata fresh = m_tagService->readTags(filePath);
        const QVector<Song> all = m_libraryManager->allTracks();
        for (Song s : all) {
            if (s.filePath == filePath) {
                s.metadata = fresh;
                m_db->upsertSong(s);
                break;
            }
        }
    }
    refreshSongTable();
}

void MainWindow::onInspectorSave(const QString       &filePath,
                                 const QString       &editedFileName,
                                 const AudioMetadata &metadata,
                                 const QStringList   &fields)
{
    QString currentPath = filePath;
    QString targetFileName = editedFileName;

    // Check if filename has changed
    const QString originalFileName = QFileInfo(filePath).fileName();
    if (!targetFileName.isEmpty() && targetFileName != originalFileName) {
        // Construct new path
        QDir dir = QFileInfo(filePath).dir();
        QString newPath = dir.absoluteFilePath(targetFileName);

        // Check if destination already exists to prevent overwrite
        if (QFile::exists(newPath)) {
            QMessageBox::warning(this, tr("Rename Failed"),
                                 tr("A file with the name '%1' already exists.").arg(targetFileName));
            return;
        }

        // Release handle if the player is currently playing or loaded this file
        if (m_playerWidget && m_playerWidget->currentFilePath() == filePath) {
            m_playerWidget->stopAndRelease();
        }

        // Rename on disk
        if (QFile::rename(filePath, newPath)) {
            // Update database record path and file name
            if (m_db) {
                m_db->updateSongPath(filePath, newPath, targetFileName);
            }
            m_logView->appendEntry(QString("Rename: %1 -> %2").arg(originalFileName, targetFileName));
            currentPath = newPath;
        } else {
            QMessageBox::warning(this, tr("Rename Failed"),
                                 tr("Could not rename the file on disk. Make sure the file is not locked."));
            return;
        }
    }

    // Now save any selected fields to the file (using the updated path if renamed)
    if (!fields.isEmpty()) {
        if (saveTagsToFile(currentPath, metadata, fields)) {
            m_logView->appendEntry(QString("Saved tags for: %1").arg(QFileInfo(currentPath).fileName()));
        }
    }

    // Reload the freshly written tags back into the inspector and refresh table
    if (m_tagService) {
        const AudioMetadata reloaded = m_tagService->readTags(currentPath);
        m_inspector->setSong(currentPath, reloaded);
        m_artworkInspector->setSong(currentPath, reloaded);
    }
    refreshSongTable();
}

void MainWindow::onReviewSong(const QString &filePath, const AudioMetadata &metadata)
{
    if (!m_metadataEngine) {
        if (m_inspector) m_inspector->showStatusMessage(tr("Review engine not available"), true);
        return;
    }

    if (m_inspector && m_inspector->filePath() == filePath) {
        m_inspector->setReviewing(true, tr("Reviewing online over YouTube, iTunes, MusicBrainz, Deezer..."));
    }

    const QString fileName = QFileInfo(filePath).fileName();
    m_logView->appendEntry(QString("🌐 Online review started for: %1").arg(fileName));
    m_statusLabel->setText(QString("Reviewing '%1' online...").arg(fileName));

    m_metadataEngine->reviewSong(filePath, metadata);
}

void MainWindow::onReviewSelectedRequested()
{
    if (!m_songTable || !m_songModel || !m_metadataEngine) return;

    QModelIndexList selected = m_songTable->selectionModel()->selectedRows();
    QVector<Song> songsToReview;

    if (selected.isEmpty()) {
        int row = m_songTable->currentIndex().row();
        if (row >= 0 && row < m_songModel->rowCount()) {
            songsToReview.append(m_songModel->songAt(row));
        } else if (m_inspector && !m_inspector->filePath().isEmpty()) {
            Song s;
            s.filePath = m_inspector->filePath();
            s.metadata = m_inspector->metadata();
            songsToReview.append(s);
        }
    } else {
        for (const QModelIndex &idx : selected) {
            if (idx.row() >= 0 && idx.row() < m_songModel->rowCount()) {
                songsToReview.append(m_songModel->songAt(idx.row()));
            }
        }
    }

    if (songsToReview.isEmpty()) return;

    if (songsToReview.size() == 1) {
        onReviewSong(songsToReview[0].filePath, songsToReview[0].metadata);
    } else {
        m_logView->appendEntry(QString("🌐 Starting batch online review for %1 tracks...").arg(songsToReview.size()));
        m_statusLabel->setText(tr("Reviewing %1 tracks online...").arg(songsToReview.size()));
        m_progressBar->setMaximum(static_cast<int>(songsToReview.size()));
        m_progressBar->setValue(0);
        m_progressBar->setVisible(true);
        if (m_inspector) {
            m_inspector->setReviewing(true, tr("Reviewing %1 tracks online...").arg(songsToReview.size()));
        }
        m_metadataEngine->reviewBatch(songsToReview);
    }
}

void MainWindow::onBulkReviewAllRequested()
{
    if (!m_libraryManager || !m_metadataEngine) return;
    const QVector<Song> all = m_libraryManager->allTracks();
    if (all.isEmpty()) {
        QMessageBox::information(this, tr("Bulk Review Online"), tr("Library is empty. Open a music folder first."));
        return;
    }

    const auto reply = QMessageBox::question(
        this,
        tr("Bulk Review Entire Library"),
        tr("This will query online providers (YouTube, iTunes, MusicBrainz, Deezer) for all %1 tracks in your library and overwrite their metadata tags.\n\nDo you wish to proceed?").arg(all.size()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    m_logView->appendEntry(QString("🌐 Starting bulk online review for entire library (%1 tracks)...").arg(all.size()));
    m_statusLabel->setText(tr("Reviewing entire library (%1 tracks) online...").arg(all.size()));
    m_progressBar->setMaximum(static_cast<int>(all.size()));
    m_progressBar->setValue(0);
    m_progressBar->setVisible(true);
    if (m_inspector) {
        m_inspector->setReviewing(true, tr("Reviewing %1 tracks online...").arg(all.size()));
    }
    m_metadataEngine->reviewBatch(all);
}

void MainWindow::onReviewBatchProgress(int completed, int total)
{
    if (total > 0) {
        m_progressBar->setMaximum(total);
        m_progressBar->setValue(completed);
        m_progressBar->setVisible(true);
        m_statusLabel->setText(tr("Online review: %1 / %2 tracks processed...").arg(completed).arg(total));
    }
}

void MainWindow::onReviewBatchFinished(int successCount, int failCount)
{
    m_progressBar->setVisible(false);
    const QString msg = tr("✓ Batch online review complete: %1 succeeded, %2 failed.").arg(successCount).arg(failCount);
    m_statusLabel->setText(msg);
    m_logView->appendEntry(msg);
    if (m_inspector) {
        m_inspector->setReviewing(false, msg);
    }
    refreshSongTable();
}

void MainWindow::onSongSelectionChanged()
{
    if (!m_songTable || !m_songModel) return;

    QModelIndexList selectedRows = m_songTable->selectionModel()->selectedRows();
    QVector<Song> selectedSongs;
    for (const QModelIndex &idx : selectedRows) {
        if (idx.row() >= 0 && idx.row() < m_songModel->rowCount()) {
            selectedSongs.append(m_songModel->songAt(idx.row()));
        }
    }

    const int count = static_cast<int>(selectedSongs.size());
    if (m_inspector) {
        m_inspector->setSelection(selectedSongs);
    }
    if (m_artworkInspector) {
        if (count == 1) {
            m_artworkInspector->setSong(selectedSongs[0].filePath, selectedSongs[0].metadata);
        } else {
            m_artworkInspector->clear();
        }
    }

    if (m_reviewSelectedAction) {
        if (count == 0) {
            m_reviewSelectedAction->setText(tr("🌐 Review Selected Online"));
            m_reviewSelectedAction->setEnabled(false);
        } else if (count == 1) {
            m_reviewSelectedAction->setText(tr("🌐 Review Selected Track Online"));
            m_reviewSelectedAction->setEnabled(true);
        } else {
            m_reviewSelectedAction->setText(tr("🌐 Review %1 Selected Tracks Online").arg(count));
            m_reviewSelectedAction->setEnabled(true);
        }
    }

    if (m_bulkCleanSelectedAction) {
        if (count == 0) {
            m_bulkCleanSelectedAction->setText(tr("✨ Clean Selected"));
            m_bulkCleanSelectedAction->setEnabled(false);
        } else if (count == 1) {
            m_bulkCleanSelectedAction->setText(tr("✨ Clean Selected Track"));
            m_bulkCleanSelectedAction->setEnabled(true);
        } else {
            m_bulkCleanSelectedAction->setText(tr("✨ Clean %1 Selected Tracks").arg(count));
            m_bulkCleanSelectedAction->setEnabled(true);
        }
    }
}

void MainWindow::onReviewFinished(const QString &filePath, const tagit::AudioMetadata &metadata, bool success)
{
    Q_UNUSED(success);
    const QString fileName = QFileInfo(filePath).fileName();

    if (m_inspector && m_inspector->filePath() == filePath) {
        m_inspector->setSong(filePath, metadata);
        m_inspector->showStatusMessage(tr("✓ Online review complete! Tags & artwork integrated."));
    }
    if (m_artworkInspector && m_artworkInspector->filePath() == filePath) {
        m_artworkInspector->setSong(filePath, metadata);
    }

    // Update database record
    if (m_db && m_libraryManager) {
        const QVector<Song> all = m_libraryManager->allTracks();
        for (Song s : all) {
            if (s.filePath == filePath) {
                s.metadata = metadata;
                m_db->upsertSong(s);
                break;
            }
        }
    }

    m_logView->appendEntry(QString("✓ Successfully reviewed and updated tags for: %1").arg(fileName));
    m_statusLabel->setText(QString("Updated tags for %1").arg(fileName));
    refreshSongTable();
}

void MainWindow::onReviewFailed(const QString &filePath, const QString &error)
{
    const QString fileName = QFileInfo(filePath).fileName();
    if (m_inspector && m_inspector->filePath() == filePath) {
        m_inspector->setReviewing(false);
        m_inspector->showStatusMessage(tr("⚠️ Review failed: %1").arg(error), true);
    }
    m_logView->appendEntry(QString("⚠️ Online review failed for %1: %2").arg(fileName, error));
    m_statusLabel->setText(QString("Review failed for %1: %2").arg(fileName, error));
}

void MainWindow::onSongTableContextMenu(const QPoint &pos)
{
    QModelIndex index = m_songTable->indexAt(pos);
    if (!index.isValid()) return;

    int selCount = static_cast<int>(m_songTable->selectionModel()->selectedRows().count());
    if (selCount <= 1) selCount = 1;

    QMenu menu(this);
    const QString cleanTitle = (selCount > 1) 
        ? QString("✨ Clean %1 Selected Songs (Filename & Tags)").arg(selCount)
        : QString("✨ Clean Selected (Filename & Tags)");
    QAction *actClean = menu.addAction(cleanTitle);
    actClean->setToolTip("Standardise filenames and synchronise parsed tags for selected track(s)");
    connect(actClean, &QAction::triggered, this, &MainWindow::onBulkCleanSelectedRequested);

    const QString reviewTitle = (selCount > 1)
        ? QString("🌐 Review %1 Selected Songs Online (Force Re-tag)").arg(selCount)
        : QString("🌐 Review Online Metadata (Force Re-tag)");
    QAction *actReview = menu.addAction(reviewTitle);
    actReview->setToolTip("Query online providers and integrate consensus metadata into selected track(s)");
    connect(actReview, &QAction::triggered, this, &MainWindow::onReviewSelectedRequested);

    menu.addSeparator();
    QAction *actSave = menu.addAction("💾 Save Current Tags");
    connect(actSave, &QAction::triggered, this, [this, index]() {
        if (index.row() >= 0 && index.row() < m_songModel->rowCount()) {
            const Song &s = m_songModel->songAt(index.row());
            saveTagsToFile(s.filePath, s.metadata, {"title","artist","album","albumArtist","genre","year","trackNumber","discNumber","lyrics","composer"});
        }
    });

    menu.exec(m_songTable->viewport()->mapToGlobal(pos));
}

bool MainWindow::cleanSong(Song &song)
{
    FilenameIntelligence fi;
    const QString originalFileName = QFileInfo(song.filePath).fileName();
    const QString cleanedFileName = fi.cleanFilename(originalFileName);
    const AudioMetadata parsed = fi.parse(cleanedFileName);

    bool changed = false;
    QString currentPath = song.filePath;

    // 1. Rename file on disk if filename changed
    if (!cleanedFileName.isEmpty() && cleanedFileName != originalFileName) {
        QDir dir = QFileInfo(song.filePath).dir();
        QString newPath = dir.absoluteFilePath(cleanedFileName);

        if (QFile::exists(newPath) && newPath != song.filePath) {
            Logger::warn("Cannot rename to existing file: " + newPath);
        } else {
            // Release handle if the player is currently playing or loaded this file
            if (m_playerWidget && m_playerWidget->currentFilePath() == song.filePath) {
                m_playerWidget->stopAndRelease();
            }

            if (QFile::rename(song.filePath, newPath)) {
                if (m_db) {
                    m_db->updateSongPath(song.filePath, newPath, cleanedFileName);
                }
                song.filePath = newPath;
                song.fileName = cleanedFileName;
                currentPath = newPath;
                changed = true;
            } else {
                Logger::warn("Failed to rename file: " + song.filePath);
            }
        }
    }

    // 2. Update and synchronize metadata tags
    QStringList fieldsToUpdate;
    if (!parsed.title.isEmpty() && song.metadata.title != parsed.title) {
        song.metadata.title = parsed.title;
        fieldsToUpdate << QStringLiteral("title");
    }
    if (!parsed.artist.isEmpty() && song.metadata.artist != parsed.artist) {
        song.metadata.artist = parsed.artist;
        fieldsToUpdate << QStringLiteral("artist");
    }
    if (parsed.trackNumber > 0 && song.metadata.trackNumber != parsed.trackNumber) {
        song.metadata.trackNumber = parsed.trackNumber;
        fieldsToUpdate << QStringLiteral("trackNumber");
    }

    if (!fieldsToUpdate.isEmpty()) {
        if (m_tagService) {
            m_tagService->writeSelectedTags(currentPath, song.metadata, fieldsToUpdate);
        }
        if (m_db) {
            m_db->upsertSong(song);
        }
        changed = true;
    } else if (changed && m_db) {
        m_db->upsertSong(song);
    }

    return changed;
}

void MainWindow::onBulkCleanAllRequested()
{
    if (!m_libraryManager) return;
    QVector<Song> all = m_libraryManager->allTracks();
    if (all.isEmpty()) {
        QMessageBox::information(this, tr("Bulk Clean"), tr("Library is empty. Open a music folder first."));
        return;
    }

    auto reply = QMessageBox::question(
        this, tr("Bulk Clean All Songs"),
        tr("Are you sure you want to clean and standardise filenames and metadata tags for all %1 song(s) in the library?")
            .arg(all.size()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (reply != QMessageBox::Yes) return;

    m_progressBar->setVisible(true);
    m_progressBar->setMaximum(static_cast<int>(all.size()));
    m_progressBar->setValue(0);
    m_statusLabel->setText(tr("Cleaning library tracks..."));

    int cleanedCount = 0;
    QString inspectedPath = m_inspector ? m_inspector->filePath() : QString();

    for (int i = 0; i < all.size(); ++i) {
        Song s = all[i];
        const QString oldPath = s.filePath;
        const QString oldName = s.fileName;
        if (cleanSong(s)) {
            ++cleanedCount;
            m_logView->appendEntry(QString("✨ Cleaned: %1 \u2192 %2").arg(oldName, s.fileName));
            if (!inspectedPath.isEmpty() && inspectedPath == oldPath && m_inspector) {
                m_inspector->setSong(s.filePath, s.metadata);
                if (m_artworkInspector) m_artworkInspector->setSong(s.filePath, s.metadata);
                inspectedPath = s.filePath;
            }
        }
        m_progressBar->setValue(i + 1);
        m_statusLabel->setText(tr("Cleaning: %1/%2 tracks (%3 cleaned)").arg(i + 1).arg(all.size()).arg(cleanedCount));
        qApp->processEvents();
    }

    m_progressBar->setVisible(false);
    m_statusLabel->setText(tr("Bulk clean complete: %1 of %2 tracks cleaned.").arg(cleanedCount).arg(all.size()));
    m_logView->appendEntry(QString("✨ Bulk clean completed: %1 of %2 tracks updated.").arg(cleanedCount).arg(all.size()));
    refreshSongTable();
}

void MainWindow::onBulkCleanSelectedRequested()
{
    if (!m_songTable || !m_songModel) return;

    QModelIndexList selected = m_songTable->selectionModel()->selectedRows();
    QVector<int> rows;
    if (selected.isEmpty()) {
        int row = m_songTable->currentIndex().row();
        if (row >= 0) {
            rows.append(row);
        } else if (m_inspector && !m_inspector->filePath().isEmpty()) {
            if (m_libraryManager) {
                QVector<Song> all = m_libraryManager->allTracks();
                for (Song s : all) {
                    if (s.filePath == m_inspector->filePath()) {
                        const QString oldPath = s.filePath;
                        const QString oldName = s.fileName;
                        if (cleanSong(s)) {
                            m_logView->appendEntry(QString("✨ Cleaned: %1 \u2192 %2").arg(oldName, s.fileName));
                            m_inspector->setSong(s.filePath, s.metadata);
                            if (m_artworkInspector) m_artworkInspector->setSong(s.filePath, s.metadata);
                            refreshSongTable();
                        }
                        return;
                    }
                }
            }
            return;
        } else {
            return;
        }
    } else {
        for (const QModelIndex &idx : selected) {
            if (idx.row() >= 0 && idx.row() < m_songModel->rowCount()) {
                rows.append(idx.row());
            }
        }
    }

    if (rows.isEmpty()) return;

    m_progressBar->setVisible(true);
    m_progressBar->setMaximum(static_cast<int>(rows.size()));
    m_progressBar->setValue(0);

    int cleanedCount = 0;
    QString inspectedPath = m_inspector ? m_inspector->filePath() : QString();

    for (int i = 0; i < rows.size(); ++i) {
        int row = rows[i];
        Song s = m_songModel->songAt(row);
        const QString oldPath = s.filePath;
        const QString oldName = s.fileName;
        if (cleanSong(s)) {
            ++cleanedCount;
            m_logView->appendEntry(QString("✨ Cleaned: %1 \u2192 %2").arg(oldName, s.fileName));
            if (!inspectedPath.isEmpty() && inspectedPath == oldPath && m_inspector) {
                m_inspector->setSong(s.filePath, s.metadata);
                if (m_artworkInspector) m_artworkInspector->setSong(s.filePath, s.metadata);
                inspectedPath = s.filePath;
            }
        }
        m_progressBar->setValue(i + 1);
        m_statusLabel->setText(tr("Cleaning: %1/%2 selected tracks (%3 cleaned)").arg(i + 1).arg(rows.size()).arg(cleanedCount));
        qApp->processEvents();
    }

    m_progressBar->setVisible(false);
    m_statusLabel->setText(tr("Clean complete: %1 of %2 selected tracks cleaned.").arg(cleanedCount).arg(rows.size()));
    m_logView->appendEntry(QString("✨ Cleaned %1 of %2 selected tracks.").arg(cleanedCount).arg(rows.size()));
    refreshSongTable();
}

// ---------------------------------------------------------------------------
// UI Setup
// ---------------------------------------------------------------------------

void MainWindow::setupUi() { setDockNestingEnabled(true); }

void MainWindow::setupMenuBar()
{
    QMenu *fileMenu = menuBar()->addMenu("&File");
    m_openFolderAction = fileMenu->addAction("&Open Music Folder\u2026");
    m_openFolderAction->setShortcut(QKeySequence::Open);
    connect(m_openFolderAction, &QAction::triggered, this, &MainWindow::onOpenFolder);
    fileMenu->addSeparator();
    m_quitAction = fileMenu->addAction("&Quit");
    m_quitAction->setShortcut(QKeySequence::Quit);
    connect(m_quitAction, &QAction::triggered, qApp, &QApplication::quit);

    menuBar()->addMenu("&View");   // populated in setupCentralWidget()

    QMenu *toolsMenu = menuBar()->addMenu("&Tools");
    m_scanAction = toolsMenu->addAction("&Scan Library");
    m_scanAction->setShortcut(QKeySequence("Ctrl+R"));
    connect(m_scanAction, &QAction::triggered, this, &MainWindow::onOpenFolder);

    m_bulkCleanAllAction = toolsMenu->addAction("✨ &Bulk Clean All Songs");
    m_bulkCleanAllAction->setShortcut(QKeySequence("Ctrl+Shift+C"));
    m_bulkCleanAllAction->setToolTip("Standardise filenames and tags for all tracks in library");
    connect(m_bulkCleanAllAction, &QAction::triggered, this, &MainWindow::onBulkCleanAllRequested);

    m_bulkCleanSelectedAction = toolsMenu->addAction("✨ Clean Selected Songs");
    m_bulkCleanSelectedAction->setShortcut(QKeySequence("Ctrl+Alt+C"));
    m_bulkCleanSelectedAction->setToolTip("Standardise filenames and tags for selected tracks");
    connect(m_bulkCleanSelectedAction, &QAction::triggered, this, &MainWindow::onBulkCleanSelectedRequested);

    toolsMenu->addSeparator();

    m_bulkReviewAllAction = toolsMenu->addAction("🌐 &Bulk Review All Songs Online");
    m_bulkReviewAllAction->setShortcut(QKeySequence("Ctrl+Shift+U"));
    m_bulkReviewAllAction->setToolTip("Force online metadata review and retagging for all songs in the library");
    connect(m_bulkReviewAllAction, &QAction::triggered, this, &MainWindow::onBulkReviewAllRequested);

    m_reviewSelectedAction = toolsMenu->addAction("🌐 &Review Selected Songs Online");
    m_reviewSelectedAction->setShortcut(QKeySequence("Ctrl+Shift+R"));
    m_reviewSelectedAction->setToolTip("Query online providers and integrate metadata for selected tracks");
    connect(m_reviewSelectedAction, &QAction::triggered, this, &MainWindow::onReviewSelectedRequested);

    toolsMenu->addSeparator();
    m_settingsAction = toolsMenu->addAction("&Settings\u2026");
    m_settingsAction->setShortcut(QKeySequence("Ctrl+,"));
    connect(m_settingsAction, &QAction::triggered, this, &MainWindow::onSettings);

    QMenu *helpMenu = menuBar()->addMenu("&Help");
    m_aboutAction = helpMenu->addAction("&About TagIt");
    connect(m_aboutAction, &QAction::triggered, this, &MainWindow::onAbout);
}

void MainWindow::setupAppToolBar()
{
    QToolBar *tb = addToolBar("Main");
    tb->setMovable(false);
    tb->addAction(m_openFolderAction);
    tb->addAction(m_scanAction);
    tb->addSeparator();
    tb->addAction(m_reviewSelectedAction);
    tb->addAction(m_bulkCleanSelectedAction);
    tb->addSeparator();
    tb->addAction(m_bulkReviewAllAction);
    tb->addAction(m_bulkCleanAllAction);
    tb->addSeparator();
    tb->addAction(m_settingsAction);
}

void MainWindow::setupStatusBar()
{
    m_statusLabel = new QLabel("Ready");
    m_progressBar = new QProgressBar();
    m_progressBar->setVisible(false);
    m_progressBar->setMaximumWidth(220);
    m_progressBar->setTextVisible(false);
    statusBar()->addWidget(m_statusLabel, 1);
    statusBar()->addPermanentWidget(m_progressBar);
}

void MainWindow::setupCentralWidget()
{
    // ---- Left dock ----
    m_folderDock = new QDockWidget("Library Browser", this);
    m_folderDock->setObjectName("folderDock");
    m_folderDock->setFeatures(QDockWidget::DockWidgetMovable
                              | QDockWidget::DockWidgetFloatable
                              | QDockWidget::DockWidgetClosable);
    m_folderTree = new QTreeView(m_folderDock);
    m_folderTree->setHeaderHidden(true);
    m_folderTree->setIndentation(16);
    m_folderTree->setAnimated(true);
    auto *fsModel = new QFileSystemModel(m_folderTree);
    fsModel->setRootPath(QDir::homePath());
    fsModel->setFilter(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Drives);
    m_folderTree->setModel(fsModel);
    m_folderTree->setRootIndex(fsModel->index(QDir::homePath()));
    for (int c = 1; c < fsModel->columnCount(); ++c) m_folderTree->hideColumn(c);
    m_folderDock->setWidget(m_folderTree);
    addDockWidget(Qt::LeftDockWidgetArea, m_folderDock);

    connect(m_folderTree, &QTreeView::doubleClicked, this,
            [this, fsModel](const QModelIndex &idx) {
                if (!fsModel->isDir(idx)) return;
                const QString path = fsModel->filePath(idx);
                m_statusLabel->setText("Scanning: " + path);
                m_searchBar->clear();
                m_enrichedCount = 0; m_successCount = 0; m_skipCount = 0; m_failCount = 0; m_totalCount = 0;
                m_pendingSongs.clear();
                if (m_refreshTimer) m_refreshTimer->stop();
                if (m_libraryManager) {
                    m_libraryManager->scanDirectory(path);
                    m_progressBar->setMaximum(0);
                    m_progressBar->setVisible(true);
                }
            });

    // ---- Right dock ----
    m_inspectorDock = new QDockWidget("Inspector", this);
    m_inspectorDock->setObjectName("inspectorDock");
    m_inspectorDock->setFeatures(QDockWidget::DockWidgetMovable
                                 | QDockWidget::DockWidgetFloatable
                                 | QDockWidget::DockWidgetClosable);
    m_inspectorTabs = new QTabWidget(m_inspectorDock);
    m_inspector     = new MetadataInspector(m_inspectorTabs);
    m_inspectorTabs->addTab(m_inspector, "Metadata");
    m_artworkInspector = new ArtworkInspector(m_inspectorTabs);
    if (m_tagService) {
        m_artworkInspector->setTagService(m_tagService);
    }
    connect(m_artworkInspector, &ArtworkInspector::artworkChanged,
            this, &MainWindow::onArtworkChanged);
    m_inspectorTabs->addTab(m_artworkInspector, "Artwork");
    m_inspectorDock->setWidget(m_inspectorTabs);
    addDockWidget(Qt::RightDockWidgetArea, m_inspectorDock);

    // ---- Bottom dock ----
    m_logDock = new QDockWidget("Activity Log", this);
    m_logDock->setObjectName("logDock");
    m_logDock->setFeatures(QDockWidget::DockWidgetMovable
                           | QDockWidget::DockWidgetFloatable
                           | QDockWidget::DockWidgetClosable);
    m_logView = new ActivityLogView(m_logDock);
    m_logDock->setWidget(m_logView);
    addDockWidget(Qt::BottomDockWidgetArea, m_logDock);

    // ---- Central widget ----
    auto *central       = new QWidget(this);
    auto *centralLayout = new QVBoxLayout(central);
    centralLayout->setContentsMargins(4, 4, 4, 0);
    centralLayout->setSpacing(4);

    // ① Search bar
    auto *searchRow = new QHBoxLayout();
    searchRow->setSpacing(6);
    auto *searchIcon = new QLabel("\U0001F50D", central);
    searchRow->addWidget(searchIcon);
    m_searchBar = new QLineEdit(central);
    m_searchBar->setPlaceholderText("Search title, artist, album, genre\u2026");
    m_searchBar->setClearButtonEnabled(true);
    searchRow->addWidget(m_searchBar, 1);
    centralLayout->addLayout(searchRow);

    // ② Song table
    m_songModel = new SongTableModel(this);
    m_songTable = new QTableView(central);
    m_songTable->setModel(m_songModel);
    m_songTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_songTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_songTable->setAlternatingRowColors(true);
    m_songTable->setSortingEnabled(true);
    m_songTable->setShowGrid(false);
    m_songTable->verticalHeader()->hide();
    m_songTable->horizontalHeader()->setStretchLastSection(true);
    m_songTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_songTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_songTable, &QTableView::customContextMenuRequested,
            this, &MainWindow::onSongTableContextMenu);
    centralLayout->addWidget(m_songTable, 1);

    // ③ Music Player
    m_playerWidget = new PlayerWidget(central);
    centralLayout->addWidget(m_playerWidget);

    setCentralWidget(central);

    // Update inspector when selection changes
    connect(m_songTable->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this]() {
                onSongSelectionChanged();
            });

    // Also update on current row change
    connect(m_songTable->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, [this](const QModelIndex &cur, const QModelIndex &) {
                if (cur.isValid() && m_songTable->selectionModel()->selectedRows().count() <= 1) {
                    onSongSelectionChanged();
                }
            });

    // Ctrl+F focuses the search bar
    auto *focusSearch = new QShortcut(QKeySequence("Ctrl+F"), this);
    connect(focusSearch, &QShortcut::activated, this,
            [this]{ m_searchBar->setFocus(); m_searchBar->selectAll(); });

    // ---- Populate View menu ----
    QMenu *viewMenu = nullptr;
    for (QAction *a : menuBar()->actions()) {
        if (a->text() == "&View") { viewMenu = a->menu(); break; }
    }
    if (viewMenu) {
        viewMenu->addSection("Panels");
        QAction *showLib  = m_folderDock->toggleViewAction();
        QAction *showIns  = m_inspectorDock->toggleViewAction();
        QAction *showLog  = m_logDock->toggleViewAction();
        showLib->setText("&Library Browser");  showLib->setShortcut(QKeySequence("Ctrl+1"));
        showIns->setText("&Inspector");        showIns->setShortcut(QKeySequence("Ctrl+2"));
        showLog->setText("&Activity Log");     showLog->setShortcut(QKeySequence("Ctrl+3"));
        viewMenu->addAction(showLib);
        viewMenu->addAction(showIns);
        viewMenu->addAction(showLog);
        viewMenu->addSeparator();
        QAction *resetLayout = viewMenu->addAction("&Reset Layout");
        connect(resetLayout, &QAction::triggered, this, [this]() {
            m_folderDock->setFloating(false);    addDockWidget(Qt::LeftDockWidgetArea,   m_folderDock);    m_folderDock->show();
            m_inspectorDock->setFloating(false); addDockWidget(Qt::RightDockWidgetArea,  m_inspectorDock); m_inspectorDock->show();
            m_logDock->setFloating(false);       addDockWidget(Qt::BottomDockWidgetArea, m_logDock);       m_logDock->show();
        });
    }
}

void MainWindow::setupConnections()
{
    connect(m_searchBar, &QLineEdit::textChanged, this, &MainWindow::onSearchChanged);
    connect(m_inspector, &MetadataInspector::saveRequested,
            this, &MainWindow::onInspectorSave);
    connect(m_songTable, &QTableView::doubleClicked, this, &MainWindow::onSongDoubleClicked);
    
    // Wire PlayerWidget signals
    if (m_playerWidget) {
        connect(m_playerWidget, &PlayerWidget::previousRequested, this, &MainWindow::onPlayerPreviousRequested);
        connect(m_playerWidget, &PlayerWidget::nextRequested, this, &MainWindow::onPlayerNextRequested);
        connect(m_playerWidget, &PlayerWidget::stopRequested, this, [this]() {
            m_statusLabel->setText(tr("Playback stopped"));
        });
        connect(m_playerWidget, &PlayerWidget::playRequested, this, [this]() {
            if (m_currentPlayingRow < 0 && m_songModel && m_songModel->rowCount() > 0) {
                playTrackAtIndex(0);
            }
        });
    }

    setupPlayerShortcuts();

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(400);
    m_refreshTimer->setSingleShot(false);
    connect(m_refreshTimer, &QTimer::timeout, this, &MainWindow::flushPendingSongs);
}

void MainWindow::setupPlayerShortcuts()
{
    if (!m_playerWidget) return;

    // Previous Track: Ctrl+Left
    auto *scPrev = new QShortcut(QKeySequence("Ctrl+Left"), this);
    connect(scPrev, &QShortcut::activated, this, &MainWindow::onPlayerPreviousRequested);

    // Next Track: Ctrl+Right
    auto *scNext = new QShortcut(QKeySequence("Ctrl+Right"), this);
    connect(scNext, &QShortcut::activated, this, &MainWindow::onPlayerNextRequested);

    // Volume Up: Ctrl+Up
    auto *scVolUp = new QShortcut(QKeySequence("Ctrl+Up"), this);
    connect(scVolUp, &QShortcut::activated, this, [this]() {
        if (m_playerWidget) m_playerWidget->volumeUp(5);
    });

    // Volume Down: Ctrl+Down
    auto *scVolDown = new QShortcut(QKeySequence("Ctrl+Down"), this);
    connect(scVolDown, &QShortcut::activated, this, [this]() {
        if (m_playerWidget) m_playerWidget->volumeDown(5);
    });

    // Mute: Ctrl+M
    auto *scMute = new QShortcut(QKeySequence("Ctrl+M"), this);
    connect(scMute, &QShortcut::activated, this, [this]() {
        if (m_playerWidget) m_playerWidget->toggleMute();
    });

    // Seek -5s: Shift+Left
    auto *scRewind = new QShortcut(QKeySequence("Shift+Left"), this);
    connect(scRewind, &QShortcut::activated, this, [this]() {
        if (m_playerWidget) m_playerWidget->seekRelative(-5000);
    });

    // Seek +5s: Shift+Right
    auto *scForward = new QShortcut(QKeySequence("Shift+Right"), this);
    connect(scForward, &QShortcut::activated, this, [this]() {
        if (m_playerWidget) m_playerWidget->seekRelative(5000);
    });

    // Play/Pause: Space (when table has focus)
    auto *scSpace = new QShortcut(QKeySequence(Qt::Key_Space), m_songTable);
    scSpace->setContext(Qt::WidgetWithChildrenShortcut);
    connect(scSpace, &QShortcut::activated, this, [this]() {
        if (m_playerWidget) m_playerWidget->togglePlayPause();
    });
}

void MainWindow::refreshSongTable()
{
    if (!m_libraryManager) return;
    const QString query = m_searchBar ? m_searchBar->text() : QString();
    if (query.trimmed().isEmpty()) {
        m_songModel->setSongs(m_libraryManager->allTracks());
    } else {
        SearchEngine e;
        m_songModel->setSongs(e.search(m_libraryManager->allTracks(), query));
    }
}

void MainWindow::onSongDoubleClicked(const QModelIndex &index)
{
    if (!index.isValid() || !m_songModel || !m_playerWidget) return;
    const int row = index.row();
    if (row >= 0 && row < m_songModel->rowCount()) {
        m_shuffleHistory.clear();
        m_shuffleHistory.append(row);
        m_shuffleHistoryIndex = 0;
        playTrackAtIndex(row);
    }
}

void MainWindow::playTrackAtIndex(int row)
{
    if (!m_songModel || !m_playerWidget) return;
    const int total = m_songModel->rowCount();
    if (row < 0 || row >= total) return;

    m_currentPlayingRow = row;
    const Song &s = m_songModel->songAt(row);
    m_playerWidget->playSong(s.filePath, s.metadata.title, s.metadata.artist);

    // Highlight and scroll to the active track
    if (m_songTable) {
        m_songTable->selectRow(row);
        m_songTable->scrollTo(m_songModel->index(row, 0), QAbstractItemView::EnsureVisible);
    }
}

void MainWindow::onPlayerPreviousRequested()
{
    if (!m_songModel || !m_playerWidget) return;
    const int total = m_songModel->rowCount();
    if (total == 0) return;

    // If shuffle is active, navigate back in history if available
    if (m_playerWidget->isShuffleEnabled()) {
        if (m_shuffleHistoryIndex > 0 && m_shuffleHistoryIndex < m_shuffleHistory.size()) {
            --m_shuffleHistoryIndex;
            const int prevRow = m_shuffleHistory[m_shuffleHistoryIndex];
            if (prevRow >= 0 && prevRow < total) {
                playTrackAtIndex(prevRow);
                return;
            }
        }
    }

    // Sequential previous
    int prevRow = (m_currentPlayingRow >= 0) ? (m_currentPlayingRow - 1) : 0;
    if (prevRow < 0) {
        if (m_playerWidget->repeatMode() == PlayerWidget::RepeatMode::All) {
            prevRow = total - 1;
        } else {
            prevRow = 0;
        }
    }

    m_shuffleHistory.append(prevRow);
    m_shuffleHistoryIndex = static_cast<int>(m_shuffleHistory.size()) - 1;
    playTrackAtIndex(prevRow);
}

void MainWindow::onPlayerNextRequested()
{
    if (!m_songModel || !m_playerWidget) return;
    const int total = m_songModel->rowCount();
    if (total == 0) return;

    // If shuffle is active
    if (m_playerWidget->isShuffleEnabled()) {
        // If we navigated backward earlier, forward goes forward in history
        if (m_shuffleHistoryIndex + 1 < m_shuffleHistory.size()) {
            ++m_shuffleHistoryIndex;
            const int nextRow = m_shuffleHistory[m_shuffleHistoryIndex];
            if (nextRow >= 0 && nextRow < total) {
                playTrackAtIndex(nextRow);
                return;
            }
        }

        // Pick a random track (preferring a different track if count > 1)
        int nextRow = QRandomGenerator::global()->bounded(total);
        if (total > 1 && nextRow == m_currentPlayingRow) {
            nextRow = (nextRow + 1 + QRandomGenerator::global()->bounded(total - 1)) % total;
        }

        m_shuffleHistory.append(nextRow);
        m_shuffleHistoryIndex = static_cast<int>(m_shuffleHistory.size()) - 1;
        playTrackAtIndex(nextRow);
        return;
    }

    // Sequential next
    int nextRow = (m_currentPlayingRow >= 0) ? (m_currentPlayingRow + 1) : 0;
    if (nextRow >= total) {
        if (m_playerWidget->repeatMode() == PlayerWidget::RepeatMode::All) {
            nextRow = 0;
        } else {
            // Repeat is off, stop at the end of the playlist
            m_playerWidget->stop();
            return;
        }
    }

    m_shuffleHistory.append(nextRow);
    m_shuffleHistoryIndex = static_cast<int>(m_shuffleHistory.size()) - 1;
    playTrackAtIndex(nextRow);
}

} // namespace tagit

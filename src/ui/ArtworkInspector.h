#ifndef TAGIT_ARTWORK_INSPECTOR_H
#define TAGIT_ARTWORK_INSPECTOR_H

#include <QWidget>
#include <QByteArray>
#include <QString>

#include "../model/AudioMetadata.h"

class QLabel;
class QPushButton;
class QScrollArea;

namespace tagit {

class TagService;

/**
 * @brief Inspector tab widget for viewing, adding, altering, removing,
 * and exporting cover art / thumbnail for the currently selected song.
 */
class ArtworkInspector : public QWidget {
    Q_OBJECT
public:
    explicit ArtworkInspector(QWidget *parent = nullptr);

    /// Set TagService instance for reading/writing artwork
    void setTagService(TagService *tagService);

    /// Update view for the selected song
    void setSong(const QString &filePath, const AudioMetadata &metadata);

    /// Clear the view when no song is selected
    void clear();

    /// Get current file path
    QString filePath() const { return m_filePath; }

signals:
    /**
     * @brief Emitted when artwork has been saved to disk so MainWindow
     * can update database records and table views.
     */
    void artworkChanged(const QString &filePath, bool hasArtwork);

private slots:
    void onChangeArtworkClicked();
    void onRemoveArtworkClicked();
    void onExportArtworkClicked();

private:
    void buildUi();
    void updatePreview();
    void setControlsEnabled(bool enabled);

    TagService   *m_tagService = nullptr;
    QString       m_filePath;
    AudioMetadata m_metadata;
    QByteArray    m_currentArtworkData;
    QString       m_currentMimeType;

    // UI elements
    QLabel      *m_fileLabel     = nullptr;
    QLabel      *m_imagePreview  = nullptr;
    QLabel      *m_infoLabel     = nullptr;
    QLabel      *m_statusLabel   = nullptr;
    QPushButton *m_changeButton  = nullptr;
    QPushButton *m_removeButton  = nullptr;
    QPushButton *m_exportButton  = nullptr;
};

} // namespace tagit

#endif // TAGIT_ARTWORK_INSPECTOR_H

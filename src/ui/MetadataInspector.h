#ifndef TAGIT_METADATA_INSPECTOR_H
#define TAGIT_METADATA_INSPECTOR_H

#include <QWidget>
#include <QMap>
#include <QStringList>

#include "../model/AudioMetadata.h"
#include "../model/Song.h"

class QLineEdit;
class QSpinBox;
class QCheckBox;
class QTextEdit;
class QLabel;
class QPushButton;

namespace tagit {

class TagService;

/**
 * @brief Editable metadata panel for the selected song.
 *
 * Each field has a checkbox next to it.
 *   ☑ checked   → this field will be included in the save
 *   ☐ unchecked → this field is shown read-only and will NOT be touched
 *
 * The "Save Selected Fields" button writes exactly the checked fields to the
 * audio file, overwriting whatever was there before.
 * Fields that are not checked are never modified.
 *
 * Usage:
 *   inspector->setSong(filePath, metadata);   // called when a single row is clicked
 *   inspector->setSelection(selectedSongs);    // called when 1 or multiple rows are selected
 *   // user edits fields and toggles checkboxes
 *   // user clicks Save → inspector emits saveRequested(filePath, metadata, fields)
 *   // MainWindow slot calls TagService::writeSelectedTags and updates the DB
 */
class MetadataInspector : public QWidget {
    Q_OBJECT
public:
    explicit MetadataInspector(QWidget *parent = nullptr);

    /// Load @p metadata for the song at @p filePath into the form.
    /// Resets all checkboxes to checked.
    void setSong(const QString &filePath, const AudioMetadata &metadata);

    /// Update inspector for single or multiple selected songs
    void setSelection(const QVector<Song> &selectedSongs);

    /// Selected songs currently held by inspector
    QVector<Song> selectedSongs() const { return m_selectedSongs; }

    /// Clear all fields and disable editing.
    void clear();

    /// Current metadata as shown in the form (including unsaved edits).
    AudioMetadata metadata() const;

    /// Current file path.
    QString filePath() const { return m_filePath; }

    /// Which fields are currently checked (field key strings).
    QStringList checkedFields() const;

    /// Update UI state during online review lookup
    void setReviewing(bool reviewing, const QString &statusText = QString());

    /// Show a temporary or permanent status message
    void showStatusMessage(const QString &msg, bool isError = false);

signals:
    /**
     * @brief Emitted when the user clicks "Save Selected Fields".
     *
     * @p filePath  — absolute path of the audio file to update
     * @p metadata  — full metadata struct with the user's edits applied
     * @p fields    — list of field keys the user wants to write
     *                (only these will be passed to TagService::writeSelectedTags)
     */
    void saveRequested(const QString       &filePath,
                       const QString       &editedFileName,
                       const AudioMetadata &metadata,
                       const QStringList   &fields);

    /**
     * @brief Emitted when the user clicks "🌐 Review Online".
     *
     * Forces an online metadata lookup across YouTube, iTunes, MusicBrainz, and Deezer.
     */
    void reviewRequested(const QString       &filePath,
                         const AudioMetadata &metadata);

    /**
     * @brief Emitted when the user triggers review with multiple songs selected.
     */
    void multiReviewRequested(const QVector<tagit::Song> &songs);

    /**
     * @brief Emitted when the user triggers clean with multiple songs selected.
     */
    void multiCleanRequested(const QVector<tagit::Song> &songs);

private slots:
    void onSaveClicked();
    void onReviewClicked();
    void onCleanClicked();
    void onSelectAll();
    void onSelectNone();

private:
    void buildUi();
    void setFieldsEnabled(bool enabled);

    QString       m_filePath;
    AudioMetadata m_original;   // as loaded from disk — for reset
    QVector<Song> m_selectedSongs;

    // Field rows  {key → {checkbox, editor widget}}
    struct FieldRow {
        QCheckBox *check  = nullptr;
        QWidget   *editor = nullptr;   // QLineEdit* or QSpinBox* or QTextEdit*
    };
    QMap<QString, FieldRow> m_rows;

    // Convenience typed accessors
    QLineEdit *lineEdit(const QString &key) const;
    QSpinBox  *spinBox(const QString  &key) const;
    QTextEdit *textEdit(const QString &key) const;

    QLabel      *m_fileLabel     = nullptr;
    QLineEdit   *m_fileNameEdit  = nullptr;
    QLabel      *m_statusLabel   = nullptr;
    QPushButton *m_reviewButton  = nullptr;
    QPushButton *m_saveButton    = nullptr;
    QPushButton *m_selectAll     = nullptr;
    QPushButton *m_selectNone    = nullptr;
    QPushButton *m_cleanButton   = nullptr;
};

} // namespace tagit

#endif // TAGIT_METADATA_INSPECTOR_H

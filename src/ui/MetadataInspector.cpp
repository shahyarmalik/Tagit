#include "MetadataInspector.h"

#include <QCheckBox>
#include <QFileInfo>
#include "../core/FilenameIntelligence.h"
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTextEdit>
#include <QVBoxLayout>

namespace tagit {

// ---------------------------------------------------------------------------
// Field definitions — order matches the form top→bottom
// key must match TagService::writeSelectedTags field names
// ---------------------------------------------------------------------------
struct FieldDef {
    QString key;
    QString label;
    enum Type { LineEdit, SpinBox, TextArea } type;
    int     min = 0, max = 0;   // for SpinBox
};

static const QList<FieldDef> kFields{
    {"title",       "Title",        FieldDef::LineEdit, 0, 0},
    {"artist",      "Artist",       FieldDef::LineEdit, 0, 0},
    {"album",       "Album",        FieldDef::LineEdit, 0, 0},
    {"albumArtist", "Album Artist", FieldDef::LineEdit, 0, 0},
    {"genre",       "Genre",        FieldDef::LineEdit, 0, 0},
    {"composer",    "Composer",     FieldDef::LineEdit, 0, 0},
    {"year",        "Year",         FieldDef::SpinBox,  0, 9999},
    {"trackNumber", "Track #",      FieldDef::SpinBox,  0, 999},
    {"discNumber",  "Disc #",       FieldDef::SpinBox,  0, 99},
    {"lyrics",      "Lyrics",       FieldDef::TextArea, 0, 0},
    {"comment",     "Comment",      FieldDef::LineEdit, 0, 0},
};

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

MetadataInspector::MetadataInspector(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
    setFieldsEnabled(false);   // nothing loaded yet
}

void MetadataInspector::buildUi()
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(4);

    // ---- File path label at the top ----
    m_fileLabel = new QLabel(tr("No file selected"), this);
    m_fileLabel->setWordWrap(true);
    m_fileLabel->setStyleSheet("color: #64748b; font-size: 11px; padding: 2px 4px;");
    outer->addWidget(m_fileLabel);

    // ---- Scrollable form ----
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto *container = new QWidget;
    auto *form = new QFormLayout(container);
    form->setContentsMargins(6, 6, 6, 6);
    form->setSpacing(6);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    // Dedicated File Name field at top of metadata section
    auto *nameLabel = new QLabel(tr("📁 File Name"), container);
    nameLabel->setStyleSheet("font-weight: bold; color: #60a5fa; padding-left: 2px;");
    
    auto *nameLayout = new QHBoxLayout();
    nameLayout->setContentsMargins(0, 0, 0, 0);
    nameLayout->setSpacing(6);

    m_fileNameEdit = new QLineEdit(container);
    m_fileNameEdit->setPlaceholderText(tr("(no file selected)"));
    m_fileNameEdit->setStyleSheet(
        "QLineEdit { background: #1e293b; color: #f8fafc; border: 1px solid #3b82f6; "
        "border-radius: 4px; padding: 5px 8px; font-weight: 600; font-size: 12px; }");
    
    m_cleanButton = new QPushButton(tr("✨ Clean"), container);
    m_cleanButton->setToolTip(tr("Clean filename and automatically update Title & Artist tags"));
    m_cleanButton->setCursor(Qt::PointingHandCursor);
    m_cleanButton->setStyleSheet(
        "QPushButton { background: #4f46e5; color: white; border: none; border-radius: 4px; padding: 5px 10px; font-weight: bold; font-size: 11px; }"
        "QPushButton:hover { background: #4338ca; }"
        "QPushButton:pressed { background: #3730a3; }"
        "QPushButton:disabled { background: #334155; color: #64748b; }");

    nameLayout->addWidget(m_fileNameEdit, 1);
    nameLayout->addWidget(m_cleanButton);
    form->addRow(nameLabel, nameLayout);

    for (const FieldDef &def : kFields) {
        // Checkbox on the left of the label
        auto *cb    = new QCheckBox(tr(def.label.toUtf8().constData()), container);
        cb->setChecked(true);
        cb->setToolTip(tr("Check to include this field in the save"));

        QWidget *editor = nullptr;

        if (def.type == FieldDef::LineEdit) {
            auto *le = new QLineEdit(container);
            le->setPlaceholderText(tr("(empty)"));
            editor = le;
        } else if (def.type == FieldDef::SpinBox) {
            auto *sb = new QSpinBox(container);
            sb->setRange(def.min, def.max);
            sb->setSpecialValueText("—");
            editor = sb;
        } else {   // TextArea
            auto *te = new QTextEdit(container);
            te->setFixedHeight(70);
            te->setPlaceholderText(tr("(empty)"));
            editor = te;
        }

        // Tie checkbox → editor: uncheck greys out the field
        QWidget *capturedEditor = editor;
        connect(cb, &QCheckBox::toggled, capturedEditor,
                [capturedEditor](bool checked) {
                    capturedEditor->setEnabled(checked);
                    capturedEditor->setStyleSheet(
                        checked ? QString()
                                : "color: gray;");
                });

        form->addRow(cb, editor);
        m_rows[def.key] = {cb, editor};
    }

    scroll->setWidget(container);
    outer->addWidget(scroll, 1);

    // ---- Button bar ----
    auto *btnBar = new QHBoxLayout();
    btnBar->setSpacing(4);

    m_selectAll    = new QPushButton(tr("✓ All"),   this);
    m_selectNone   = new QPushButton(tr("✗ None"),  this);
    m_reviewButton = new QPushButton(tr("🌐 Review Online"), this);
    m_reviewButton->setToolTip(tr("Force review and integrate tags from online providers (YouTube, iTunes, MusicBrainz, Deezer)"));
    m_reviewButton->setStyleSheet(
        "QPushButton { background: #3b82f6; color: white; "
        "border-radius: 4px; padding: 5px 10px; font-weight: bold; }"
        "QPushButton:hover  { background: #2563eb; }"
        "QPushButton:pressed { background: #1d4ed8; }"
        "QPushButton:disabled { background: #64748b; color: #94a3b8; }");

    m_saveButton = new QPushButton(tr("💾 Save Selected Fields"), this);
    m_saveButton->setDefault(true);
    m_saveButton->setStyleSheet(
        "QPushButton { background: #10b981; color: white; "
        "border-radius: 4px; padding: 5px 12px; font-weight: bold; }"
        "QPushButton:hover  { background: #059669; }"
        "QPushButton:pressed { background: #047857; }"
        "QPushButton:disabled { background: #64748b; color: #94a3b8; }");

    m_selectAll->setFlat(true);
    m_selectNone->setFlat(true);

    btnBar->addWidget(m_selectAll);
    btnBar->addWidget(m_selectNone);
    btnBar->addStretch();
    btnBar->addWidget(m_reviewButton);
    btnBar->addWidget(m_saveButton);
    outer->addLayout(btnBar);

    // ---- Status label ----
    m_statusLabel = new QLabel(this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setWordWrap(true);
    outer->addWidget(m_statusLabel);

    // ---- Connections ----
    connect(m_reviewButton, &QPushButton::clicked, this, &MetadataInspector::onReviewClicked);
    connect(m_saveButton,   &QPushButton::clicked, this, &MetadataInspector::onSaveClicked);
    connect(m_cleanButton,  &QPushButton::clicked, this, &MetadataInspector::onCleanClicked);
    connect(m_selectAll,    &QPushButton::clicked, this, &MetadataInspector::onSelectAll);
    connect(m_selectNone,   &QPushButton::clicked, this, &MetadataInspector::onSelectNone);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void MetadataInspector::setSong(const QString &filePath, const AudioMetadata &md)
{
    m_selectedSongs.clear();
    if (!filePath.isEmpty()) {
        Song s;
        s.filePath = filePath;
        s.fileName = QFileInfo(filePath).fileName();
        s.metadata = md;
        m_selectedSongs.append(s);
    }

    m_filePath = filePath;
    m_original = md;

    // Show path and filename
    const QString name = QFileInfo(filePath).fileName();
    m_fileLabel->setText(filePath);
    m_fileLabel->setToolTip(filePath);
    if (m_fileNameEdit) {
        m_fileNameEdit->setText(name.isEmpty() ? filePath : name);
        m_fileNameEdit->setToolTip(filePath);
        m_fileNameEdit->setCursorPosition(0);
        m_fileNameEdit->setEnabled(true);
    }
    if (m_cleanButton) {
        m_cleanButton->setText(tr("✨ Clean"));
        m_cleanButton->setEnabled(true);
    }

    // Populate fields
    auto setLE = [&](const QString &key, const QString &val) {
        if (auto *le = lineEdit(key)) le->setText(val);
    };
    auto setSB = [&](const QString &key, int val) {
        if (auto *sb = spinBox(key)) sb->setValue(val);
    };
    auto setTE = [&](const QString &key, const QString &val) {
        if (auto *te = textEdit(key)) te->setPlainText(val);
    };

    setLE("title",       md.title);
    setLE("artist",      md.artist);
    setLE("album",       md.album);
    setLE("albumArtist", md.albumArtist);
    setLE("genre",       md.genre);
    setLE("composer",    md.composer);
    setSB("year",        md.year);
    setSB("trackNumber", md.trackNumber);
    setSB("discNumber",  md.discNumber);
    setTE("lyrics",      md.lyrics);
    setLE("comment",     md.comment);

    // Reset all checkboxes to checked and editors to enabled
    for (auto &row : m_rows) {
        row.check->setChecked(true);
        row.editor->setEnabled(true);
        row.editor->setStyleSheet(QString());
    }

    m_statusLabel->clear();
    setFieldsEnabled(true);
    m_saveButton->setEnabled(true);
    m_reviewButton->setEnabled(true);
    m_reviewButton->setText(tr("🌐 Review Online"));
}

void MetadataInspector::setSelection(const QVector<Song> &selectedSongs)
{
    m_selectedSongs = selectedSongs;

    if (selectedSongs.isEmpty()) {
        clear();
        return;
    }

    if (selectedSongs.size() == 1) {
        setSong(selectedSongs[0].filePath, selectedSongs[0].metadata);
        return;
    }

    // Multiple songs selected
    m_filePath.clear();
    m_original = AudioMetadata{};

    const int count = static_cast<int>(selectedSongs.size());
    m_fileLabel->setText(tr("🎵 %1 tracks selected").arg(count));
    m_fileLabel->setToolTip(tr("%1 songs selected in table").arg(count));

    if (m_fileNameEdit) {
        m_fileNameEdit->setText(tr("(Multiple files selected)"));
        m_fileNameEdit->setToolTip(QString());
        m_fileNameEdit->setEnabled(false);
    }

    for (auto &row : m_rows) {
        if (auto *le = qobject_cast<QLineEdit *>(row.editor)) le->clear();
        else if (auto *sb = qobject_cast<QSpinBox *>(row.editor)) sb->setValue(0);
        else if (auto *te = qobject_cast<QTextEdit *>(row.editor)) te->clear();
        row.check->setChecked(false);
    }

    setFieldsEnabled(false);
    m_saveButton->setEnabled(false);

    if (m_cleanButton) {
        m_cleanButton->setEnabled(true);
        m_cleanButton->setText(tr("✨ Clean %1 Selected").arg(count));
    }

    if (m_reviewButton) {
        m_reviewButton->setEnabled(true);
        m_reviewButton->setText(tr("🌐 Review %1 Selected Online").arg(count));
    }

    m_statusLabel->setText(tr("Ready to batch review or clean %1 selected tracks.").arg(count));
    m_statusLabel->setStyleSheet("color: #38bdf8; font-style: italic;");
}

void MetadataInspector::clear()
{
    m_filePath.clear();
    m_selectedSongs.clear();
    m_original = AudioMetadata{};
    m_fileLabel->setText(tr("No file selected"));
    m_fileLabel->setToolTip(QString());
    if (m_fileNameEdit) {
        m_fileNameEdit->clear();
        m_fileNameEdit->setToolTip(QString());
        m_fileNameEdit->setEnabled(false);
    }
    if (m_cleanButton) {
        m_cleanButton->setText(tr("✨ Clean"));
        m_cleanButton->setEnabled(false);
    }

    for (auto &row : m_rows) {
        if (auto *le = qobject_cast<QLineEdit *>(row.editor)) le->clear();
        else if (auto *sb = qobject_cast<QSpinBox *>(row.editor)) sb->setValue(0);
        else if (auto *te = qobject_cast<QTextEdit *>(row.editor)) te->clear();
        row.check->setChecked(false);
    }

    m_statusLabel->clear();
    setFieldsEnabled(false);
    m_saveButton->setEnabled(false);
    m_reviewButton->setEnabled(false);
    m_reviewButton->setText(tr("🌐 Review Online"));
}

void MetadataInspector::setReviewing(bool reviewing, const QString &statusText)
{
    const bool hasItems = (!m_filePath.isEmpty() || !m_selectedSongs.isEmpty());
    if (m_reviewButton) {
        m_reviewButton->setEnabled(!reviewing && hasItems);
        if (reviewing) {
            m_reviewButton->setText(tr("⏳ Reviewing..."));
        } else if (m_selectedSongs.size() > 1) {
            m_reviewButton->setText(tr("🌐 Review %1 Selected Online").arg(m_selectedSongs.size()));
        } else {
            m_reviewButton->setText(tr("🌐 Review Online"));
        }
    }
    if (m_saveButton) {
        m_saveButton->setEnabled(!reviewing && !m_filePath.isEmpty());
    }
    if (!statusText.isEmpty()) {
        m_statusLabel->setText(statusText);
        m_statusLabel->setStyleSheet(reviewing ? "color: #38bdf8; font-weight: bold;" : QString());
    }
}

void MetadataInspector::showStatusMessage(const QString &msg, bool isError)
{
    m_statusLabel->setText(msg);
    m_statusLabel->setStyleSheet(isError ? "color: #ef4444; font-weight: bold;" : "color: #10b981; font-weight: bold;");
}

void MetadataInspector::onReviewClicked()
{
    if (m_selectedSongs.size() > 1) {
        setReviewing(true, tr("Reviewing %1 selected tracks online...").arg(m_selectedSongs.size()));
        emit multiReviewRequested(m_selectedSongs);
        return;
    }

    if (m_filePath.isEmpty()) return;
    setReviewing(true, tr("Querying online providers..."));
    emit reviewRequested(m_filePath, metadata());
}

AudioMetadata MetadataInspector::metadata() const
{
    AudioMetadata md = m_original;

    auto getLE = [&](const QString &key) -> QString {
        if (auto *le = lineEdit(key)) return le->text().trimmed();
        return {};
    };
    auto getSB = [&](const QString &key) -> int {
        if (auto *sb = spinBox(key)) return sb->value();
        return 0;
    };
    auto getTE = [&](const QString &key) -> QString {
        if (auto *te = textEdit(key)) return te->toPlainText().trimmed();
        return {};
    };

    md.title       = getLE("title");
    md.artist      = getLE("artist");
    md.album       = getLE("album");
    md.albumArtist = getLE("albumArtist");
    md.genre       = getLE("genre");
    md.composer    = getLE("composer");
    md.year        = getSB("year");
    md.trackNumber = getSB("trackNumber");
    md.discNumber  = getSB("discNumber");
    md.lyrics      = getTE("lyrics");
    md.comment     = getLE("comment");

    return md;
}

QStringList MetadataInspector::checkedFields() const
{
    QStringList result;
    for (auto it = m_rows.begin(); it != m_rows.end(); ++it) {
        if (it.value().check->isChecked()) {
            result << it.key();
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// Private slots
// ---------------------------------------------------------------------------

void MetadataInspector::onSaveClicked()
{
    if (m_filePath.isEmpty()) return;

    const QStringList fields = checkedFields();
    const QString editedFileName = m_fileNameEdit->text().trimmed();
    const QString originalFileName = QFileInfo(m_filePath).fileName();
    const bool fileNameChanged = (editedFileName != originalFileName && !editedFileName.isEmpty());

    if (fields.isEmpty() && !fileNameChanged) {
        m_statusLabel->setStyleSheet("color: orange;");
        m_statusLabel->setText(tr("No fields selected or renamed — nothing to save."));
        return;
    }

    m_statusLabel->setStyleSheet("color: gray;");
    m_statusLabel->setText(tr("Saving…"));

    emit saveRequested(m_filePath, editedFileName, metadata(), fields);
}

void MetadataInspector::onCleanClicked()
{
    if (m_selectedSongs.size() > 1) {
        emit multiCleanRequested(m_selectedSongs);
        return;
    }

    if (m_filePath.isEmpty()) return;

    const QString currentName = m_fileNameEdit->text().trimmed();
    if (currentName.isEmpty()) return;

    FilenameIntelligence fi;
    const QString cleanedName = fi.cleanFilename(currentName);
    m_fileNameEdit->setText(cleanedName);

    // Also parse and populate metadata fields!
    const AudioMetadata parsed = fi.parse(cleanedName);
    
    // Check if artist was parsed, set it, check it
    if (!parsed.artist.isEmpty()) {
        if (auto *le = lineEdit(QStringLiteral("artist"))) {
            le->setText(parsed.artist);
            if (auto *row = m_rows.value(QStringLiteral("artist")).check) {
                row->setChecked(true);
                m_rows.value(QStringLiteral("artist")).editor->setEnabled(true);
            }
        }
    }

    // Check if title was parsed, set it, check it
    if (!parsed.title.isEmpty()) {
        if (auto *le = lineEdit(QStringLiteral("title"))) {
            le->setText(parsed.title);
            if (auto *row = m_rows.value(QStringLiteral("title")).check) {
                row->setChecked(true);
                m_rows.value(QStringLiteral("title")).editor->setEnabled(true);
            }
        }
    }

    // Check if trackNumber was parsed, set it, check it
    if (parsed.trackNumber > 0) {
        if (auto *sb = spinBox(QStringLiteral("trackNumber"))) {
            sb->setValue(parsed.trackNumber);
            if (auto *row = m_rows.value(QStringLiteral("trackNumber")).check) {
                row->setChecked(true);
                m_rows.value(QStringLiteral("trackNumber")).editor->setEnabled(true);
            }
        }
    }
}

void MetadataInspector::onSelectAll()
{
    for (auto &row : m_rows) {
        row.check->setChecked(true);
    }
}

void MetadataInspector::onSelectNone()
{
    for (auto &row : m_rows) {
        row.check->setChecked(false);
    }
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void MetadataInspector::setFieldsEnabled(bool enabled)
{
    m_saveButton->setEnabled(enabled);
    if (m_fileNameEdit) {
        m_fileNameEdit->setEnabled(enabled);
    }
    if (m_cleanButton) {
        m_cleanButton->setEnabled(enabled);
    }
    for (auto &row : m_rows) {
        row.check->setEnabled(enabled);
        row.editor->setEnabled(enabled && row.check->isChecked());
    }
}

QLineEdit *MetadataInspector::lineEdit(const QString &key) const
{
    auto it = m_rows.find(key);
    if (it == m_rows.end()) return nullptr;
    return qobject_cast<QLineEdit *>(it.value().editor);
}

QSpinBox *MetadataInspector::spinBox(const QString &key) const
{
    auto it = m_rows.find(key);
    if (it == m_rows.end()) return nullptr;
    return qobject_cast<QSpinBox *>(it.value().editor);
}

QTextEdit *MetadataInspector::textEdit(const QString &key) const
{
    auto it = m_rows.find(key);
    if (it == m_rows.end()) return nullptr;
    return qobject_cast<QTextEdit *>(it.value().editor);
}

} // namespace tagit

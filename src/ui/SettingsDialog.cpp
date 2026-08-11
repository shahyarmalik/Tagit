#include "SettingsDialog.h"
#include "../core/SettingsManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QFrame>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QLabel>

namespace tagit {

// Field definitions: {key, display label}
// Key must match the strings in SettingsManager::allowedTagFields()
static const QList<QPair<QString,QString>> kFieldDefs {
    {"title",       "Title"},
    {"artist",      "Artist"},
    {"album",       "Album"},
    {"albumArtist", "Album Artist"},
    {"genre",       "Genre"},
    {"composer",    "Composer"},
    {"year",        "Year"},
    {"trackNumber", "Track Number"},
    {"discNumber",  "Disc Number"},
    {"lyrics",      "Lyrics"},
    {"artwork",     "Cover Art"},
};

SettingsDialog::SettingsDialog(SettingsManager *settings, QWidget *parent)
    : QDialog(parent)
    , m_settings(settings)
{
    setWindowTitle(tr("TagIt Settings"));
    setMinimumWidth(480);
    buildUi();
    loadSettings();

    auto *buttons = findChild<QDialogButtonBox *>();
    if (buttons) {
        connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::saveSettings);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }
}

void SettingsDialog::buildUi()
{
    auto *outer = new QVBoxLayout(this);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto *container = new QWidget;
    auto *layout    = new QVBoxLayout(container);
    layout->setSpacing(8);

    // ---- General ----
    auto *generalGroup = new QGroupBox(tr("General"), container);
    auto *generalForm  = new QFormLayout(generalGroup);
    m_language = new QComboBox(generalGroup);
    m_language->addItem("English", "en");
    m_language->addItem("Deutsch", "de");
    m_language->addItem("Français", "fr");
    generalForm->addRow(tr("Language:"), m_language);
    layout->addWidget(generalGroup);

    // ---- Library ----
    auto *libraryGroup = new QGroupBox(tr("Library"), container);
    auto *libraryForm  = new QFormLayout(libraryGroup);
    m_scanOnStartup = new QCheckBox(tr("Scan libraries on startup"), libraryGroup);
    m_watchFolders  = new QCheckBox(tr("Watch folders for changes"), libraryGroup);
    libraryForm->addRow(m_scanOnStartup);
    libraryForm->addRow(m_watchFolders);
    layout->addWidget(libraryGroup);

    // ---- Metadata ----
    auto *metadataGroup = new QGroupBox(tr("Metadata"), container);
    auto *metadataForm  = new QFormLayout(metadataGroup);
    m_preserveExisting = new QCheckBox(tr("Never overwrite existing metadata"), metadataGroup);
    m_allowOnline = new QCheckBox(tr("Enable online lookup (YouTube · iTunes · MusicBrainz · Deezer)"), metadataGroup);
    m_allowOnline->setToolTip(tr("Queries all four providers in parallel.\n"
                                  "For each field the value agreed on by the most providers wins.\n"
                                  "Existing embedded tags are never overwritten."));
    m_confidenceThreshold = new QSpinBox(metadataGroup);
    m_confidenceThreshold->setRange(0, 100);
    m_confidenceThreshold->setSuffix(" %");
    m_confidenceThreshold->setToolTip(tr(
        "Only write tags to disk when the cross-provider confidence\n"
        "meets or exceeds this percentage.\n\n"
        "25%  = write if any single provider matched (recommended).\n"
        "50%  = write only when 2 of 4 providers agreed.\n"
        "75%  = write only when 3 of 4 providers agreed.\n"
        "100% = write only when all 4 providers returned the same value.\n\n"
        "Tags are always shown in the app regardless of this setting."));
    metadataForm->addRow(m_preserveExisting);
    metadataForm->addRow(m_allowOnline);
    metadataForm->addRow(tr("Write when confidence ≥:"), m_confidenceThreshold);
    layout->addWidget(metadataGroup);

    // ---- Tag Fields ----
    auto *fieldsGroup = new QGroupBox(tr("Tag Fields to Update"), container);
    fieldsGroup->setToolTip(tr(
        "Check the fields you want TagIt to fill in automatically.\n"
        "Unchecked fields are never modified, even if they are empty."));

    auto *fieldsGrid = new QGridLayout(fieldsGroup);
    int row = 0, col = 0;
    for (const auto &[key, label] : kFieldDefs) {
        auto *cb = new QCheckBox(tr(label.toUtf8().constData()), fieldsGroup);
        m_fieldChecks[key] = cb;
        fieldsGrid->addWidget(cb, row, col);
        col = (col + 1) % 2;
        if (col == 0) ++row;
    }

    // "Select All" / "Clear All" convenience buttons
    auto *selRow    = new QHBoxLayout;
    auto *selectAll = new QPushButton(tr("Select All"),  fieldsGroup);
    auto *clearAll  = new QPushButton(tr("Clear All"), fieldsGroup);
    selectAll->setFlat(true);
    clearAll->setFlat(true);
    selRow->addStretch();
    selRow->addWidget(selectAll);
    selRow->addWidget(clearAll);
    fieldsGrid->addLayout(selRow, row + 1, 0, 1, 2);

    connect(selectAll, &QPushButton::clicked, this, [this]() {
        for (auto *cb : m_fieldChecks) cb->setChecked(true);
    });
    connect(clearAll, &QPushButton::clicked, this, [this]() {
        for (auto *cb : m_fieldChecks) cb->setChecked(false);
    });

    layout->addWidget(fieldsGroup);

    // ---- Backup ----
    auto *backupGroup = new QGroupBox(tr("Backup"), container);
    auto *backupForm  = new QFormLayout(backupGroup);
    m_createBackups = new QCheckBox(tr("Create backup before writing tags"), backupGroup);
    m_maxBackups    = new QSpinBox(backupGroup);
    m_maxBackups->setRange(1, 100);
    backupForm->addRow(m_createBackups);
    backupForm->addRow(tr("Keep up to:"), m_maxBackups);
    layout->addWidget(backupGroup);

    // ---- Appearance ----
    auto *appearanceGroup = new QGroupBox(tr("Appearance"), container);
    auto *appearanceForm  = new QFormLayout(appearanceGroup);
    m_theme = new QComboBox(appearanceGroup);
    m_theme->addItem(tr("System"), "system");
    m_theme->addItem(tr("Light"),  "light");
    m_theme->addItem(tr("Dark"),   "dark");
    appearanceForm->addRow(tr("Theme:"), m_theme);
    layout->addWidget(appearanceGroup);

    layout->addStretch();
    scroll->setWidget(container);
    outer->addWidget(scroll, 1);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->setObjectName("settingsButtons");
    outer->addWidget(buttons);
}

void SettingsDialog::loadSettings()
{
    m_language->setCurrentIndex(m_language->findData(m_settings->language()));
    m_scanOnStartup->setChecked(m_settings->scanOnStartup());
    m_watchFolders->setChecked(m_settings->watchFolders());
    m_preserveExisting->setChecked(m_settings->preserveExistingMetadata());
    m_allowOnline->setChecked(m_settings->allowOnlineLookup());
    m_confidenceThreshold->setValue(m_settings->metadataConfidenceThreshold());
    m_createBackups->setChecked(m_settings->createBackupsBeforeWrite());
    m_maxBackups->setValue(m_settings->maxBackupHistory());
    m_theme->setCurrentIndex(m_theme->findData(m_settings->theme()));

    // Per-field checkboxes
    const QStringList allowed = m_settings->allowedTagFields();
    for (auto it = m_fieldChecks.begin(); it != m_fieldChecks.end(); ++it) {
        it.value()->setChecked(allowed.contains(it.key()));
    }
}

void SettingsDialog::saveSettings()
{
    if (!m_settings) return;

    m_settings->setLanguage(m_language->currentData().toString());
    m_settings->setScanOnStartup(m_scanOnStartup->isChecked());
    m_settings->setWatchFolders(m_watchFolders->isChecked());
    m_settings->setPreserveExistingMetadata(m_preserveExisting->isChecked());
    m_settings->setAllowOnlineLookup(m_allowOnline->isChecked());
    m_settings->setMetadataConfidenceThreshold(m_confidenceThreshold->value());
    m_settings->setCreateBackupsBeforeWrite(m_createBackups->isChecked());
    m_settings->setMaxBackupHistory(m_maxBackups->value());
    m_settings->setTheme(m_theme->currentData().toString());

    // Per-field allowed list
    QStringList allowed;
    for (auto it = m_fieldChecks.begin(); it != m_fieldChecks.end(); ++it) {
        if (it.value()->isChecked()) allowed << it.key();
    }
    m_settings->setAllowedTagFields(allowed);

    accept();
}

} // namespace tagit

#ifndef TAGIT_SETTINGS_DIALOG_H
#define TAGIT_SETTINGS_DIALOG_H

#include <QDialog>
#include <QMap>

class QCheckBox;
class QComboBox;
class QSpinBox;

namespace tagit {

class SettingsManager;

/**
 * @brief Settings dialog wired to SettingsManager.
 *
 * Sections:
 *   General    — language
 *   Library    — scan on startup, watch folders
 *   Metadata   — preserve existing, online lookup, confidence threshold
 *   Tag Fields — per-field checkboxes controlling which tags TagIt may write
 *   Backup     — create backups, max history
 *   Appearance — theme
 */
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(SettingsManager *settings, QWidget *parent = nullptr);

private slots:
    void saveSettings();

private:
    void buildUi();
    void loadSettings();

    SettingsManager *m_settings = nullptr;

    // General
    QComboBox *m_language = nullptr;

    // Library
    QCheckBox *m_scanOnStartup = nullptr;
    QCheckBox *m_watchFolders  = nullptr;

    // Metadata
    QCheckBox *m_preserveExisting    = nullptr;
    QCheckBox *m_allowOnline         = nullptr;
    QSpinBox  *m_confidenceThreshold = nullptr;

    // Tag Fields — key matches SettingsManager::allowedTagFields() values
    QMap<QString, QCheckBox *> m_fieldChecks;

    // Backup
    QCheckBox *m_createBackups = nullptr;
    QSpinBox  *m_maxBackups    = nullptr;

    // Appearance
    QComboBox *m_theme = nullptr;
};

} // namespace tagit

#endif // TAGIT_SETTINGS_DIALOG_H

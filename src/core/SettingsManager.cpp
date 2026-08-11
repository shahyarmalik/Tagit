#include "SettingsManager.h"

#include <algorithm>

namespace tagit {

SettingsManager::SettingsManager(const QString &appDataPath, QObject *parent)
    : QObject(parent)
    , m_settings(appDataPath + "/settings.ini", QSettings::IniFormat)
{
}

SettingsManager::~SettingsManager() = default;

// --- General ---
QString SettingsManager::language() const
{
    return m_settings.value("general/language", "en").toString();
}

void SettingsManager::setLanguage(const QString &lang)
{
    m_settings.setValue("general/language", lang);
    emit settingChanged("general/language", lang);
}

bool SettingsManager::firstRun() const
{
    return m_settings.value("general/firstRun", true).toBool();
}

void SettingsManager::setFirstRun(bool firstRun)
{
    m_settings.setValue("general/firstRun", firstRun);
    emit settingChanged("general/firstRun", firstRun);
}

// --- Library ---
QStringList SettingsManager::musicLibraryPaths() const
{
    return m_settings.value("library/paths", QStringList()).toStringList();
}

void SettingsManager::setMusicLibraryPaths(const QStringList &paths)
{
    m_settings.setValue("library/paths", paths);
    emit settingChanged("library/paths", QVariant::fromValue(paths));
}

bool SettingsManager::scanOnStartup() const
{
    return m_settings.value("library/scanOnStartup", true).toBool();
}

void SettingsManager::setScanOnStartup(bool enable)
{
    m_settings.setValue("library/scanOnStartup", enable);
    emit settingChanged("library/scanOnStartup", enable);
}

bool SettingsManager::watchFolders() const
{
    return m_settings.value("library/watchFolders", false).toBool();
}

void SettingsManager::setWatchFolders(bool enable)
{
    m_settings.setValue("library/watchFolders", enable);
    emit settingChanged("library/watchFolders", enable);
}

// --- Metadata ---
bool SettingsManager::preserveExistingMetadata() const
{
    return m_settings.value("metadata/preserveExisting", true).toBool();
}

void SettingsManager::setPreserveExistingMetadata(bool enable)
{
    m_settings.setValue("metadata/preserveExisting", enable);
    emit settingChanged("metadata/preserveExisting", enable);
}

bool SettingsManager::allowOnlineLookup() const
{
    return m_settings.value("metadata/allowOnlineLookup", false).toBool();
}

void SettingsManager::setAllowOnlineLookup(bool enable)
{
    m_settings.setValue("metadata/allowOnlineLookup", enable);
    emit settingChanged("metadata/allowOnlineLookup", enable);
}

int SettingsManager::metadataConfidenceThreshold() const
{
    // Default 30: write tags if any provider matched at all.
    // Raise to 67 in Settings if you want at least 2 of 3 providers to agree.
    return m_settings.value("metadata/confidenceThreshold", 30).toInt();
}

void SettingsManager::setMetadataConfidenceThreshold(int percent)
{
    m_settings.setValue("metadata/confidenceThreshold", std::clamp(percent, 0, 100));
    emit settingChanged("metadata/confidenceThreshold", percent);
}

// --- Allowed tag fields ---
/// The full canonical set of writable field names.
static const QStringList kAllTagFields{
    QStringLiteral("title"),
    QStringLiteral("artist"),
    QStringLiteral("album"),
    QStringLiteral("albumArtist"),
    QStringLiteral("genre"),
    QStringLiteral("composer"),
    QStringLiteral("year"),
    QStringLiteral("trackNumber"),
    QStringLiteral("discNumber"),
    QStringLiteral("lyrics"),
    QStringLiteral("artwork"),
};

QStringList SettingsManager::allowedTagFields() const
{
    if (!m_settings.contains("metadata/allowedFields")) {
        return kAllTagFields;   // first run — all fields allowed by default
    }
    return m_settings.value("metadata/allowedFields").toStringList();
}

void SettingsManager::setAllowedTagFields(const QStringList &fields)
{
    m_settings.setValue("metadata/allowedFields", fields);
    emit settingChanged("metadata/allowedFields", QVariant::fromValue(fields));
}

bool SettingsManager::isFieldAllowed(const QString &field) const
{
    return allowedTagFields().contains(field);
}

// --- Backup ---
bool SettingsManager::createBackupsBeforeWrite() const
{
    return m_settings.value("backup/createBeforeWrite", true).toBool();
}

void SettingsManager::setCreateBackupsBeforeWrite(bool enable)
{
    m_settings.setValue("backup/createBeforeWrite", enable);
    emit settingChanged("backup/createBeforeWrite", enable);
}

int SettingsManager::maxBackupHistory() const
{
    return m_settings.value("backup/maxHistory", 10).toInt();
}

void SettingsManager::setMaxBackupHistory(int count)
{
    m_settings.setValue("backup/maxHistory", std::max(1, count));
    emit settingChanged("backup/maxHistory", count);
}

// --- Appearance ---
QString SettingsManager::theme() const
{
    return m_settings.value("appearance/theme", "system").toString();
}

void SettingsManager::setTheme(const QString &theme)
{
    m_settings.setValue("appearance/theme", theme);
    emit settingChanged("appearance/theme", theme);
}

// --- General ---
QVariant SettingsManager::value(const QString &key, const QVariant &defaultValue) const
{
    return m_settings.value(key, defaultValue);
}

void SettingsManager::setValue(const QString &key, const QVariant &value)
{
    m_settings.setValue(key, value);
    emit settingChanged(key, value);
}

} // namespace tagit


#ifndef TAGIT_SETTINGS_MANAGER_H
#define TAGIT_SETTINGS_MANAGER_H

#include <QObject>
#include <QSettings>
#include <QString>
#include <QVariant>
#include <QStringList>

namespace tagit {

/**
 * @brief Manages application settings with persistence via QSettings.
 *
 * Settings are stored in the platform-appropriate location (e.g., ~/.config/tagit/).
 */
class SettingsManager : public QObject {
    Q_OBJECT
public:
    explicit SettingsManager(const QString &appDataPath, QObject *parent = nullptr);
    ~SettingsManager() override;

    // --- General ---
    QString language() const;
    void setLanguage(const QString &lang);

    bool firstRun() const;
    void setFirstRun(bool firstRun);

    // --- Library ---
    QStringList musicLibraryPaths() const;
    void setMusicLibraryPaths(const QStringList &paths);

    bool scanOnStartup() const;
    void setScanOnStartup(bool enable);

    bool watchFolders() const;
    void setWatchFolders(bool enable);

    // --- Metadata ---
    bool preserveExistingMetadata() const;
    void setPreserveExistingMetadata(bool enable);

    bool allowOnlineLookup() const;
    void setAllowOnlineLookup(bool enable);

    int metadataConfidenceThreshold() const;
    void setMetadataConfidenceThreshold(int percent);

    /**
     * @brief Which tag fields TagIt is allowed to write to files.
     *
     * The returned list contains field name strings from the set:
     *   "title", "artist", "album", "albumArtist", "genre", "composer",
     *   "year", "trackNumber", "discNumber", "lyrics", "artwork"
     *
     * By default every field is allowed (returns the full set).
     * When the user unchecks a field in Settings, it is removed from
     * this list and TagIt will never write that field to any file.
     */
    QStringList allowedTagFields() const;
    void        setAllowedTagFields(const QStringList &fields);

    /// Convenience: true when @p field is in allowedTagFields().
    bool isFieldAllowed(const QString &field) const;

    // --- Backup ---
    bool createBackupsBeforeWrite() const;
    void setCreateBackupsBeforeWrite(bool enable);

    int maxBackupHistory() const;
    void setMaxBackupHistory(int count);

    // --- Appearance ---
    QString theme() const;
    void setTheme(const QString &theme);

    // --- General get/set ---
    QVariant value(const QString &key, const QVariant &defaultValue = QVariant()) const;
    void setValue(const QString &key, const QVariant &value);

signals:
    void settingChanged(const QString &key, const QVariant &value);

private:
    QSettings m_settings;
};

} // namespace tagit

#endif // TAGIT_SETTINGS_MANAGER_H


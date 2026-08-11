#ifndef TAGIT_IPLATFORM_SERVICES_H
#define TAGIT_IPLATFORM_SERVICES_H

#include <QString>
#include <QStringList>

namespace tagit {

/**
 * @brief Abstract interface for platform-specific services.
 *
 * All platform-dependent functionality is accessed through this interface.
 * Concrete implementations exist for Linux, Windows, and macOS.
 */
class IPlatformServices {
public:
    virtual ~IPlatformServices() = default;

    /// Returns the platform name (Linux, Windows, macOS).
    virtual QString platformName() const = 0;

    /// Returns the default music library path for the platform.
    virtual QString defaultMusicPath() const = 0;

    /// Returns the application data directory (e.g., ~/.config/tagit).
    virtual QString appDataPath() const = 0;

    /// Returns the application cache directory.
    virtual QString appCachePath() const = 0;

    /// Returns a list of supported audio file extensions (with dot, e.g. ".mp3").
    virtual QStringList supportedAudioExtensions() const = 0;

    /// Returns the number of logical CPU cores available.
    virtual int idealThreadCount() const = 0;

    /// Opens a file manager window showing the given path.
    virtual void showInFileManager(const QString &path) = 0;

    /// Opens a URL in the default browser (for online lookups).
    virtual void openUrl(const QString &url) = 0;
};

} // namespace tagit

#endif // TAGIT_IPLATFORM_SERVICES_H


#ifndef TAGIT_LINUX_PLATFORM_SERVICES_H
#define TAGIT_LINUX_PLATFORM_SERVICES_H

#include "../IPlatformServices.h"

namespace tagit {

/**
 * @brief Linux implementation of IPlatformServices.
 *
 * Follows XDG Base Directory Specification for config/cache paths.
 */
class LinuxPlatformServices : public IPlatformServices {
public:
    LinuxPlatformServices();
    ~LinuxPlatformServices() override = default;

    QString platformName() const override;
    QString defaultMusicPath() const override;
    QString appDataPath() const override;
    QString appCachePath() const override;
    QStringList supportedAudioExtensions() const override;
    int idealThreadCount() const override;
    void showInFileManager(const QString &path) override;
    void openUrl(const QString &url) override;

private:
    QString resolveXdgPath(const QString &envVar, const QString &fallback) const;
};

} // namespace tagit

#endif // TAGIT_LINUX_PLATFORM_SERVICES_H


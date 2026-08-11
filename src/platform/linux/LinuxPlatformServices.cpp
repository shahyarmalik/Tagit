#include "LinuxPlatformServices.h"

#include <QProcess>
#include <QStandardPaths>
#include <QDir>
#include <QThread>
#include <cstdlib>

namespace tagit {

LinuxPlatformServices::LinuxPlatformServices()
{
}

QString LinuxPlatformServices::platformName() const
{
    return QStringLiteral("Linux");
}

QString LinuxPlatformServices::defaultMusicPath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
}

QString LinuxPlatformServices::appDataPath() const
{
    return resolveXdgPath("XDG_CONFIG_HOME", QDir::homePath() + "/.config") + "/tagit";
}

QString LinuxPlatformServices::appCachePath() const
{
    return resolveXdgPath("XDG_CACHE_HOME", QDir::homePath() + "/.cache") + "/tagit";
}

QStringList LinuxPlatformServices::supportedAudioExtensions() const
{
    return {
        ".mp3",  ".flac", ".ogg", ".opus", ".m4a",
        ".aac",  ".wav",  ".wma", ".aiff", ".dsf",
        ".ape",  ".mpc",  ".wv",  ".tta"
    };
}

int LinuxPlatformServices::idealThreadCount() const
{
    return QThread::idealThreadCount();
}

void LinuxPlatformServices::showInFileManager(const QString &path)
{
    QProcess::startDetached("xdg-open", {QDir(path).absolutePath()});
}

void LinuxPlatformServices::openUrl(const QString &url)
{
    QProcess::startDetached("xdg-open", {url});
}

QString LinuxPlatformServices::resolveXdgPath(const QString &envVar, const QString &fallback) const
{
    const char *env = std::getenv(envVar.toUtf8().constData());
    if (env && *env) {
        return QString::fromUtf8(env);
    }
    return fallback;
}

} // namespace tagit


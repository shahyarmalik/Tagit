#include "FilesystemService.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

namespace tagit {

QStringList FilesystemService::scanForAudioFiles(const QString &rootPath, const QStringList &extensions) const
{
    QStringList results;
    QDirIterator it(rootPath, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString filePath = it.next();
        if (isAudioFile(filePath, extensions)) {
            results << filePath;
        }
    }
    return results;
}

QString FilesystemService::fileExtension(const QString &filePath)
{
    return QFileInfo(filePath).suffix().toLower().prepend('.');
}

bool FilesystemService::isAudioFile(const QString &filePath, const QStringList &extensions) const
{
    if (extensions.isEmpty()) {
        return true;
    }
    const QString ext = fileExtension(filePath);
    for (const QString &supported : extensions) {
        if (ext == supported) {
            return true;
        }
    }
    return false;
}

bool FilesystemService::safeRename(const QString &oldPath, const QString &newPath)
{
    if (oldPath == newPath) {
        return true;
    }
    QDir dir;
    if (!dir.rename(oldPath, newPath)) {
        return false;
    }
    return true;
}

QString FilesystemService::uniqueFilePath(const QString &desiredPath)
{
    QFileInfo info(desiredPath);
    if (!info.exists()) {
        return desiredPath;
    }

    const QString dir = info.absolutePath();
    const QString base = info.completeBaseName();
    const QString ext = info.suffix();

    for (int i = 1; i < 10000; ++i) {
        QString candidate = dir + "/" + base + " (" + QString::number(i) + ")";
        if (!ext.isEmpty()) {
            candidate += "." + ext;
        }
        if (!QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
    return desiredPath;
}

} // namespace tagit


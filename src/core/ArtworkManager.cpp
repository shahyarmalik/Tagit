#include "ArtworkManager.h"

#include <QDir>
#include <QFile>
#include <QCryptographicHash>
#include <QFileInfo>

namespace tagit {

ArtworkManager::ArtworkManager(const QString &cacheDir, QObject *parent)
    : QObject(parent)
    , m_cacheDir(cacheDir)
{
    QDir().mkpath(m_cacheDir);
}

QByteArray ArtworkManager::cachedArtwork(const Song &song) const
{
    const QString path = artworkCachePath(song);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

bool ArtworkManager::cacheArtwork(const Song &song, const QByteArray &data)
{
    if (data.isEmpty()) {
        return false;
    }
    QFile file(artworkCachePath(song));
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    return file.write(data) == data.size();
}

bool ArtworkManager::hasArtwork(const Song &song) const
{
    if (song.metadata.hasEmbeddedArtwork) {
        return true;
    }
    return QFile::exists(artworkCachePath(song));
}

void ArtworkManager::clearCache()
{
    QDir dir(m_cacheDir);
    const QStringList entries = dir.entryList(QDir::Files);
    for (const QString &entry : entries) {
        dir.remove(entry);
    }
}

QString ArtworkManager::artworkCachePath(const Song &song) const
{
    const QByteArray hash = QCryptographicHash::hash(song.filePath.toUtf8(),
                                                     QCryptographicHash::Sha1);
    return m_cacheDir + "/" + QString::fromLatin1(hash.toHex()) + ".img";
}

} // namespace tagit


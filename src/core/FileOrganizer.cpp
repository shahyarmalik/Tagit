#include "FileOrganizer.h"

#include <QDir>
#include <QFileInfo>

namespace tagit {

FileOrganizer::FileOrganizer(const QString &targetRoot)
    : m_targetRoot(targetRoot)
{
}

QString FileOrganizer::destinationPath(const Song &song, const QString &pattern) const
{
    QString result = pattern;

    auto safeName = [](const QString &value) -> QString {
        QString cleaned = value.trimmed();
        cleaned.replace('/', '_').replace('\\', '_');
        cleaned.replace(':', '_').replace('*', '_').replace('?', '_');
        cleaned.replace('"', '_').replace('<', '_').replace('>', '_');
        cleaned.replace('|', '_');
        return cleaned.isEmpty() ? QStringLiteral("Unknown") : cleaned;
    };

    result.replace("{artist}", safeName(song.metadata.artist));
    result.replace("{album}", safeName(song.metadata.album));
    result.replace("{title}", safeName(song.metadata.title));

    const QString track = song.metadata.trackNumber > 0
        ? QString("%1").arg(song.metadata.trackNumber, 2, 10, QLatin1Char('0'))
        : QStringLiteral("00");
    result.replace("{track}", track);

    if (!result.endsWith('.')) {
        result += "." + song.format;
    }

    return m_targetRoot + "/" + result;
}

QString FileOrganizer::organizeSong(const Song &song, const QString &pattern) const
{
    const QString dest = destinationPath(song, pattern);
    const QFileInfo destInfo(dest);
    QDir().mkpath(destInfo.absolutePath());

    const QString uniqueDest = m_fs.uniqueFilePath(dest);
    if (!m_fs.safeRename(song.filePath, uniqueDest)) {
        return {};
    }
    return uniqueDest;
}

QVector<QPair<Song, QString>> FileOrganizer::preview(const QVector<Song> &songs,
                                                     const QString &pattern) const
{
    QVector<QPair<Song, QString>> result;
    result.reserve(songs.size());
    for (const Song &song : songs) {
        result.append({song, destinationPath(song, pattern)});
    }
    return result;
}

QString FileOrganizer::targetRoot() const
{
    return m_targetRoot;
}

} // namespace tagit


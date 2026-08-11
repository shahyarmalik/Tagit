#include "SongTableModel.h"

namespace tagit {

SongTableModel::SongTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int SongTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_songs.size());
}

int SongTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return ColumnCount;
}

QVariant SongTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_songs.size()) {
        return {};
    }

    const Song &song = m_songs.at(index.row());

    if (role == Qt::DisplayRole || role == Qt::ToolTipRole) {
        switch (index.column()) {
        case ColTitle:   return song.metadata.title;
        case ColArtist:  return song.metadata.artist;
        case ColAlbum:   return song.metadata.album;
        case ColGenre:   return song.metadata.genre;
        case ColTrack:
            return song.metadata.trackNumber > 0 ? song.metadata.trackNumber : QVariant();
        case ColDuration:
            return song.metadata.durationMs > 0 ? formatDuration(song.metadata.durationMs) : QVariant();
        case ColFormat:  return song.format;
        case ColPath:    return song.filePath;
        default:         return {};
        }
    }

    if (role == Qt::TextAlignmentRole && index.column() == ColTrack) {
        return int(Qt::AlignRight | Qt::AlignVCenter);
    }

    if (role == Qt::UserRole) {
        return song.id;
    }

    return {};
}

QVariant SongTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) {
        return {};
    }
    switch (section) {
    case ColTitle:    return tr("Title");
    case ColArtist:   return tr("Artist");
    case ColAlbum:    return tr("Album");
    case ColGenre:    return tr("Genre");
    case ColTrack:    return tr("#");
    case ColDuration: return tr("Length");
    case ColFormat:   return tr("Format");
    case ColPath:     return tr("Path");
    default:          return {};
    }
}

void SongTableModel::rebuildIndex()
{
    m_pathIndex.clear();
    for (int i = 0; i < m_songs.size(); ++i) {
        m_pathIndex.insert(m_songs.at(i).filePath, i);
    }
}

void SongTableModel::setSongs(const QVector<Song> &songs)
{
    beginResetModel();
    m_songs = songs;
    rebuildIndex();
    endResetModel();
}

void SongTableModel::addOrUpdateSong(const Song &song)
{
    auto it = m_pathIndex.find(song.filePath);
    if (it != m_pathIndex.end()) {
        const int row = it.value();
        if (row >= 0 && row < m_songs.size()) {
            m_songs[row] = song;
            emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
        }
    } else {
        const int row = static_cast<int>(m_songs.size());
        beginInsertRows(QModelIndex(), row, row);
        m_songs.append(song);
        m_pathIndex.insert(song.filePath, row);
        endInsertRows();
    }
}

void SongTableModel::appendSongs(const QVector<Song> &songs)
{
    if (songs.isEmpty()) return;
    const int start = static_cast<int>(m_songs.size());
    const int end = start + static_cast<int>(songs.size()) - 1;
    beginInsertRows(QModelIndex(), start, end);
    for (int i = 0; i < songs.size(); ++i) {
        m_songs.append(songs.at(i));
        m_pathIndex.insert(songs.at(i).filePath, start + i);
    }
    endInsertRows();
}

void SongTableModel::updateSongMetadata(const QString &filePath, const AudioMetadata &metadata)
{
    auto it = m_pathIndex.find(filePath);
    if (it != m_pathIndex.end()) {
        const int row = it.value();
        if (row >= 0 && row < m_songs.size()) {
            m_songs[row].metadata = metadata;
            emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
        }
    }
}

QVector<Song> SongTableModel::songs() const
{
    return m_songs;
}

void SongTableModel::clear()
{
    beginResetModel();
    m_songs.clear();
    m_pathIndex.clear();
    endResetModel();
}

Song SongTableModel::songAt(int row) const
{
    if (row < 0 || row >= m_songs.size()) {
        return {};
    }
    return m_songs.at(row);
}

QString SongTableModel::formatDuration(qint64 ms)
{
    const qint64 totalSeconds = ms / 1000;
    const qint64 minutes = totalSeconds / 60;
    const qint64 seconds = totalSeconds % 60;
    return QStringLiteral("%1:%2")
        .arg(minutes)
        .arg(seconds, 2, 10, QLatin1Char('0'));
}

} // namespace tagit


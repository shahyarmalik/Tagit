#ifndef TAGIT_SONG_TABLE_MODEL_H
#define TAGIT_SONG_TABLE_MODEL_H

#include <QAbstractTableModel>
#include <QVector>

#include "../model/Song.h"

namespace tagit {

/**
 * @brief Table model exposing the library song list to the UI.
 *
 * Columns: Title, Artist, Album, Genre, Track, Duration, Format, Path.
 */
class SongTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column {
        ColTitle = 0,
        ColArtist,
        ColAlbum,
        ColGenre,
        ColTrack,
        ColDuration,
        ColFormat,
        ColPath,
        ColumnCount
    };

    explicit SongTableModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    void setSongs(const QVector<Song> &songs);
    void addOrUpdateSong(const Song &song);
    void appendSongs(const QVector<Song> &songs);
    void updateSongMetadata(const QString &filePath, const AudioMetadata &metadata);
    QVector<Song> songs() const;
    void clear();

    Song songAt(int row) const;

private:
    static QString formatDuration(qint64 ms);
    void rebuildIndex();

    QVector<Song> m_songs;
    QHash<QString, int> m_pathIndex;
};

} // namespace tagit

#endif // TAGIT_SONG_TABLE_MODEL_H


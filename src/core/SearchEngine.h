#ifndef TAGIT_SEARCH_ENGINE_H
#define TAGIT_SEARCH_ENGINE_H

#include <QString>
#include <QVector>

#include "../model/Song.h"

namespace tagit {

/**
 * @brief Full-text search over the library index.
 *
 * Matches against title, artist, album, genre, filename and path. Results are
 * ranked with a simple scoring model (exact prefix matches rank higher).
 */
class SearchEngine {
public:
    SearchEngine() = default;

    /// Return songs matching @p query, ranked best-first.
    QVector<Song> search(const QVector<Song> &songs, const QString &query) const;

private:
    int score(const Song &song, const QString &query) const;
    bool matches(const QString &haystack, const QString &needle) const;
};

} // namespace tagit

#endif // TAGIT_SEARCH_ENGINE_H


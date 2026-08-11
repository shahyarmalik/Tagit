#include "SearchEngine.h"

#include <algorithm>
#include <QPair>

namespace tagit {

QVector<Song> SearchEngine::search(const QVector<Song> &songs, const QString &query) const
{
    const QString trimmed = query.trimmed();
    if (trimmed.isEmpty()) {
        return songs;
    }

    QVector<QPair<Song, int>> ranked;
    for (const Song &song : songs) {
        const int s = score(song, trimmed);
        if (s > 0) {
            ranked.append({song, s});
        }
    }

    std::stable_sort(ranked.begin(), ranked.end(),
              [](const QPair<Song, int> &a, const QPair<Song, int> &b) {
                  return a.second > b.second;
              });

    QVector<Song> results;
    results.reserve(ranked.size());
    for (const auto &entry : ranked) {
        results.append(entry.first);
    }
    return results;
}

int SearchEngine::score(const Song &song, const QString &query) const
{
    int total = 0;
    const QString lowerQuery = query.toLower();

    auto fieldScore = [&](const QString &field, int weight) {
        if (field.isEmpty()) {
            return;
        }
        const QString lower = field.toLower();
        if (lower == lowerQuery) {
            total += 4 * weight;            // exact match
        } else if (lower.startsWith(lowerQuery)) {
            total += 3 * weight;            // prefix match
        } else if (lower.contains(lowerQuery)) {
            total += weight;                // substring match
        }
    };

    fieldScore(song.metadata.title, 5);
    fieldScore(song.metadata.artist, 4);
    fieldScore(song.metadata.album, 3);
    fieldScore(song.metadata.genre, 2);
    fieldScore(song.fileName, 2);
    fieldScore(song.filePath, 1);
    return total;
}

bool SearchEngine::matches(const QString &haystack, const QString &needle) const
{
    return haystack.contains(needle, Qt::CaseInsensitive);
}

} // namespace tagit


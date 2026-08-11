#ifndef TAGIT_NETWORK_SERVICE_H
#define TAGIT_NETWORK_SERVICE_H

#include <QObject>
#include <QString>
#include <QQueue>
#include <QMap>
#include <QVector>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <memory>

#include "../model/AudioMetadata.h"

namespace tagit {

/**
 * @brief Online metadata + cover art provider with cross-provider voting.
 *
 * For every lookup all four providers are queried IN PARALLEL:
 *   • YouTube        — best for video titles, fan-uploads, AMVs, unofficial releases
 *   • iTunes Search  — best for mainstream/commercial releases
 *   • MusicBrainz    — best for open metadata, composers, ISRC
 *   • Deezer Search  — best for mainstream streaming, cover art, track info
 *
 * When all (or all available) providers have responded, a per-field
 * majority vote is run:
 *   - For each field (title, artist, album …) collect the non-empty
 *     values returned by each provider.
 *   - The value that appears in the most provider results wins.
 *   - If there is a tie, the result from the higher-scoring provider wins.
 *   - The winning value's confidence is set to the vote fraction × 100
 *     (e.g. 2/3 providers agree → confidence 66; 3/3 → 100).
 *
 * After the consensus metadata is built, cover art is downloaded from
 * the best available artwork URL.
 *
 * A single lookupAggregated signal carries the final consensus result.
 */
class NetworkService : public QObject {
    Q_OBJECT

public:
    explicit NetworkService(QObject *parent = nullptr);
    ~NetworkService() override;

    void setEnabled(bool enabled);
    bool isEnabled() const;

    /// Queue a lookup for @p artist / @p title.
    void lookup(const QString &artist, const QString &title);

    /// Download artwork from @p artworkUrl and re-emit lookupAggregated
    /// with artworkData populated.
    void downloadArtwork(const QString &artworkUrl,
                         const QString &artist,
                         const QString &title,
                         const AudioMetadata &metadata);

signals:
    /**
     * @brief Emitted once all providers have responded (or timed out).
     *
     * @p metadata is the per-field consensus result.
     * The original query (@p artist, @p title) is echoed back so
     * MetadataEngine can match the reply to the right song.
     */
    void lookupAggregated(const QString &artist, const QString &title,
                          const AudioMetadata &metadata);

    void lookupFailed(const QString &artist, const QString &title,
                      const QString &error);

    // Legacy compat — wired to lookupAggregated internally
    void lookupFinished(const QString &artist, const QString &title,
                        const AudioMetadata &metadata);

private slots:
    void processNextInQueue();
    void onYouTubeReply(QNetworkReply *reply,
                        const QString &artist, const QString &title);
    void onItunesReply(QNetworkReply *reply,
                       const QString &artist, const QString &title);
    void onMusicBrainzReply(QNetworkReply *reply,
                            const QString &artist, const QString &title);
    void onDeezerReply(QNetworkReply *reply,
                       const QString &artist, const QString &title);
    void onArtworkReply(QNetworkReply *reply, const QString &artist,
                        const QString &title, AudioMetadata metadata);

private:
    // Fire all four providers simultaneously for one query key
    void fireLookupKey(const QString &artist, const QString &title);
    void fireYouTubeRequest(const QString &artist, const QString &title);
    void fireItunesRequest(const QString &artist, const QString &title);
    void fireMusicBrainzRequest(const QString &artist, const QString &title);
    void fireDeezerRequest(const QString &artist, const QString &title);

    // Called after each provider replies; triggers aggregation when all done
    void onProviderResult(const QString &key, const QString &provider,
                          const AudioMetadata &result, int score);
    void tryAggregateAndEmit(const QString &key);

    // Voting helpers
    static AudioMetadata voteConsensus(const QVector<AudioMetadata> &results,
                                       const QVector<int>            &scores);
    static QString       mostCommonString(const QVector<QString> &vals,
                                          const QVector<int>     &weights,
                                          int                    &outConf);
    static int           mostCommonInt(const QVector<int> &vals,
                                       const QVector<int> &weights,
                                       int                &outConf);

    // Fuzzy matching helpers
    static int similarity(const QString &a, const QString &b);
    static int scoreMatch(const QString &qArtist, const QString &qTitle,
                          const QString &rArtist, const QString &rTitle);

    // Per-key in-flight state
    struct ProviderState {
        QMap<QString, AudioMetadata> results; // provider name → result
        QMap<QString, int>           scores;  // provider name → score
        int expected = 4;                     // how many providers we fired
        int received = 0;
    };

    bool m_enabled = true;
    int  m_activeRequests = 0;
    const int m_maxConcurrent = 15;

    struct LookupRequest { QString artist; QString title; };
    QQueue<LookupRequest>              m_queue;
    QHash<QString, ProviderState>      m_inFlight;
    QHash<QString, AudioMetadata>      m_cache;
    std::unique_ptr<QNetworkAccessManager> m_nam;

    static QString makeKey(const QString &a, const QString &t)
    { return a.trimmed() + QChar(0x1F) + t.trimmed(); }
};

} // namespace tagit

#endif // TAGIT_NETWORK_SERVICE_H

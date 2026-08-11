#include "NetworkService.h"
#include "../core/Logger.h"
#include "../core/FilenameIntelligence.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <algorithm>
#include <functional>

namespace tagit {

// ===========================================================================
// Fuzzy similarity helpers
// ===========================================================================

static int levenshtein(const QString &a, const QString &b)
{
    const int la = static_cast<int>(a.size());
    const int lb = static_cast<int>(b.size());
    if (la == 0) return lb;
    if (lb == 0) return la;
    QVector<int> prev(lb + 1), curr(lb + 1);
    for (int j = 0; j <= lb; ++j) prev[j] = j;
    for (int i = 1; i <= la; ++i) {
        curr[0] = i;
        for (int j = 1; j <= lb; ++j) {
            const int cost = (a[i-1].toLower() == b[j-1].toLower()) ? 0 : 1;
            curr[j] = std::min({curr[j-1]+1, prev[j]+1, prev[j-1]+cost});
        }
        prev = curr;
    }
    return prev[lb];
}

int NetworkService::similarity(const QString &a, const QString &b)
{
    if (a.isEmpty() && b.isEmpty()) return 100;
    if (a.isEmpty() || b.isEmpty()) return 0;
    const int maxLen = std::max(static_cast<int>(a.size()),
                                static_cast<int>(b.size()));
    const int dist   = levenshtein(a.trimmed().toLower(), b.trimmed().toLower());
    return std::max(0, 100 - (dist * 100 / maxLen));
}

int NetworkService::scoreMatch(const QString &qArtist, const QString &qTitle,
                                const QString &rArtist, const QString &rTitle)
{
    const int titleSim  = similarity(qTitle,  rTitle);
    const int artistSim = qArtist.isEmpty() ? 60
                                            : similarity(qArtist, rArtist);
    const int combined  = (titleSim * 60 + artistSim * 40) / 100;

    int bonus = 0;
    const QString lqTitle  = qTitle.toLower().trimmed();
    const QString lrTitle  = rTitle.toLower().trimmed();
    const QString lqArtist = qArtist.toLower().trimmed();
    const QString lrArtist = rArtist.toLower().trimmed();

    if (!lqTitle.isEmpty() && (lrTitle.contains(lqTitle) || lqTitle.contains(lrTitle))) bonus += 15;
    if (!lqArtist.isEmpty() && (lrArtist.contains(lqArtist) || lqArtist.contains(lrArtist))) bonus += 15;

    return std::min(100, combined + bonus);
}

// ===========================================================================
// Construction
// ===========================================================================

NetworkService::NetworkService(QObject *parent)
    : QObject(parent)
    , m_nam(std::make_unique<QNetworkAccessManager>())
{}

NetworkService::~NetworkService() = default;

void NetworkService::setEnabled(bool e) { m_enabled = e; }
bool NetworkService::isEnabled()  const { return m_enabled; }

void NetworkService::lookup(const QString &artist, const QString &title)
{
    if (!m_enabled) { emit lookupFailed(artist, title, "Disabled"); return; }
    
    const QString key = makeKey(artist, title);
    if (m_cache.contains(key)) {
        AudioMetadata cached = m_cache.value(key);
        QTimer::singleShot(0, this, [this, artist, title, cached]() {
            emit lookupAggregated(artist, title, cached);
            emit lookupFinished(artist, title, cached);
        });
        return;
    }

    m_queue.enqueue({artist, title});
    processNextInQueue();
}

void NetworkService::processNextInQueue()
{
    while (m_activeRequests < m_maxConcurrent && !m_queue.isEmpty()) {
        ++m_activeRequests;
        const LookupRequest req = m_queue.dequeue();
        fireLookupKey(req.artist, req.title);
    }
}

// ---------------------------------------------------------------------------
// Fire all three providers simultaneously
// ---------------------------------------------------------------------------
void NetworkService::fireLookupKey(const QString &artist, const QString &title)
{
    const QString key = makeKey(artist, title);
    ProviderState &st = m_inFlight[key];
    st.expected = 4;
    st.received = 0;
    st.results.clear();
    st.scores.clear();

    fireYouTubeRequest(artist, title);
    fireItunesRequest(artist, title);
    fireMusicBrainzRequest(artist, title);
    fireDeezerRequest(artist, title);
}

// ---------------------------------------------------------------------------
// Called after every provider reply — aggregates when all done
// ---------------------------------------------------------------------------
void NetworkService::onProviderResult(const QString &key,
                                       const QString &provider,
                                       const AudioMetadata &result,
                                       int score)
{
    if (!m_inFlight.contains(key)) return;
    ProviderState &st = m_inFlight[key];

    if (score > 0) {
        st.results[provider] = result;
        st.scores[provider]  = score;
    }
    ++st.received;
    tryAggregateAndEmit(key);
}

void NetworkService::tryAggregateAndEmit(const QString &key)
{
    ProviderState &st = m_inFlight[key];
    if (st.received < st.expected) return;  // still waiting

    // Split the key back into artist + title
    const int sep    = static_cast<int>(key.indexOf(QChar(0x1F)));
    const QString qa = key.left(sep);
    const QString qt = key.mid(sep + 1);

    if (st.results.isEmpty()) {
        emit lookupFailed(qa, qt, "No provider matched");
        m_inFlight.remove(key);
        --m_activeRequests;
        QTimer::singleShot(10, this, &NetworkService::processNextInQueue);
        return;
    }

    // Build ordered vectors for the vote
    QVector<AudioMetadata> results;
    QVector<int>           scores;
    for (const QString &p : st.results.keys()) {
        results.append(st.results[p]);
        scores.append(st.scores[p]);
    }

    AudioMetadata consensus = voteConsensus(results, scores);

    // Pick the best artwork URL (stored in comment) from highest-scoring result
    QString artworkUrl;
    int     bestScore = -1;
    for (int i = 0; i < results.size(); ++i) {
        if (!results[i].comment.isEmpty() && scores[i] > bestScore) {
            bestScore  = scores[i];
            artworkUrl = results[i].comment;
        }
    }

    const int N = static_cast<int>(results.size());
    Logger::info(
        QStringLiteral("Consensus (%1/%2 providers): '%3 – %4' "
                        "album='%5' genre='%6' conf=%7")
            .arg(N).arg(st.expected)
            .arg(consensus.artist, consensus.title,
                 consensus.album,  consensus.genre)
            .arg(consensus.confidence.overall()));

    m_inFlight.remove(key);

    if (!artworkUrl.isEmpty() && !consensus.hasEmbeddedArtwork
        && consensus.artworkData.isEmpty()) {
        downloadArtwork(artworkUrl, qa, qt, consensus);
    } else {
        m_cache.insert(key, consensus);
        emit lookupAggregated(qa, qt, consensus);
        emit lookupFinished(qa, qt, consensus);
        --m_activeRequests;
        QTimer::singleShot(10, this, &NetworkService::processNextInQueue);
    }
}

// ===========================================================================
// Voting / consensus
// ===========================================================================

/// Normalise a string for comparison: lowercase, collapse spaces
static QString norm(const QString &s) { return s.trimmed().toLower().simplified(); }

QString NetworkService::mostCommonString(const QVector<QString> &vals,
                                          const QVector<int>     &weights,
                                          int                    &outConf)
{
    outConf = 0;
    if (vals.isEmpty()) return {};

    // Group by normalised value; accumulate weights
    QMap<QString, int>    weightSum;  // norm → total weight
    QMap<QString, QString> original;  // norm → first original
    int total = 0;
    for (int i = 0; i < vals.size(); ++i) {
        const QString n = norm(vals[i]);
        if (n.isEmpty()) continue;
        weightSum[n]  += weights[i];
        total         += weights[i];
        if (!original.contains(n)) original[n] = vals[i];
    }
    if (weightSum.isEmpty()) return {};

    // Pick highest weight
    QString bestNorm;
    int     bestW = 0;
    for (auto it = weightSum.begin(); it != weightSum.end(); ++it) {
        if (it.value() > bestW) { bestW = it.value(); bestNorm = it.key(); }
    }

    outConf = total > 0 ? (bestW * 100 / total) : 0;
    return original.value(bestNorm);
}

int NetworkService::mostCommonInt(const QVector<int> &vals,
                                   const QVector<int> &weights,
                                   int                &outConf)
{
    outConf = 0;
    if (vals.isEmpty()) return 0;

    QMap<int, int> weightSum;
    int total = 0;
    for (int i = 0; i < vals.size(); ++i) {
        if (vals[i] == 0) continue;
        weightSum[vals[i]] += weights[i];
        total              += weights[i];
    }
    if (weightSum.isEmpty()) return 0;

    int bestVal = 0, bestW = 0;
    for (auto it = weightSum.begin(); it != weightSum.end(); ++it) {
        if (it.value() > bestW) { bestW = it.value(); bestVal = it.key(); }
    }

    outConf = total > 0 ? (bestW * 100 / total) : 0;
    return bestVal;
}

AudioMetadata NetworkService::voteConsensus(const QVector<AudioMetadata> &results,
                                             const QVector<int>            &scores)
{
    AudioMetadata out;
    if (results.isEmpty()) return out;
    if (results.size() == 1) {
        out = results[0];
        out.comment.clear();
        return out;
    }

    // Collect per-field value vectors
    auto strField = [&](auto getter, auto &dst, int &conf) {
        QVector<QString> vals;
        for (const auto &r : results) vals.append(getter(r));
        dst = mostCommonString(vals, scores, conf);
    };
    auto intField = [&](auto getter, auto &dst, int &conf) {
        QVector<int> vals;
        for (const auto &r : results) vals.append(getter(r));
        dst = mostCommonInt(vals, scores, conf);
    };

    strField([](const AudioMetadata &r){ return r.title;       }, out.title,       out.confidence.title);
    strField([](const AudioMetadata &r){ return r.artist;      }, out.artist,      out.confidence.artist);
    strField([](const AudioMetadata &r){ return r.album;       }, out.album,       out.confidence.album);
    strField([](const AudioMetadata &r){ return r.albumArtist; }, out.albumArtist, out.confidence.albumArtist);
    strField([](const AudioMetadata &r){ return r.genre;       }, out.genre,       out.confidence.genre);
    strField([](const AudioMetadata &r){ return r.composer;    }, out.composer,    out.confidence.composer);
    strField([](const AudioMetadata &r){ return r.lyrics;      }, out.lyrics,      out.confidence.lyrics);
    strField([](const AudioMetadata &r){ return r.isrc;        }, out.isrc,        out.confidence.title);

    intField([](const AudioMetadata &r){ return r.year;        }, out.year,        out.confidence.year);
    intField([](const AudioMetadata &r){ return r.trackNumber; }, out.trackNumber, out.confidence.trackNumber);
    intField([](const AudioMetadata &r){ return r.discNumber;  }, out.discNumber,  out.confidence.discNumber);

    // comment field is not voted — used as artwork URL carrier, pick best score
    return out;
}

// ===========================================================================
// Provider 0: YouTube & YouTube Music
// ===========================================================================

void NetworkService::fireYouTubeRequest(const QString &artist,
                                        const QString &title)
{
    const QString query = (artist.isEmpty() ? title : artist + " " + title).trimmed();

    QUrlQuery q;
    q.addQueryItem("search_query", query);
    q.addQueryItem("sp",  "EgIQAQ==");
    q.addQueryItem("pbj", "1");

    QUrl url("https://www.youtube.com/results");
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("TagIt/%1").arg(QStringLiteral(TAGIT_VERSION)));
    req.setRawHeader("Accept",          "application/json");
    req.setRawHeader("Accept-Language", "en-US,en;q=0.9");
    req.setRawHeader("X-YouTube-Client-Name",    "1");
    req.setRawHeader("X-YouTube-Client-Version", "2.20240101.00.00");

    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, artist, title]() { onYouTubeReply(reply, artist, title); });

    Logger::debug(QStringLiteral("YouTube lookup: %1 — %2").arg(artist, title));
}

void NetworkService::onYouTubeReply(QNetworkReply *reply,
                                     const QString  &artist,
                                     const QString  &title)
{
    reply->deleteLater();
    const QString key = makeKey(artist, title);

    if (reply->error() != QNetworkReply::NoError) {
        Logger::debug("YouTube error: " + reply->errorString());
        onProviderResult(key, "youtube", {}, 0);
        return;
    }

    const QJsonDocument doc  = QJsonDocument::fromJson(reply->readAll());
    const QJsonArray    root = doc.array();

    AudioMetadata best;
    int           bestScore = -1;

    std::function<void(const QJsonValue &)> walk = [&](const QJsonValue &v) {
        if (v.isObject()) {
            const QJsonObject obj = v.toObject();
            if (obj.contains("videoRenderer")) {
                const QJsonObject vr = obj["videoRenderer"].toObject();
                const QString videoId = vr["videoId"].toString();
                const QString vTitle  =
                    vr["title"].toObject()["runs"].toArray()
                        .first().toObject()["text"].toString().trimmed();
                const QString channel =
                    vr["ownerText"].toObject()["runs"].toArray()
                        .first().toObject()["text"].toString().trimmed();

                FilenameIntelligence fi;
                const QString cleanedTitle = fi.cleanFilename(vTitle);
                const AudioMetadata parsedFromTitle = fi.parse(cleanedTitle);

                QString rArtist = parsedFromTitle.artist.isEmpty() ? channel : parsedFromTitle.artist;
                QString rTitle  = parsedFromTitle.title.isEmpty() ? cleanedTitle : parsedFromTitle.title;

                const int sc = scoreMatch(artist, title, rArtist, rTitle);
                if (sc > bestScore && !videoId.isEmpty()) {
                    bestScore = sc;
                    best.title  = rTitle;
                    best.artist = rArtist;
                    best.comment = QStringLiteral(
                        "https://i.ytimg.com/vi/%1/maxresdefault.jpg").arg(videoId);
                    best.confidence.title  = sc;
                    best.confidence.artist = sc;
                }
            }
            for (const QString &k : obj.keys()) walk(obj[k]);
        } else if (v.isArray()) {
            for (const QJsonValue &item : v.toArray()) walk(item);
        }
    };
    for (const QJsonValue &item : root) walk(item);

    if (bestScore >= 25) {
        Logger::debug(QStringLiteral("YouTube: score %1 → '%2 – %3'")
                          .arg(bestScore).arg(best.artist, best.title));
        onProviderResult(key, "youtube", best, bestScore);
    } else {
        onProviderResult(key, "youtube", {}, 0);
    }
}

// ===========================================================================
// Provider 1: iTunes Search API
// ===========================================================================

void NetworkService::fireItunesRequest(const QString &artist, const QString &title)
{
    const QString term = (artist.isEmpty() ? title : artist + " " + title).trimmed();
    QUrlQuery q;
    q.addQueryItem("term",   term);
    q.addQueryItem("media",  "music");
    q.addQueryItem("entity", "song");
    q.addQueryItem("limit",  "15");

    QUrl url("https://itunes.apple.com/search");
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("TagIt/%1").arg(QStringLiteral(TAGIT_VERSION)));
    req.setRawHeader("Accept", "application/json");

    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, artist, title]() { onItunesReply(reply, artist, title); });

    Logger::debug(QStringLiteral("iTunes lookup: %1 — %2").arg(artist, title));
}

void NetworkService::onItunesReply(QNetworkReply *reply,
                                    const QString  &artist,
                                    const QString  &title)
{
    reply->deleteLater();
    const QString key = makeKey(artist, title);

    if (reply->error() != QNetworkReply::NoError) {
        Logger::warn("iTunes error: " + reply->errorString());
        onProviderResult(key, "itunes", {}, 0);
        return;
    }

    const QJsonArray results =
        QJsonDocument::fromJson(reply->readAll()).object()["results"].toArray();

    AudioMetadata best;
    int bestScore = -1;

    for (const QJsonValue &v : results) {
        const QJsonObject obj     = v.toObject();
        const QString rArtist = obj["artistName"].toString().trimmed();
        const QString rTitle  = obj["trackName"].toString().trimmed();
        const int sc = scoreMatch(artist, title, rArtist, rTitle);
        if (sc > bestScore) {
            bestScore = sc;
            best.artist      = rArtist;
            best.title       = rTitle;
            best.album       = obj["collectionName"].toString().trimmed();
            best.albumArtist = rArtist;
            best.genre       = obj["primaryGenreName"].toString().trimmed();
            const QString rd = obj["releaseDate"].toString();
            if (!rd.isEmpty()) best.year = rd.left(4).toInt();
            const QString au = obj["artworkUrl100"].toString();
            best.comment     = au.isEmpty() ? QString()
                             : QString(au).replace("100x100", "600x600");
            best.confidence.title  = sc;
            best.confidence.artist = sc;
            best.confidence.album  = sc >= 70 ? 85 : 70;
            best.confidence.genre  = 80;
            best.confidence.year   = 80;
        }
    }

    if (bestScore >= 25) {
        Logger::debug(QStringLiteral("iTunes: score %1 → '%2 – %3'")
                          .arg(bestScore).arg(best.artist, best.title));
        onProviderResult(key, "itunes", best, bestScore);
    } else {
        onProviderResult(key, "itunes", {}, 0);
    }
}

// ===========================================================================
// Provider 2: MusicBrainz
// ===========================================================================

void NetworkService::fireMusicBrainzRequest(const QString &artist,
                                              const QString &title)
{
    QString cleanTitle = title;
    cleanTitle = cleanTitle.remove('"').remove('(').remove(')').remove('[').remove(']').trimmed();
    QString cleanArtist = artist;
    cleanArtist = cleanArtist.remove('"').remove('(').remove(')').remove('[').remove(']').trimmed();

    QString lucene;
    if (!cleanArtist.isEmpty()) {
        lucene = QStringLiteral("recording:(%1) AND (artist:(%2) OR artistname:(%2))")
                     .arg(cleanTitle, cleanArtist);
    } else {
        lucene = QStringLiteral("recording:(%1)").arg(cleanTitle);
    }

    QUrlQuery q;
    q.addQueryItem("query", lucene);
    q.addQueryItem("limit", "15");
    q.addQueryItem("fmt",   "json");

    QUrl url("https://musicbrainz.org/ws/2/recording/");
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("TagIt/%1 (tagit.app)")
                      .arg(QStringLiteral(TAGIT_VERSION)));
    req.setRawHeader("Accept", "application/json");

    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, artist, title]() { onMusicBrainzReply(reply, artist, title); });

    Logger::debug(QStringLiteral("MusicBrainz lookup: %1 — %2").arg(artist, title));
}

void NetworkService::onMusicBrainzReply(QNetworkReply *reply,
                                         const QString  &artist,
                                         const QString  &title)
{
    reply->deleteLater();
    const QString key = makeKey(artist, title);

    if (reply->error() != QNetworkReply::NoError) {
        Logger::warn("MusicBrainz error: " + reply->errorString());
        onProviderResult(key, "musicbrainz", {}, 0);
        return;
    }

    const QJsonArray recs =
        QJsonDocument::fromJson(reply->readAll()).object()["recordings"].toArray();

    AudioMetadata best;
    int bestScore = -1;

    for (const QJsonValue &v : recs) {
        const QJsonObject rec    = v.toObject();
        const QString     rTitle = rec["title"].toString().trimmed();
        QString rArtist;
        const QJsonArray credits = rec["artist-credit"].toArray();
        if (!credits.isEmpty())
            rArtist = credits[0].toObject()["artist"].toObject()["name"].toString().trimmed();

        const int sc = scoreMatch(artist, title, rArtist, rTitle);
        if (sc <= bestScore) continue;
        bestScore = sc;

        best.artist = rArtist;
        best.title  = rTitle;

        const QJsonArray releases = rec["releases"].toArray();
        if (!releases.isEmpty()) {
            const QJsonObject rel = releases[0].toObject();
            best.album = rel["title"].toString().trimmed();
            const QString d = rel["date"].toString();
            if (!d.isEmpty()) best.year = d.left(4).toInt();
        }

        const QJsonArray isrcs = rec["isrcs"].toArray();
        if (!isrcs.isEmpty()) best.isrc = isrcs[0].toString();

        const QJsonArray rels = rec["relations"].toArray();
        for (const QJsonValue &r : rels) {
            const QJsonObject ro = r.toObject();
            if (ro["type"].toString() == "composer") {
                best.composer = ro["artist"].toObject()["name"].toString().trimmed();
                break;
            }
        }

        const QJsonArray tags = rec["tags"].toArray();
        int topVote = -1;
        for (const QJsonValue &t : tags) {
            const int vote = t.toObject()["count"].toInt();
            if (vote > topVote) { topVote = vote; best.genre = t.toObject()["name"].toString(); }
        }

        best.confidence.title  = sc;
        best.confidence.artist = sc;
        best.confidence.album  = sc >= 70 ? 80 : 65;
        best.confidence.genre  = 70;
    }

    if (bestScore >= 25) {
        Logger::debug(QStringLiteral("MusicBrainz: score %1 → '%2 – %3'")
                          .arg(bestScore).arg(best.artist, best.title));
        onProviderResult(key, "musicbrainz", best, bestScore);
    } else {
        onProviderResult(key, "musicbrainz", {}, 0);
    }
}

// ===========================================================================
// Provider 3: Deezer
// ===========================================================================

void NetworkService::fireDeezerRequest(const QString &artist,
                                        const QString &title)
{
    const QString query = (artist.isEmpty() ? title : artist + " " + title).trimmed();

    QUrl url("https://api.deezer.com/search");
    QUrlQuery q;
    q.addQueryItem("q", query);
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("TagIt/%1").arg(QStringLiteral(TAGIT_VERSION)));
    req.setRawHeader("Accept", "application/json");

    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, artist, title]() { onDeezerReply(reply, artist, title); });

    Logger::debug(QStringLiteral("Deezer lookup: %1 — %2").arg(artist, title));
}

void NetworkService::onDeezerReply(QNetworkReply *reply,
                                    const QString  &artist,
                                    const QString  &title)
{
    reply->deleteLater();
    const QString key = makeKey(artist, title);

    if (reply->error() != QNetworkReply::NoError) {
        Logger::warn("Deezer error: " + reply->errorString());
        onProviderResult(key, "deezer", {}, 0);
        return;
    }

    const QJsonObject responseObj = QJsonDocument::fromJson(reply->readAll()).object();
    const QJsonArray data = responseObj["data"].toArray();

    AudioMetadata best;
    int bestScore = -1;

    for (const QJsonValue &v : data) {
        const QJsonObject obj     = v.toObject();
        const QString rArtist = obj["artist"].toObject()["name"].toString().trimmed();
        const QString rTitle  = obj["title"].toString().trimmed();
        const int sc = scoreMatch(artist, title, rArtist, rTitle);
        if (sc > bestScore) {
            bestScore = sc;
            best.artist      = rArtist;
            best.title       = rTitle;
            best.album       = obj["album"].toObject()["title"].toString().trimmed();
            best.albumArtist = rArtist;
            
            const QString au = obj["album"].toObject()["cover_xl"].toString();
            best.comment     = au.isEmpty() ? obj["album"].toObject()["cover_big"].toString() : au;

            best.confidence.title  = sc;
            best.confidence.artist = sc;
            best.confidence.album  = sc >= 70 ? 85 : 70;
        }
    }

    if (bestScore >= 25) {
        Logger::debug(QStringLiteral("Deezer: score %1 → '%2 – %3'")
                          .arg(bestScore).arg(best.artist, best.title));
        onProviderResult(key, "deezer", best, bestScore);
    } else {
        onProviderResult(key, "deezer", {}, 0);
    }
}

// ===========================================================================
// Cover art download
// ===========================================================================

void NetworkService::downloadArtwork(const QString       &artworkUrl,
                                      const QString       &artist,
                                      const QString       &title,
                                      const AudioMetadata &metadata)
{
    if (artworkUrl.isEmpty()) return;

    QNetworkRequest req{QUrl(artworkUrl)};
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("TagIt/%1").arg(QStringLiteral(TAGIT_VERSION)));

    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, artist, title, metadata]() {
                onArtworkReply(reply, artist, title, metadata);
            });
}

void NetworkService::onArtworkReply(QNetworkReply *reply,
                                     const QString  &artist,
                                     const QString  &title,
                                     AudioMetadata   metadata)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        const QString url = reply->url().toString();
        if (url.contains("maxresdefault")) {
            const QString fallback = QString(url).replace("maxresdefault", "hqdefault");
            downloadArtwork(fallback, artist, title, metadata);
        } else {
            Logger::debug("Artwork download failed: " + reply->errorString());
            m_cache.insert(makeKey(artist, title), metadata);
            emit lookupAggregated(artist, title, metadata);
            emit lookupFinished(artist, title, metadata);
            --m_activeRequests;
            QTimer::singleShot(10, this, &NetworkService::processNextInQueue);
        }
        return;
    }

    metadata.artworkData = reply->readAll();
    Logger::info(QStringLiteral("Artwork %1 bytes for '%2 – %3'")
                     .arg(metadata.artworkData.size()).arg(artist, title));

    m_cache.insert(makeKey(artist, title), metadata);
    emit lookupAggregated(artist, title, metadata);
    emit lookupFinished(artist, title, metadata);
    --m_activeRequests;
    QTimer::singleShot(10, this, &NetworkService::processNextInQueue);
}

} // namespace tagit

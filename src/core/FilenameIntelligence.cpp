#include "FilenameIntelligence.h"

#include <QRegularExpression>
#include <QFileInfo>
#include <QtAlgorithms>
#include <algorithm>

namespace tagit {

FilenameIntelligence::FilenameIntelligence()
{
    // Download artifacts that are removed from generated metadata.
    m_artifactTokens = {
        QStringLiteral("official"), QStringLiteral("official video"),
        QStringLiteral("official music video"), QStringLiteral("music video"),
        QStringLiteral("official audio"), QStringLiteral("audio"),
        QStringLiteral("lyrics"), QStringLiteral("lyric video"),
        QStringLiteral("visualizer"), QStringLiteral("hd"),
        QStringLiteral("hq"), QStringLiteral("download"),
        QStringLiteral("downloadming"), QStringLiteral("pagalworld"),
        QStringLiteral("mr-jatt"), QStringLiteral("youtube"),
        QStringLiteral("youtube music"), QStringLiteral("mp3"),
        QStringLiteral("m4a"), QStringLiteral("flac"), QStringLiteral("wav"),
        QStringLiteral("video"), QStringLiteral("4k"), QStringLiteral("1080p"),
        QStringLiteral("720p"), QStringLiteral("full video"), QStringLiteral("full song"),
        QStringLiteral("full audio"), QStringLiteral("free download"),
        QStringLiteral("songspk"), QStringLiteral("djpunjab"), QStringLiteral("djmaza"),
        QStringLiteral("lossless"), QStringLiteral("cdrip"), QStringLiteral("webrip")
    };

    // Meaningful descriptors that are preserved.
    m_descriptorTokens = {
        QStringLiteral("live"), QStringLiteral("acoustic"),
        QStringLiteral("remix"), QStringLiteral("cover"),
        QStringLiteral("nightcore"), QStringLiteral("slowed"),
        QStringLiteral("reverb"), QStringLiteral("amv"),
        QStringLiteral("ost"), QStringLiteral("instrumental"),
        QStringLiteral("karaoke"), QStringLiteral("radio edit"),
        QStringLiteral("album version"), QStringLiteral("original mix"),
        QStringLiteral("extended mix"), QStringLiteral("club mix")
    };
}

AudioMetadata FilenameIntelligence::parse(const QString &fileName) const
{
    QString name = stripExtension(fileName);
    if (name.isEmpty()) {
        return {};
    }

    // Strip common web domain brackets / prefixes, e.g. "[Songs.pk] ", "www.site.com - "
    static const QRegularExpression sitePrefixRe(
        QStringLiteral(R"(^(?:\[.*?\]|\(.*?\))\s*[-_–—]?\s*(?=.)|(?:www\.[a-z0-9\-\.]+\.[a-z]{2,5}\s*[-_–—]?\s*))"),
        QRegularExpression::CaseInsensitiveOption);
    name.remove(sitePrefixRe);

    // Extract a leading track number ("01 ...") and strip it from the name.
    AudioMetadata result = extractTrackNumber(name);

    QStringList tokens = tokenize(name);
    tokens = removeArtifacts(tokens);

    result.mergeMissing(extractArtistTitle(tokens));

    // Preserved descriptors ("(Live)", "(Acoustic)", ...) are appended to the
    // title so "Artist - Title (Live)" becomes title "Title Live".
    const QStringList descriptors = extractDescriptorList(tokens);
    if (!descriptors.isEmpty()) {
        QString title = result.title;
        if (!title.isEmpty()) {
            title += ' ';
        }
        title += descriptors.join(' ');
        result.title = title;
        result.confidence.title = std::max(result.confidence.title, 85);
    }

    return result;
}

bool FilenameIntelligence::canExtractArtistTitle(const QString &fileName) const
{
    const QString name = stripExtension(fileName);
    const QStringList tokens = tokenize(name);
    return tokens.contains("-");
}

QString FilenameIntelligence::stripExtension(const QString &fileName) const
{
    return QFileInfo(fileName).completeBaseName();
}

QStringList FilenameIntelligence::tokenize(QString name) const
{
    // Replace underscores, em-dashes, en-dashes, pipes, tildes with standard delimiters
    name.replace('_', ' ');
    name.replace(QStringLiteral("–"), QStringLiteral(" - "));
    name.replace(QStringLiteral("—"), QStringLiteral(" - "));
    name.replace(QStringLiteral("|"), QStringLiteral(" - "));
    name.replace(QStringLiteral("~"), QStringLiteral(" - "));
    name = name.simplified();

    // Scan character-by-character so separators ("-") and parenthesized
    // groups ("(Official Audio)") are emitted as distinct tokens, and
    // whitespace separates individual words.
    QStringList tokens;
    QString current;
    int i = 0;
    const int n = static_cast<int>(name.size());
    auto flush = [&tokens, &current]() {
        if (!current.trimmed().isEmpty()) {
            tokens << current.trimmed();
            current.clear();
        }
    };
    while (i < n) {
        const QChar ch = name.at(i);
        if (ch == '(' || ch == '[') {
            const QChar close = (ch == '(') ? QLatin1Char(')') : QLatin1Char(']');
            int j = i + 1;
            while (j < n && name.at(j) != close) {
                ++j;
            }
            const QString group = name.mid(i, j - i + 1);
            flush();
            tokens << group;
            i = j + 1;
        } else if (ch == '-' || ch == ')' || ch == ']') {
            flush();
            tokens << QString(ch);
            ++i;
        } else if (ch.isSpace()) {
            flush();
            ++i;
        } else {
            current += ch;
            ++i;
        }
    }
    flush();
    return tokens;
}

QStringList FilenameIntelligence::removeArtifacts(const QStringList &tokens) const
{
    QStringList filtered;
    for (const QString &token : tokens) {
        // Keep structural separators emitted by the tokenizer.
        if (token == "-" || token == "(" || token == ")"
            || token == "[" || token == "]") {
            filtered << token;
            continue;
        }

        // For parenthesized groups, evaluate the inner text.
        QString inner = token;
        if ((token.startsWith('(') && token.endsWith(')'))
            || (token.startsWith('[') && token.endsWith(']'))) {
            inner = token.mid(1, token.size() - 2);
        }

        const QString lower = inner.trimmed().toLower();

        bool isArtifact = false;
        for (const QString &artifact : m_artifactTokens) {
            if (lower == artifact) {
                isArtifact = true;
                break;
            }
        }

        // Bitrate patterns: "(320kbps)", "(320k)", "(256kbps)" ...
        if (!isArtifact) {
            static const QRegularExpression bitrateRe(QStringLiteral("^\\d{2,4}\\s*kbps?$"));
            if (bitrateRe.match(lower).hasMatch()) {
                isArtifact = true;
            }
        }

        if (!isArtifact) {
            filtered << token;
        }
    }
    return filtered;
}

AudioMetadata FilenameIntelligence::extractArtistTitle(const QStringList &tokens) const
{
    AudioMetadata result;

    // Find the LAST dash that has real content on both sides.
    // This correctly handles "Artist ft Other - Title" and
    // "Artist - Title - Extra" (takes last dash as the separator).
    int dashIndex = -1;
    for (int i = static_cast<int>(tokens.size()) - 1; i >= 0; --i) {
        if (tokens[i] == "-") {
            // Make sure there is at least one non-separator token on each side.
            bool leftOk  = false;
            bool rightOk = false;
            for (int l = 0; l < i; ++l) {
                if (tokens[l] != "-") { leftOk = true; break; }
            }
            for (int r = i + 1; r < tokens.size(); ++r) {
                if (tokens[r] != "-") { rightOk = true; break; }
            }
            if (leftOk && rightOk) { dashIndex = i; break; }
        }
    }

    if (dashIndex < 0) return result;

    // Collect everything left of the dash as the artist (including ft clauses).
    QStringList artistTokens;
    for (int i = 0; i < dashIndex; ++i) {
        if (tokens[i] != "-") {
            artistTokens << tokens[i];
        }
    }

    // Collect everything right of the dash as the title,
    // skipping parenthesised descriptor groups.
    QStringList titleTokens;
    for (int i = dashIndex + 1; i < tokens.size(); ++i) {
        const QString &tok = tokens[i];
        if (tok.startsWith('(') || tok.startsWith('[')) continue;
        if (tok == "-") continue;
        titleTokens << tok;
    }

    if (!artistTokens.isEmpty()) {
        result.artist = artistTokens.join(' ');
        result.confidence.artist = 95;
    }
    if (!titleTokens.isEmpty()) {
        result.title = titleTokens.join(' ');
        result.confidence.title = 90;
    }
    return result;
}

AudioMetadata FilenameIntelligence::extractTrackNumber(QString &name) const
{
    AudioMetadata result;
    static const QRegularExpression re(QStringLiteral("^\\s*(\\d{1,3})\\s+"));
    const QRegularExpressionMatch match = re.match(name);
    if (match.hasMatch()) {
        result.trackNumber = match.captured(1).toInt();
        result.confidence.trackNumber = 90;
        name.remove(match.capturedStart(), match.capturedLength());
        name = name.trimmed();
    }
    return result;
}

QStringList FilenameIntelligence::extractDescriptorList(const QStringList &tokens) const
{
    QStringList descriptors;
    for (const QString &token : tokens) {
        const bool paren = token.startsWith('(') && token.endsWith(')');
        const bool bracket = token.startsWith('[') && token.endsWith(']');
        if (!paren && !bracket) {
            continue;
        }
        const QString inner = token.mid(1, token.size() - 2).trimmed();
        const QString lower = inner.toLower();
        for (const QString &desc : m_descriptorTokens) {
            // Match descriptors exactly or as a suffix ("Seeb Remix" -> "remix").
            if (lower == desc || lower.endsWith(desc)) {
                descriptors << inner;
                break;
            }
        }
    }
    return descriptors;
}

static QString toTitleCase(const QString &s)
{
    if (s.isEmpty()) return s;
    QStringList parts = s.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (QString &part : parts) {
        if (!part.isEmpty()) {
            part[0] = part[0].toUpper();
            for (int i = 1; i < part.size(); ++i) {
                part[i] = part[i].toLower();
            }
        }
    }
    return parts.join(QLatin1Char(' '));
}

QString FilenameIntelligence::cleanFilename(const QString &fileName) const
{
    QFileInfo fi(fileName);
    QString baseName = fi.completeBaseName();
    const QString ext = fi.suffix();

    if (baseName.isEmpty()) {
        return fileName;
    }

    // Strip common web domain brackets / prefixes
    static const QRegularExpression sitePrefixRe(
        QStringLiteral(R"(^(?:\[.*?\]|\(.*?\))\s*[-_–—]?\s*(?=.)|(?:www\.[a-z0-9\-\.]+\.[a-z]{2,5}\s*[-_–—]?\s*))"),
        QRegularExpression::CaseInsensitiveOption);
    baseName.remove(sitePrefixRe);

    QStringList tokens = tokenize(baseName);
    tokens = removeArtifacts(tokens);

    QString result;
    for (int i = 0; i < tokens.size(); ++i) {
        const QString token = tokens[i];
        if (token == QStringLiteral("-")) {
            if (!result.isEmpty() && !result.endsWith(QLatin1Char(' '))) {
                result += QLatin1Char(' ');
            }
            result += QStringLiteral("- ");
        } else if (token.startsWith(QLatin1Char('(')) || token.startsWith(QLatin1Char('['))) {
            if (!result.isEmpty() && !result.endsWith(QLatin1Char(' '))) {
                result += QLatin1Char(' ');
            }
            const QChar open = token[0];
            const QChar close = token[token.size() - 1];
            const QString inner = token.mid(1, token.size() - 2).trimmed();
            result += QString("%1%2%3").arg(open).arg(toTitleCase(inner)).arg(close);
        } else {
            if (!result.isEmpty() && !result.endsWith(QLatin1Char(' ')) && !result.endsWith(QLatin1Char('-'))) {
                result += QLatin1Char(' ');
            }
            result += toTitleCase(token);
        }
    }

    result = result.trimmed();
    if (result.isEmpty()) {
        return fileName;
    }

    if (!ext.isEmpty()) {
        result += QLatin1Char('.') + ext;
    }
    return result;
}

} // namespace tagit


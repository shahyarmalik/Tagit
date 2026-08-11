#include "DuplicateEngine.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>

namespace tagit {

QVector<QPair<qint64, qint64>> DuplicateEngine::findCandidates(const QVector<Song> &songs) const
{
    QVector<QPair<qint64, qint64>> candidates;
    for (int i = 0; i < songs.size(); ++i) {
        for (int j = i + 1; j < songs.size(); ++j) {
            const Song &a = songs[i];
            const Song &b = songs[j];
            if (areStrongDuplicates(a, b) || metadataSimilar(a, b)) {
                candidates.append({a.id, b.id});
            }
        }
    }
    return candidates;
}

bool DuplicateEngine::areStrongDuplicates(const Song &a, const Song &b) const
{
    if (a.fileSize == 0 || b.fileSize == 0) {
        return false;
    }
    return a.fileSize == b.fileSize
        && a.metadata.durationMs == b.metadata.durationMs
        && a.metadata.bitrate == b.metadata.bitrate;
}

QString DuplicateEngine::quickFileHash(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);

    // Hash the first and last 64 KB for speed.
    QByteArray head = file.read(64 * 1024);
    hash.addData(head);

    if (file.size() > 128 * 1024) {
        file.seek(file.size() - 64 * 1024);
        QByteArray tail = file.read(64 * 1024);
        hash.addData(tail);
    }

    return QString::fromLatin1(hash.result().toHex());
}

bool DuplicateEngine::metadataSimilar(const Song &a, const Song &b) const
{
    // Normalized title/artist comparison.
    const QString titleA = a.metadata.title.trimmed().toLower();
    const QString titleB = b.metadata.title.trimmed().toLower();
    const QString artistA = a.metadata.artist.trimmed().toLower();
    const QString artistB = b.metadata.artist.trimmed().toLower();

    if (titleA.isEmpty() || titleB.isEmpty() || titleA != titleB) {
        return false;
    }
    if (artistA.isEmpty() || artistB.isEmpty() || artistA != artistB) {
        return false;
    }

    // Same title+artist but different duration: could be a live/remix variant.
    // Require duration within 3 seconds OR same duration.
    const qint64 delta = qAbs(a.metadata.durationMs - b.metadata.durationMs);
    return delta <= 3000;
}

} // namespace tagit


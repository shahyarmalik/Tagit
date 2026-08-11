#include "BackupManager.h"
#include "Logger.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QCollator>
#include <algorithm>

namespace tagit {

BackupManager::BackupManager(const QString &backupRoot, QObject *parent)
    : QObject(parent)
    , m_backupRoot(backupRoot)
{
    QDir().mkpath(m_backupRoot);
}

QString BackupManager::createRestorePoint(const QString &filePath)
{
    const QFileInfo info(filePath);
    if (!info.exists()) {
        Logger::warn("Cannot create restore point for missing file: " + filePath);
        return {};
    }

    const QString backupId = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
    const QString target   = backupFilePath(filePath, backupId);

    // Ensure the backup directory exists.
    QDir().mkpath(QFileInfo(target).absolutePath());

    if (!QFile::copy(filePath, target)) {
        Logger::error("Backup copy failed: " + filePath + " -> " + target);
        return {};
    }

    QStringList ids = m_backupIndex.value(filePath);
    ids.prepend(backupId);
    m_backupIndex[filePath] = ids;

    Logger::info("Restore point created: " + target);
    return backupId;
}

bool BackupManager::restore(const QString &filePath)
{
    const QStringList backups = listBackups(filePath);
    if (backups.isEmpty()) {
        return false;
    }
    return restoreBackup(filePath, backups.first());
}

bool BackupManager::restoreBackup(const QString &filePath, const QString &backupId)
{
    const QString backup = backupFilePath(filePath, backupId);
    if (!QFile::exists(backup)) {
        Logger::error("Backup file not found: " + backup);
        return false;
    }

    // --- Safe restore: copy to a temp path first, then atomically rename ---
    // This guarantees the original is never removed unless we have a good copy.
    const QString tempPath = filePath + ".restore_tmp";

    // Remove any stale temp file from a previous failed restore.
    if (QFile::exists(tempPath)) {
        QFile::remove(tempPath);
    }

    if (!QFile::copy(backup, tempPath)) {
        Logger::error("Restore copy failed: " + backup + " -> " + tempPath);
        return false;
    }

    // Remove the current file only after the copy succeeded.
    if (QFile::exists(filePath) && !QFile::remove(filePath)) {
        Logger::error("Could not remove original before restore: " + filePath);
        QFile::remove(tempPath); // clean up the temp copy
        return false;
    }

    if (!QFile::rename(tempPath, filePath)) {
        Logger::error("Restore rename failed: " + tempPath + " -> " + filePath);
        // The temp copy is still intact — give the user a chance to recover it manually.
        Logger::warn("Temp restore copy left at: " + tempPath);
        return false;
    }

    Logger::info("Restored: " + filePath + " from backup " + backupId);
    return true;
}

QStringList BackupManager::listBackups(const QString &filePath) const
{
    QStringList ids = m_backupIndex.value(filePath);

    // Also discover backups on disk in case the in-memory index is cold.
    const QFileInfo info(filePath);
    const QString   base = info.completeBaseName();
    const QDir      backupDir(m_backupRoot);
    const QStringList entries = backupDir.entryList(
        QStringList() << (base + ".*.bak"), QDir::Files, QDir::Time);

    for (const QString &entry : entries) {
        // entry looks like: "basename.20260801_123456_000.mp3.bak"
        // extract the id portion between the first dot and the last two extensions
        const QString id = entry.mid(base.size() + 1,
                                     entry.size() - base.size() - 5 /* ".bak" + leading "." */);
        if (!ids.contains(id)) {
            ids << id;
        }
    }

    QCollator collator;
    collator.setNumericMode(true);
    std::sort(ids.begin(), ids.end(), [&collator](const QString &a, const QString &b) {
        return collator(a, b) > 0; // newest first (lexicographic desc on timestamp strings)
    });
    return ids;
}

int BackupManager::cleanupOldBackups(const QString &filePath, int maxCount)
{
    const QStringList backups = listBackups(filePath);
    if (backups.size() <= maxCount) {
        return 0;
    }

    int removed = 0;
    for (int i = maxCount; i < backups.size(); ++i) {
        const QString path = backupFilePath(filePath, backups[i]);
        if (QFile::remove(path)) {
            ++removed;
        }
    }

    // Trim the in-memory index too.
    QStringList ids = m_backupIndex.value(filePath);
    while (ids.size() > maxCount) {
        ids.removeLast();
    }
    m_backupIndex[filePath] = ids;

    return removed;
}

QString BackupManager::backupFilePath(const QString &filePath,
                                      const QString &backupId) const
{
    const QFileInfo info(filePath);
    const QString   base = info.completeBaseName();
    const QString   ext  = info.suffix();
    // e.g. <backupRoot>/basename.20260801_123456_000.mp3.bak
    QString path = m_backupRoot + "/" + base + "." + backupId;
    if (!ext.isEmpty()) {
        path += "." + ext;
    }
    return path + ".bak";
}

} // namespace tagit

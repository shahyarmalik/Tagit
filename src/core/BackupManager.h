#ifndef TAGIT_BACKUP_MANAGER_H
#define TAGIT_BACKUP_MANAGER_H

#include <QObject>
#include <QDateTime>
#include <QMap>
#include <QStringList>

namespace tagit {

/**
 * @brief Every modification creates a restore point.
 *
 * Supports undo, restore, backup history and cleanup policies.
 * Backups are stored as copies of the original file plus a sidecar
 * metadata record in the app data directory.
 */
class BackupManager : public QObject {
    Q_OBJECT
public:
    explicit BackupManager(const QString &backupRoot, QObject *parent = nullptr);

    /// Create a restore point for @p filePath. Returns the backup id.
    QString createRestorePoint(const QString &filePath);

    /// Restore the most recent backup for @p filePath.
    bool restore(const QString &filePath);

    /// Restore a specific backup by id.
    bool restoreBackup(const QString &filePath, const QString &backupId);

    /// List available backups for @p filePath (newest first).
    QStringList listBackups(const QString &filePath) const;

    /// Remove backups for @p filePath older than @p maxCount.
    int cleanupOldBackups(const QString &filePath, int maxCount);

private:
    QString backupFilePath(const QString &filePath, const QString &backupId) const;

    QString m_backupRoot;
    QMap<QString, QStringList> m_backupIndex; // filePath -> list of backup ids
};

} // namespace tagit

#endif // TAGIT_BACKUP_MANAGER_H


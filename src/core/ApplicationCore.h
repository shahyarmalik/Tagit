#ifndef TAGIT_APPLICATION_CORE_H
#define TAGIT_APPLICATION_CORE_H

#include <QObject>
#include <memory>

namespace tagit {

class SettingsManager;
class DatabaseService;
class LibraryManager;
class ILibraryManager;
class MetadataEngine;
class BackupManager;
class NetworkService;
class ArtworkManager;
class TagService;

/**
 * @brief Composition root for the application core.
 *
 * Owns the services and managers that implement the metadata decision engine,
 * library management, backups, duplicates, organization and search.
 */
class ApplicationCore : public QObject {
    Q_OBJECT
public:
    explicit ApplicationCore(const QString &appDataPath, QObject *parent = nullptr);
    ~ApplicationCore() override;

    SettingsManager *settings() const;
    ILibraryManager *library() const;
    MetadataEngine  *metadataEngine() const;
    BackupManager   *backups() const;
    NetworkService  *network() const;
    ArtworkManager  *artwork() const;
    DatabaseService *database() const;
    TagService      *tagService() const;

    /// Open the SQLite database at the default app data path.
    bool openDatabase(const QString &dbPath);

private:
    std::unique_ptr<SettingsManager>  m_settings;
    std::unique_ptr<DatabaseService>  m_database;
    std::unique_ptr<LibraryManager>   m_library;
    std::unique_ptr<MetadataEngine>   m_metadataEngine;
    std::unique_ptr<BackupManager>    m_backupManager;
    std::unique_ptr<NetworkService>   m_network;
    std::unique_ptr<ArtworkManager>   m_artwork;
    std::unique_ptr<TagService>       m_tagService;
};

} // namespace tagit

#endif // TAGIT_APPLICATION_CORE_H


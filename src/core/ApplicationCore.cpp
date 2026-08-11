#include "ApplicationCore.h"

#include "SettingsManager.h"
#include "LibraryManager.h"
#include "MetadataEngine.h"
#include "BackupManager.h"
#include "../platform/DatabaseService.h"
#include "../platform/NetworkService.h"
#include "../platform/TagService.h"
#include "ArtworkManager.h"
#include "Logger.h"

namespace tagit {

ApplicationCore::ApplicationCore(const QString &appDataPath, QObject *parent)
    : QObject(parent)
    , m_settings(std::make_unique<SettingsManager>(appDataPath))
    , m_database(std::make_unique<DatabaseService>())
    , m_library(std::make_unique<LibraryManager>(m_settings.get(),
                                                  m_database.get(), this))
    , m_metadataEngine(std::make_unique<MetadataEngine>(m_settings.get(), this))
    , m_backupManager(std::make_unique<BackupManager>(appDataPath + "/backups", this))
    , m_network(std::make_unique<NetworkService>(this))
    , m_artwork(std::make_unique<ArtworkManager>(appDataPath + "/artwork", this))
    , m_tagService(std::make_unique<TagService>())
{
    // Wire the online provider into the metadata engine.
    m_metadataEngine->setNetworkService(m_network.get());

    // Enable online lookup — it is now fully implemented.
    // The first time the app runs this sets the stored preference to true.
    // Users can still turn it off in Settings.
    if (m_settings->firstRun()) {
        m_settings->setAllowOnlineLookup(true);
    }
    m_network->setEnabled(m_settings->allowOnlineLookup());

    // Re-apply the network enabled flag whenever the user changes the setting.
    connect(m_settings.get(), &SettingsManager::settingChanged,
            this, [this](const QString &key, const QVariant &value) {
                if (key == QStringLiteral("metadata/allowOnlineLookup")) {
                    m_network->setEnabled(value.toBool());
                    Logger::info(QStringLiteral("Online lookup %1")
                                     .arg(value.toBool() ? "enabled" : "disabled"));
                }
            });

    // Give the library manager access to the metadata engine so it can
    // auto-enrich tracks immediately after each scan.
    m_library->setMetadataEngine(m_metadataEngine.get());
}

ApplicationCore::~ApplicationCore() = default;

SettingsManager *ApplicationCore::settings()     const { return m_settings.get(); }
ILibraryManager *ApplicationCore::library()      const { return m_library.get(); }
MetadataEngine  *ApplicationCore::metadataEngine() const { return m_metadataEngine.get(); }
BackupManager   *ApplicationCore::backups()      const { return m_backupManager.get(); }
NetworkService  *ApplicationCore::network()      const { return m_network.get(); }
ArtworkManager  *ApplicationCore::artwork()      const { return m_artwork.get(); }
DatabaseService *ApplicationCore::database()     const { return m_database.get(); }
TagService      *ApplicationCore::tagService()    const { return m_tagService.get(); }

bool ApplicationCore::openDatabase(const QString &dbPath)
{
    return m_database->open(dbPath);
}

} // namespace tagit

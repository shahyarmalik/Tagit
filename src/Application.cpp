#include "Application.h"
#include "core/Logger.h"
#include "core/ApplicationCore.h"
#include "core/SettingsManager.h"
#include "platform/IPlatformServices.h"
#include "platform/linux/LinuxPlatformServices.h"
#include "ui/MainWindow.h"
#include "model/Song.h"
#include "model/AudioMetadata.h"

#include <QMetaType>
#include <QStyleFactory>

namespace tagit {

Application::Application(int &argc, char **argv)
    : m_app(argc, argv)
{
    m_app.setApplicationName("TagIt");
    m_app.setApplicationDisplayName("TagIt — Music Library Manager");
    m_app.setApplicationVersion(TAGIT_VERSION);
    m_app.setOrganizationName("TagIt");
    m_app.setOrganizationDomain("tagit.app");

    // Use Fusion style for a consistent cross-platform look
    m_app.setStyle(QStyleFactory::create("Fusion"));

    // Register custom types for cross-thread queued signal/slot delivery.
    qRegisterMetaType<tagit::Song>("tagit::Song");
    qRegisterMetaType<tagit::AudioMetadata>("tagit::AudioMetadata");
    qRegisterMetaType<QVector<tagit::Song>>("QVector<tagit::Song>");
    qRegisterMetaType<QStringList>("QStringList");   // for writeConsensusTags queued call

    initializeServices();
    createMainWindow();
}

Application::~Application()
{
    m_mainWindow.reset();
    m_core.reset();
    m_platform.reset();
    Logger::shutdown();
}

int Application::run()
{
    Logger::info("TagIt " TAGIT_VERSION " starting...");
    Logger::info("Platform: " + m_platform->platformName());

    m_mainWindow->show();

    // First run: welcome the user without blocking on a modal folder dialog.
    // The user opens a music folder explicitly via File > Open Music Folder.
    if (m_core->settings()->firstRun()) {
        Logger::info("First run detected - showing welcome message");
        m_mainWindow->showWelcomeMessage();
        m_core->settings()->setFirstRun(false);
    }

    Logger::info("Application event loop started");
    return m_app.exec();
}

void Application::initializeServices()
{
    // Platform services
    m_platform = std::make_shared<LinuxPlatformServices>();

    // Logger
    Logger::initialize(m_platform->appDataPath(), "trace");

    // Application core (settings, database, library, metadata, backups, ...)
    m_core = std::make_unique<ApplicationCore>(m_platform->appDataPath());
    m_core->openDatabase(m_platform->appDataPath() + "/library.db");

    Logger::info("All services initialized");
    Logger::debug("App data path: " + m_platform->appDataPath());
    Logger::debug("App cache path: " + m_platform->appCachePath());
    Logger::debug("Default music path: " + m_platform->defaultMusicPath());
}

void Application::createMainWindow()
{
    m_mainWindow = std::make_unique<MainWindow>(m_platform.get(), m_core->settings());
    m_mainWindow->setLibraryManager(m_core->library());
    m_mainWindow->setApplicationCore(m_core.get());
}

} // namespace tagit


#ifndef TAGIT_APPLICATION_H
#define TAGIT_APPLICATION_H

#include <QApplication>
#include <memory>

namespace tagit {

class IPlatformServices;
class ApplicationCore;
class MainWindow;

/**
 * @brief Application bootstrap and lifecycle manager.
 *
 * Initializes all core services (platform, settings, database, logging)
 * and creates the main window. Responsible for clean startup and shutdown.
 */
class Application {
public:
    Application(int &argc, char **argv);
    ~Application();

    /// Run the application (enters Qt event loop).
    int run();

private:
    void initializeServices();
    void createMainWindow();

    QApplication m_app;
    std::shared_ptr<IPlatformServices> m_platform;
    std::unique_ptr<ApplicationCore> m_core;
    std::unique_ptr<MainWindow> m_mainWindow;
};

} // namespace tagit

#endif // TAGIT_APPLICATION_H


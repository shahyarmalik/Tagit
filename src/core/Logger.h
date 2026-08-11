#ifndef TAGIT_LOGGER_H
#define TAGIT_LOGGER_H

#include <QString>
#include <QStringList>
#include <memory>

// Only forward-declare spdlog types when the library is actually present.
#ifdef TAGIT_HAS_SPDLOG
namespace spdlog {
class logger;
}
#endif

namespace tagit {

/**
 * @brief Application-wide logging wrapper.
 *
 * When spdlog is available (TAGIT_HAS_SPDLOG) it is used as the backend;
 * otherwise Qt's qDebug/qInfo/qWarning fallback is used so the application
 * always compiles and logs without spdlog installed.
 */
class Logger {
public:
    /// Initialize the global logger. Must be called once at startup.
    static void initialize(const QString &appDataPath, const QString &logLevel = "info");

    /// Shut down the logger and flush all pending writes.
    static void shutdown();

    // --- Logging methods ---
    static void trace(const QString &message);
    static void debug(const QString &message);
    static void info(const QString &message);
    static void warn(const QString &message);
    static void error(const QString &message);
    static void critical(const QString &message);

    /// Returns the last N log entries as a string list (for UI display).
    static QStringList recentEntries(int count = 100);

private:
    static void appendRecent(const QString &line);

#ifdef TAGIT_HAS_SPDLOG
    static std::shared_ptr<spdlog::logger> s_logger;
#endif
    static QStringList s_recentEntries;
    static constexpr int kMaxRecentEntries = 1000;
};

} // namespace tagit

#endif // TAGIT_LOGGER_H

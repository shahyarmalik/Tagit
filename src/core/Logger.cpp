#include "Logger.h"

#include <QDir>
#include <QMutex>
#include <QMutexLocker>
#include <QDebug>
#include <QDateTime>
#include <algorithm>

#ifdef TAGIT_HAS_SPDLOG
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/ringbuffer_sink.h>
#endif

namespace tagit {

#ifdef TAGIT_HAS_SPDLOG
std::shared_ptr<spdlog::logger> Logger::s_logger = nullptr;
#endif
QStringList Logger::s_recentEntries;
static QMutex s_mutex;

void Logger::initialize(const QString &appDataPath, const QString &logLevel)
{
    QMutexLocker lock(&s_mutex);

    QString logDir = appDataPath + "/logs";
    QDir().mkpath(logDir);

#ifdef TAGIT_HAS_SPDLOG
    if (s_logger) {
        return; // Already initialized
    }

    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto fileSink    = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        (logDir + "/tagit.log").toStdString(), /*truncate=*/true);
    auto ringbufferSink = std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(kMaxRecentEntries);

    spdlog::level::level_enum level = spdlog::level::info;
    if      (logLevel == "trace") level = spdlog::level::trace;
    else if (logLevel == "debug") level = spdlog::level::debug;
    else if (logLevel == "warn")  level = spdlog::level::warn;
    else if (logLevel == "error") level = spdlog::level::err;

    consoleSink->set_level(level);
    fileSink->set_level(spdlog::level::trace); // file always gets everything
    ringbufferSink->set_level(level);

    s_logger = std::make_shared<spdlog::logger>("tagit",
        spdlog::sinks_init_list{consoleSink, fileSink, ringbufferSink});
    s_logger->set_level(level);
    spdlog::register_logger(s_logger);
    s_logger->flush_on(spdlog::level::critical);
#else
    Q_UNUSED(logLevel)
#endif

    // Use appendRecent directly so the mutex in the public helpers is not re-entered.
    s_recentEntries.prepend("[INFO] Logger initialized. Log directory: " + logDir);
}

void Logger::shutdown()
{
    QMutexLocker lock(&s_mutex);
#ifdef TAGIT_HAS_SPDLOG
    if (s_logger) {
        s_logger->flush();
        spdlog::drop("tagit");
        s_logger.reset();
    }
#endif
    s_recentEntries.clear();
}

// ---------------------------------------------------------------------------
// Public logging methods
// ---------------------------------------------------------------------------

void Logger::trace(const QString &message)
{
#ifdef TAGIT_HAS_SPDLOG
    if (s_logger) s_logger->trace(message.toStdString());
#else
    qDebug("[TRACE] %s", qUtf8Printable(message));
#endif
    appendRecent("[TRACE] " + message);
}

void Logger::debug(const QString &message)
{
#ifdef TAGIT_HAS_SPDLOG
    if (s_logger) s_logger->debug(message.toStdString());
#else
    qDebug("[DEBUG] %s", qUtf8Printable(message));
#endif
    appendRecent("[DEBUG] " + message);
}

void Logger::info(const QString &message)
{
#ifdef TAGIT_HAS_SPDLOG
    if (s_logger) s_logger->info(message.toStdString());
#else
    qInfo("[INFO] %s", qUtf8Printable(message));
#endif
    appendRecent("[INFO] " + message);
}

void Logger::warn(const QString &message)
{
#ifdef TAGIT_HAS_SPDLOG
    if (s_logger) s_logger->warn(message.toStdString());
#else
    qWarning("[WARN] %s", qUtf8Printable(message));
#endif
    appendRecent("[WARN] " + message);
}

void Logger::error(const QString &message)
{
#ifdef TAGIT_HAS_SPDLOG
    if (s_logger) s_logger->error(message.toStdString());
#else
    qWarning("[ERROR] %s", qUtf8Printable(message));
#endif
    appendRecent("[ERROR] " + message);
}

void Logger::critical(const QString &message)
{
#ifdef TAGIT_HAS_SPDLOG
    if (s_logger) s_logger->critical(message.toStdString());
#else
    qCritical("[CRITICAL] %s", qUtf8Printable(message));
#endif
    appendRecent("[CRITICAL] " + message);
}

QStringList Logger::recentEntries(int count)
{
    QMutexLocker lock(&s_mutex);
    return s_recentEntries.mid(0, std::min(count, static_cast<int>(s_recentEntries.size())));
}

void Logger::appendRecent(const QString &line)
{
    QMutexLocker lock(&s_mutex);
    s_recentEntries.prepend(line);
    while (s_recentEntries.size() > kMaxRecentEntries) {
        s_recentEntries.removeLast();
    }
}

} // namespace tagit

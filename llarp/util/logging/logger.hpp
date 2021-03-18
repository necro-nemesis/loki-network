#pragma once

#include <memory>
#include <llarp/util/time.hpp>
#include "logstream.hpp"
#include "logger_internal.hpp"

namespace llarp
{
  enum class LogType
  {
    Unknown = 0,
    File,
    Json,
    Syslog,
  };
  LogType
  LogTypeFromString(const std::string&);

  struct LogContext
  {
    using IOFunc_t = std::function<void(void)>;

    LogContext();
    LogLevel curLevel = eLogInfo;
    LogLevel startupLevel = eLogInfo;
    LogLevel runtimeLevel = eLogInfo;
    ILogStream_ptr logStream;
    std::string nodeName = "lokinet";

    const llarp_time_t started;

    static LogContext&
    Instance();

    void
    DropToRuntimeLevel();

    void
    RevertRuntimeLevel();

    /// A blocking call that will not return until any existing log functions have flushed.
    /// Should only be called in rare circumstances, such as when the program is about to exit.
    void
    ImmediateFlush();

    /// Initialize the logging system.
    ///
    /// @param level is the new log level (below which log statements will be ignored)
    /// @param type is the type of logger to set up
    /// @param file is the file to log to (relevant for types File and Json)
    /// @param nickname is a tag to add to each log statement
    /// @param io is a callable that queues work that does io, async
    void
    Initialize(
        LogLevel level,
        LogType type,
        const std::string& file,
        const std::string& nickname,
        std::function<void(IOFunc_t)> io);
  };

  /// RAII type to turn logging off
  /// logging is suppressed as long as the silencer is in scope
  struct LogSilencer
  {
    LogSilencer();
    ~LogSilencer();
    explicit LogSilencer(LogContext& ctx);

   private:
    LogContext& parent;
    ILogStream_ptr stream;
  };

  void
  SetLogLevel(LogLevel lvl);

  LogLevel
  GetLogLevel();

  /** internal */
  template <typename... TArgs>
  inline static void
  _Log(LogLevel lvl, const char* fname, int lineno, TArgs&&... args) noexcept
  {
    auto& log = LogContext::Instance();
    if (log.curLevel > lvl || log.logStream == nullptr)
      return;
    std::stringstream ss;
    LogAppend(ss, std::forward<TArgs>(args)...);
    log.logStream->AppendLog(lvl, fname, lineno, log.nodeName, ss.str());
  }
}  // namespace llarp

#define LogTrace(...) _Log(llarp::eLogTrace, LOG_TAG, __LINE__, __VA_ARGS__)
#define LogDebug(...) _Log(llarp::eLogDebug, LOG_TAG, __LINE__, __VA_ARGS__)
#define LogInfo(...) _Log(llarp::eLogInfo, LOG_TAG, __LINE__, __VA_ARGS__)
#define LogWarn(...) _Log(llarp::eLogWarn, LOG_TAG, __LINE__, __VA_ARGS__)
#define LogError(...) _Log(llarp::eLogError, LOG_TAG, __LINE__, __VA_ARGS__)

#define LogTraceTag(tag, ...) _Log(llarp::eLogTrace, tag, __LINE__, __VA_ARGS__)
#define LogDebugTag(tag, ...) _Log(llarp::eLogDebug, tag, __LINE__, __VA_ARGS__)
#define LogInfoTag(tag, ...) _Log(llarp::eLogInfo, tag, __LINE__, __VA_ARGS__)
#define LogWarnTag(tag, ...) _Log(llarp::eLogWarn, tag, __LINE__, __VA_ARGS__)
#define LogErrorTag(tag, ...) _Log(llarp::eLogError, tag, __LINE__, __VA_ARGS__)

#define LogTraceExplicit(tag, line, ...) _Log(llarp::eLogTrace, tag, line, __VA_ARGS__)
#define LogDebugExplicit(tag, line, ...) _Log(llarp::eLogDebug, tag, line, __VA_ARGS__)
#define LogInfoExplicit(tag, line, ...) _Log(llarp::eLogInfo, tag, line __VA_ARGS__)
#define LogWarnExplicit(tag, line, ...) _Log(llarp::eLogWarn, tag, line, __VA_ARGS__)
#define LogErrorExplicit(tag, line, ...) _Log(llarp::eLogError, tag, line, __VA_ARGS__)

#ifndef LOG_TAG
#define LOG_TAG "default"
#endif

#pragma once

#include <cassert>

#define USE_LOG_COLOR

#define FMT_HEADER_ONLY
#include <fmt/core.h>

namespace bee
{
/// <summary>
/// Logger class
/// </summary>
class Log
{
public:
    /// <summary>
    /// Initialize the logger, setting the pattern and the level of logging.
    /// </summary>
    static void Initialize();

    /// <summary>
    /// Info level logging.
    /// </summary>
    /// <param name="fmt">Format string, using Python-like format string syntax.</param>
    ///	<param name="...args">List of positional arguments.</param>
    template <typename FormatString, typename... Args>
    static void Info(const FormatString& fmt, const Args&... args);

    /// <summary>
    /// Warning level logging. Engine can still run as intended.
    /// </summary>
    /// <param name="fmt">Format string, using Python-like format string syntax.</param>
    ///	<param name="...args">List of positional arguments.</param>
    template <typename FormatString, typename... Args>
    static void Warn(const FormatString& fmt, const Args&... args);

    /// <summary>
    /// Error level logging. Engine may still be able to run, but not as intended.
    /// </summary>
    /// <param name="fmt">Format string, using Python-like format string syntax.</param>
    /// <param name="...args">List of positional arguments.</param>
    template <typename FormatString, typename... Args>
    static void Error(const FormatString& fmt, const Args&... args);

    /// <summary>
    /// Critical level logging. Engine cannot run.
    /// </summary>
    /// <param name="fmt">Format string, using Python-like format string syntax.</param>
    ///	<param name="...args">List of positional arguments.</param>
    template <typename FormatString, typename... Args>
    static void Critical(const FormatString& fmt, const Args&... args);

private:
#ifdef USE_LOG_COLOR
    static constexpr auto magenta = "\033[35m";
    static constexpr auto green = "\033[32m";
    static constexpr auto red = "\033[31m";
    static constexpr auto reset = "\033[0m";
#else
    static constexpr auto magenta = "";
    static constexpr auto green = "";
    static constexpr auto red = "";
    static constexpr auto reset = "";
#endif
};

template <typename FormatString, typename... Args>
inline void Log::Info(const FormatString& fmt, const Args&... args)
{
    printf("[%sinfo%s] ", green, reset);
    fmt::print(fmt, args...);
    printf("\n");
}

template <typename FormatString, typename... Args>
inline void Log::Warn(const FormatString& fmt, const Args&... args)
{
    printf("[%swarn%s] ", magenta, reset);
    fmt::print(fmt, args...);
    printf("\n");
}

template <typename FormatString, typename... Args>
inline void Log::Error(const FormatString& fmt, const Args&... args)
{
    printf("[%serror%s] ", red, reset);
    fmt::print(fmt, args...);
    printf("\n");
}

template <typename FormatString, typename... Args>
inline void Log::Critical(const FormatString& fmt, const Args&... args)
{
    printf("[%serror%s] ", red, reset);
    fmt::print(fmt, args...);
    printf("\n");
    assert(false);
}

}  // namespace bee
/**
 * @file log.h
 * @brief Lightweight logging library with file output support
 * 
 * Features:
 * - Multiple log levels: Debug, Info, Warning, Error
 * - Console and file output
 * - Thread-safe logging
 * - Format string support using std::format
 * - Source location tracking (C++20/23)
 */

#pragma once

#include <string>
#include <string_view>
#include <format>
#include <source_location>
#include <fstream>
#include <mutex>
#include <chrono>
#include <print>
#include <cstdint>

namespace yu {

/// Log severity levels
enum class LogLevel : std::uint8_t {
    Debug   = 0,
    Info    = 1,
    Warning = 2,
    Error   = 3,
    None    = 4  // Disable all logging
};

/// Convert LogLevel to string representation
[[nodiscard]] constexpr std::string_view LogLevelToString(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::Debug:   return "DEBUG";
        case LogLevel::Info:    return "INFO";
        case LogLevel::Warning: return "WARN";
        case LogLevel::Error:   return "ERROR";
        default:                return "UNKNOWN";
    }
}

/// Logger configuration and state
class Logger {
public:
    /// Get singleton instance
    [[nodiscard]] static Logger& Instance() noexcept;

    /// Set minimum log level (messages below this level are ignored)
    void SetMinLevel(LogLevel level) noexcept { m_minLevel = level; }
    
    /// Get current minimum log level
    [[nodiscard]] LogLevel GetMinLevel() const noexcept { return m_minLevel; }

    /// Enable/disable console output
    void SetConsoleOutput(bool enabled) noexcept { m_consoleOutput = enabled; }
    
    /// Check if console output is enabled
    [[nodiscard]] bool IsConsoleOutput() const noexcept { return m_consoleOutput; }

    /// Set log file path (empty string disables file logging)
    /// @param filepath Path to the log file
    /// @return true if file was opened successfully
    bool SetLogFile(std::string_view filepath);
    
    /// Close the current log file
    void CloseLogFile();
    
    /// Check if file logging is active
    [[nodiscard]] bool IsFileLogging() const noexcept { return m_fileStream.is_open(); }

    /// Core logging function
    void Log(LogLevel level, std::string_view message, 
             const std::source_location& loc = std::source_location::current());

    /// Formatted logging with variadic arguments
    template<typename... Args>
    void LogFmt(LogLevel level, std::format_string<Args...> fmt, Args&&... args,
                const std::source_location& loc = std::source_location::current()) {
        if (level < m_minLevel) return;
        Log(level, std::format(fmt, std::forward<Args>(args)...), loc);
    }

    // Prevent copying
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    Logger() = default;
    ~Logger();

    [[nodiscard]] std::string FormatTimestamp() const;
    [[nodiscard]] std::string FormatLogEntry(LogLevel level, std::string_view message,
                                              const std::source_location& loc) const;

    LogLevel      m_minLevel{LogLevel::Debug};
    bool          m_consoleOutput{true};
    std::ofstream m_fileStream;
    std::mutex    m_mutex;
};

// ============================================================================
// Convenience logging functions
// ============================================================================

/// Log a debug message
inline void LogDebug(std::string_view message,
                     const std::source_location& loc = std::source_location::current()) {
    Logger::Instance().Log(LogLevel::Debug, message, loc);
}

/// Log an info message
inline void LogInfo(std::string_view message,
                    const std::source_location& loc = std::source_location::current()) {
    Logger::Instance().Log(LogLevel::Info, message, loc);
}

/// Log a warning message
inline void LogWarning(std::string_view message,
                       const std::source_location& loc = std::source_location::current()) {
    Logger::Instance().Log(LogLevel::Warning, message, loc);
}

/// Log an error message
inline void LogError(std::string_view message,
                     const std::source_location& loc = std::source_location::current()) {
    Logger::Instance().Log(LogLevel::Error, message, loc);
}

// ============================================================================
// Formatted logging macros (captures source location correctly)
// ============================================================================

/// Log formatted debug message
#define YU_LOG_DEBUG(fmt, ...) \
    ::yu::Logger::Instance().Log(::yu::LogLevel::Debug, \
        std::format(fmt __VA_OPT__(,) __VA_ARGS__), std::source_location::current())

/// Log formatted info message
#define YU_LOG_INFO(fmt, ...) \
    ::yu::Logger::Instance().Log(::yu::LogLevel::Info, \
        std::format(fmt __VA_OPT__(,) __VA_ARGS__), std::source_location::current())

/// Log formatted warning message
#define YU_LOG_WARN(fmt, ...) \
    ::yu::Logger::Instance().Log(::yu::LogLevel::Warning, \
        std::format(fmt __VA_OPT__(,) __VA_ARGS__), std::source_location::current())

/// Log formatted error message
#define YU_LOG_ERROR(fmt, ...) \
    ::yu::Logger::Instance().Log(::yu::LogLevel::Error, \
        std::format(fmt __VA_OPT__(,) __VA_ARGS__), std::source_location::current())

// ============================================================================
// Global configuration helpers
// ============================================================================

/// Set the log file path
inline bool SetLogFile(std::string_view filepath) {
    return Logger::Instance().SetLogFile(filepath);
}

/// Set minimum log level
inline void SetLogLevel(LogLevel level) noexcept {
    Logger::Instance().SetMinLevel(level);
}

} // namespace yu

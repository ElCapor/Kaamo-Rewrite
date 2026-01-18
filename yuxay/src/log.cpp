/**
 * @file log.cpp
 * @brief Implementation of the logging library
 */

#include "yu/log.h"
#include <iostream>
#include <filesystem>
#include <cstdlib>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

namespace yu {

Logger& Logger::Instance() noexcept {
    static Logger instance;
    return instance;
}

Logger::~Logger() {
    CloseLogFile();
}

bool Logger::SetLogFile(std::string_view filepath) {
    std::lock_guard lock(m_mutex);
    
    // Close existing file if open
    if (m_fileStream.is_open()) {
        m_fileStream.close();
    }
    
    if (filepath.empty()) {
        return true; // Empty path means disable file logging
    }
    
    // Create parent directories if they don't exist
    std::filesystem::path path(filepath);
    if (path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            std::cerr << "[YU::LOG] Failed to create log directory: " << ec.message() << '\n';
            return false;
        }
    }
    
    m_fileStream.open(path, std::ios::out | std::ios::app);
    if (!m_fileStream.is_open()) {
        std::cerr << "[YU::LOG] Failed to open log file: " << filepath << '\n';
        return false;
    }
    
    return true;
}

void Logger::CloseLogFile() {
    std::lock_guard lock(m_mutex);
    if (m_fileStream.is_open()) {
        m_fileStream.flush();
        m_fileStream.close();
    }
}

std::string Logger::FormatTimestamp() const {
    using namespace std::chrono;
    
    auto now = system_clock::now();
    auto time = system_clock::to_time_t(now);
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &time);
#else
    localtime_r(&time, &tm_buf);
#endif
    
    return std::format("{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}.{:03d}",
                       tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
                       tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
                       static_cast<int>(ms.count()));
}

std::string Logger::FormatLogEntry(LogLevel level, std::string_view message,
                                    const std::source_location& loc) const {
    // Extract just the filename from the full path
    std::string_view filename = loc.file_name();
    if (auto pos = filename.find_last_of("/\\"); pos != std::string_view::npos) {
        filename = filename.substr(pos + 1);
    }
    
    return std::format("[{}] [{}] [{}:{}] {}",
                       FormatTimestamp(),
                       LogLevelToString(level),
                       filename,
                       loc.line(),
                       message);
}

void Logger::Log(LogLevel level, std::string_view message, const std::source_location& loc) {
    if (level < m_minLevel) return;
    
    std::string entry = FormatLogEntry(level, message, loc);
    
    std::lock_guard lock(m_mutex);
    
    // Console output with optional colors (ANSI escape codes)
    if (m_consoleOutput) {
        if (m_colorOutput) {
            std::string_view colorCode;
            switch (level) {
                case LogLevel::Debug:   colorCode = "\033[36m"; break;  // Cyan
                case LogLevel::Info:    colorCode = "\033[32m"; break;  // Green
                case LogLevel::Warning: colorCode = "\033[33m"; break;  // Yellow
                case LogLevel::Error:   colorCode = "\033[31m"; break;  // Red
                default:                colorCode = "\033[0m";  break;  // Reset
            }
            std::cout << colorCode << entry << "\033[0m\n";
        } else {
            std::cout << entry << '\n';
        }
    }
    
    // File output
    if (m_fileStream.is_open()) {
        m_fileStream << entry << '\n';
        m_fileStream.flush(); // Ensure immediate write for debugging
    }
}

bool Logger::DetectColorSupport() noexcept {
#ifdef _WIN32
    // Check if stdout is a console (not redirected to file/pipe)
    if (!_isatty(_fileno(stdout))) {
        return false;
    }
    
    // Try to enable virtual terminal processing on Windows 10+
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) {
        return false;
    }
    
    // ENABLE_VIRTUAL_TERMINAL_PROCESSING = 0x0004
    constexpr DWORD ENABLE_VT_PROCESSING = 0x0004;
    if (dwMode & ENABLE_VT_PROCESSING) {
        return true; // Already enabled
    }
    
    // Try to enable it
    if (SetConsoleMode(hOut, dwMode | ENABLE_VT_PROCESSING)) {
        return true;
    }
    
    // Windows older than 10 without VT support
    return false;
#else
    // Unix-like: check if stdout is a TTY and TERM is set
    if (!isatty(fileno(stdout))) {
        return false;
    }
    
    const char* term = std::getenv("TERM");
    if (!term || std::string_view(term) == "dumb") {
        return false;
    }
    
    // Check for NO_COLOR environment variable (https://no-color.org/)
    if (std::getenv("NO_COLOR")) {
        return false;
    }
    
    return true;
#endif
}

} // namespace yu

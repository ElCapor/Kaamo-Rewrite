/**
 * @file io.h
 * @brief File I/O library with base path support
 * 
 * Features:
 * - Read/write bytes and strings to files
 * - Configurable base path for relative file operations
 * - Memory-mapped file support (optional)
 * - Cross-platform path handling
 * - Result types for error handling
 */

#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <span>
#include <filesystem>
#include <expected>
#include <fstream>
#include <cstdint>
#include <optional>

namespace yu {
namespace io {

// ============================================================================
// Error Types
// ============================================================================

/// I/O error codes
enum class IOError {
    None = 0,
    FileNotFound,
    AccessDenied,
    DirectoryNotFound,
    DiskFull,
    InvalidPath,
    ReadError,
    WriteError,
    Unknown
};

/// Convert IOError to human-readable string
[[nodiscard]] constexpr std::string_view IOErrorToString(IOError error) noexcept {
    switch (error) {
        case IOError::None:              return "No error";
        case IOError::FileNotFound:      return "File not found";
        case IOError::AccessDenied:      return "Access denied";
        case IOError::DirectoryNotFound: return "Directory not found";
        case IOError::DiskFull:          return "Disk full";
        case IOError::InvalidPath:       return "Invalid path";
        case IOError::ReadError:         return "Read error";
        case IOError::WriteError:        return "Write error";
        default:                         return "Unknown error";
    }
}

// ============================================================================
// Result Types (using C++23 std::expected)
// ============================================================================

/// Result type for byte operations
template<typename T>
using Result = std::expected<T, IOError>;

/// Byte buffer type
using ByteBuffer = std::vector<std::uint8_t>;

// ============================================================================
// Path Management
// ============================================================================

/// File system manager with base path support
class FileSystem {
public:
    /// Get singleton instance
    [[nodiscard]] static FileSystem& Instance() noexcept;

    /// Set the base path for relative file operations
    /// @param basePath The base directory path
    void SetBasePath(const std::filesystem::path& basePath);
    
    /// Get the current base path
    [[nodiscard]] const std::filesystem::path& GetBasePath() const noexcept { return m_basePath; }
    
    /// Resolve a path (prepends base path if the given path is relative)
    [[nodiscard]] std::filesystem::path ResolvePath(const std::filesystem::path& path) const;
    
    /// Get the directory of the current executable
    [[nodiscard]] static std::filesystem::path GetExecutablePath();
    
    /// Check if a file exists
    [[nodiscard]] bool Exists(const std::filesystem::path& path) const;
    
    /// Check if path is a directory
    [[nodiscard]] bool IsDirectory(const std::filesystem::path& path) const;
    
    /// Check if path is a regular file
    [[nodiscard]] bool IsFile(const std::filesystem::path& path) const;
    
    /// Get file size in bytes
    [[nodiscard]] Result<std::uintmax_t> GetFileSize(const std::filesystem::path& path) const;
    
    /// Create directories recursively
    [[nodiscard]] Result<bool> CreateDirectories(const std::filesystem::path& path) const;

    // Prevent copying
    FileSystem(const FileSystem&) = delete;
    FileSystem& operator=(const FileSystem&) = delete;

private:
    FileSystem() = default;
    std::filesystem::path m_basePath;
};

// ============================================================================
// File Reading Functions
// ============================================================================

/// Read entire file as bytes
/// @param path File path (relative to base path or absolute)
/// @return ByteBuffer containing file contents, or IOError on failure
[[nodiscard]] Result<ByteBuffer> ReadBytes(const std::filesystem::path& path);

/// Read entire file as string
/// @param path File path (relative to base path or absolute)
/// @return String containing file contents, or IOError on failure
[[nodiscard]] Result<std::string> ReadString(const std::filesystem::path& path);

/// Read file with size limit
/// @param path File path
/// @param maxBytes Maximum number of bytes to read
/// @return ByteBuffer with up to maxBytes, or IOError on failure
[[nodiscard]] Result<ByteBuffer> ReadBytesLimited(const std::filesystem::path& path, 
                                                   std::size_t maxBytes);

/// Read specific byte range from file
/// @param path File path
/// @param offset Starting offset in bytes
/// @param count Number of bytes to read
/// @return ByteBuffer containing the requested range, or IOError on failure
[[nodiscard]] Result<ByteBuffer> ReadBytesRange(const std::filesystem::path& path,
                                                 std::size_t offset, std::size_t count);

// ============================================================================
// File Writing Functions
// ============================================================================

/// Write mode for file operations
enum class WriteMode {
    Overwrite,  // Create or replace file
    Append,     // Append to existing file
    CreateNew   // Fail if file exists
};

/// Write bytes to file
/// @param path File path
/// @param data Span of bytes to write
/// @param mode Write mode
/// @return true on success, or IOError on failure
[[nodiscard]] Result<bool> WriteBytes(const std::filesystem::path& path,
                                       std::span<const std::uint8_t> data,
                                       WriteMode mode = WriteMode::Overwrite);

/// Write string to file
/// @param path File path
/// @param content String to write
/// @param mode Write mode
/// @return true on success, or IOError on failure
[[nodiscard]] Result<bool> WriteString(const std::filesystem::path& path,
                                        std::string_view content,
                                        WriteMode mode = WriteMode::Overwrite);

/// Write bytes from vector (convenience overload)
inline Result<bool> WriteBytes(const std::filesystem::path& path,
                               const ByteBuffer& data,
                               WriteMode mode = WriteMode::Overwrite) {
    return WriteBytes(path, std::span<const std::uint8_t>(data), mode);
}

// ============================================================================
// Convenience Global Functions
// ============================================================================

/// Set the global base path for file operations
inline void SetBasePath(const std::filesystem::path& basePath) {
    FileSystem::Instance().SetBasePath(basePath);
}

/// Get the executable's directory path
inline std::filesystem::path GetExecutablePath() {
    return FileSystem::GetExecutablePath();
}

/// Initialize file system with executable's directory as base path
inline void InitializeWithExecutablePath() {
    SetBasePath(GetExecutablePath());
}

} // namespace io
} // namespace yu

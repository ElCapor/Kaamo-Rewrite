/**
 * @file io.cpp
 * @brief Implementation of the file I/O library
 */

#include "yu/io.h"
#include <fstream>
#include <system_error>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <windows.h>
#else
    #include <unistd.h>
    #include <linux/limits.h>
#endif

namespace yu {
namespace io {

// ============================================================================
// FileSystem Implementation
// ============================================================================

FileSystem& FileSystem::Instance() noexcept {
    static FileSystem instance;
    return instance;
}

void FileSystem::SetBasePath(const std::filesystem::path& basePath) {
    if (basePath.empty()) {
        m_basePath.clear();
        return;
    }
    
    std::error_code ec;
    m_basePath = std::filesystem::absolute(basePath, ec);
    if (ec) {
        m_basePath = basePath;
    }
}

std::filesystem::path FileSystem::ResolvePath(const std::filesystem::path& path) const {
    if (path.empty()) {
        return m_basePath;
    }
    
    // If path is absolute, return as-is
    if (path.is_absolute()) {
        return path;
    }
    
    // If no base path set, use current directory
    if (m_basePath.empty()) {
        return std::filesystem::absolute(path);
    }
    
    // Combine base path with relative path
    return m_basePath / path;
}

std::filesystem::path FileSystem::GetExecutablePath() {
#ifdef _WIN32
    wchar_t buffer[MAX_PATH];
    DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length > 0 && length < MAX_PATH) {
        std::filesystem::path exePath(buffer);
        return exePath.parent_path();
    }
#else
    char buffer[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", buffer, PATH_MAX);
    if (count > 0) {
        buffer[count] = '\0';
        std::filesystem::path exePath(buffer);
        return exePath.parent_path();
    }
#endif
    
    // Fallback to current directory
    return std::filesystem::current_path();
}

bool FileSystem::Exists(const std::filesystem::path& path) const {
    std::error_code ec;
    return std::filesystem::exists(ResolvePath(path), ec);
}

bool FileSystem::IsDirectory(const std::filesystem::path& path) const {
    std::error_code ec;
    return std::filesystem::is_directory(ResolvePath(path), ec);
}

bool FileSystem::IsFile(const std::filesystem::path& path) const {
    std::error_code ec;
    return std::filesystem::is_regular_file(ResolvePath(path), ec);
}

Result<std::uintmax_t> FileSystem::GetFileSize(const std::filesystem::path& path) const {
    std::error_code ec;
    auto resolved = ResolvePath(path);
    
    if (!std::filesystem::exists(resolved, ec)) {
        return std::unexpected(IOError::FileNotFound);
    }
    
    auto size = std::filesystem::file_size(resolved, ec);
    if (ec) {
        return std::unexpected(IOError::ReadError);
    }
    
    return size;
}

Result<bool> FileSystem::CreateDirectories(const std::filesystem::path& path) const {
    std::error_code ec;
    auto resolved = ResolvePath(path);
    
    std::filesystem::create_directories(resolved, ec);
    if (ec) {
        if (ec == std::errc::permission_denied) {
            return std::unexpected(IOError::AccessDenied);
        }
        return std::unexpected(IOError::Unknown);
    }
    
    return true;
}

// ============================================================================
// File Reading Implementation
// ============================================================================

namespace {

IOError MapFileStreamError(const std::ifstream& stream) {
    if (stream.bad()) return IOError::ReadError;
    if (stream.fail()) return IOError::ReadError;
    return IOError::Unknown;
}

IOError MapOfstreamError(const std::ofstream& stream) {
    if (stream.bad()) return IOError::WriteError;
    if (stream.fail()) return IOError::WriteError;
    return IOError::Unknown;
}

} // anonymous namespace

Result<ByteBuffer> ReadBytes(const std::filesystem::path& path) {
    auto resolved = FileSystem::Instance().ResolvePath(path);
    
    std::error_code ec;
    if (!std::filesystem::exists(resolved, ec)) {
        return std::unexpected(IOError::FileNotFound);
    }
    
    std::ifstream file(resolved, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return std::unexpected(IOError::AccessDenied);
    }
    
    auto size = file.tellg();
    if (size < 0) {
        return std::unexpected(IOError::ReadError);
    }
    
    file.seekg(0, std::ios::beg);
    
    ByteBuffer buffer(static_cast<std::size_t>(size));
    if (size > 0) {
        file.read(reinterpret_cast<char*>(buffer.data()), size);
        if (!file) {
            return std::unexpected(MapFileStreamError(file));
        }
    }
    
    return buffer;
}

Result<std::string> ReadString(const std::filesystem::path& path) {
    auto bytesResult = ReadBytes(path);
    if (!bytesResult) {
        return std::unexpected(bytesResult.error());
    }
    
    const auto& bytes = *bytesResult;
    return std::string(bytes.begin(), bytes.end());
}

Result<ByteBuffer> ReadBytesLimited(const std::filesystem::path& path, std::size_t maxBytes) {
    auto resolved = FileSystem::Instance().ResolvePath(path);
    
    std::error_code ec;
    if (!std::filesystem::exists(resolved, ec)) {
        return std::unexpected(IOError::FileNotFound);
    }
    
    std::ifstream file(resolved, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return std::unexpected(IOError::AccessDenied);
    }
    
    auto size = file.tellg();
    if (size < 0) {
        return std::unexpected(IOError::ReadError);
    }
    
    file.seekg(0, std::ios::beg);
    
    std::size_t bytesToRead = std::min(static_cast<std::size_t>(size), maxBytes);
    ByteBuffer buffer(bytesToRead);
    
    if (bytesToRead > 0) {
        file.read(reinterpret_cast<char*>(buffer.data()), 
                  static_cast<std::streamsize>(bytesToRead));
        if (!file) {
            return std::unexpected(MapFileStreamError(file));
        }
    }
    
    return buffer;
}

Result<ByteBuffer> ReadBytesRange(const std::filesystem::path& path,
                                   std::size_t offset, std::size_t count) {
    auto resolved = FileSystem::Instance().ResolvePath(path);
    
    std::error_code ec;
    if (!std::filesystem::exists(resolved, ec)) {
        return std::unexpected(IOError::FileNotFound);
    }
    
    std::ifstream file(resolved, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return std::unexpected(IOError::AccessDenied);
    }
    
    auto fileSize = file.tellg();
    if (fileSize < 0 || static_cast<std::size_t>(fileSize) < offset) {
        return std::unexpected(IOError::ReadError);
    }
    
    file.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    
    std::size_t availableBytes = static_cast<std::size_t>(fileSize) - offset;
    std::size_t bytesToRead = std::min(count, availableBytes);
    
    ByteBuffer buffer(bytesToRead);
    if (bytesToRead > 0) {
        file.read(reinterpret_cast<char*>(buffer.data()), 
                  static_cast<std::streamsize>(bytesToRead));
        if (!file) {
            return std::unexpected(MapFileStreamError(file));
        }
    }
    
    return buffer;
}

// ============================================================================
// File Writing Implementation
// ============================================================================

Result<bool> WriteBytes(const std::filesystem::path& path,
                         std::span<const std::uint8_t> data,
                         WriteMode mode) {
    auto resolved = FileSystem::Instance().ResolvePath(path);
    
    // Check for CreateNew mode
    if (mode == WriteMode::CreateNew) {
        std::error_code ec;
        if (std::filesystem::exists(resolved, ec)) {
            return std::unexpected(IOError::AccessDenied);
        }
    }
    
    // Create parent directories if needed
    if (resolved.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(resolved.parent_path(), ec);
        // Ignore error if directory already exists
    }
    
    std::ios_base::openmode openMode = std::ios::binary;
    if (mode == WriteMode::Append) {
        openMode |= std::ios::app;
    } else {
        openMode |= std::ios::out | std::ios::trunc;
    }
    
    std::ofstream file(resolved, openMode);
    if (!file.is_open()) {
        return std::unexpected(IOError::AccessDenied);
    }
    
    if (!data.empty()) {
        file.write(reinterpret_cast<const char*>(data.data()), 
                   static_cast<std::streamsize>(data.size()));
        if (!file) {
            return std::unexpected(MapOfstreamError(file));
        }
    }
    
    file.flush();
    return true;
}

Result<bool> WriteString(const std::filesystem::path& path,
                          std::string_view content,
                          WriteMode mode) {
    auto bytes = std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(content.data()),
        content.size()
    );
    return WriteBytes(path, bytes, mode);
}

} // namespace io
} // namespace yu

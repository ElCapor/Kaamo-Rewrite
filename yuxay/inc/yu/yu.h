/**
 * @file yu.h
 * @brief Main header for the Yu library
 * 
 * Yu is a lightweight, modern C++23 utility library designed for game development.
 * It provides essential features without heavy dependencies.
 * 
 * Features:
 * - Logging with file output support
 * - File I/O with base path management
 * - Tagged memory allocation and tracking
 * - RAII helpers and utilities
 * 
 * @version 1.0.0
 * @author RayShip Development
 */

#pragma once

// Core headers
#include "yu/log.h"
#include "yu/io.h"
#include "yu/memory.h"
#include "yu/raii.h"

/// Yu library version information
namespace yu {

/// Library version constants
namespace version {
    constexpr int Major = 1;
    constexpr int Minor = 0;
    constexpr int Patch = 0;
    
    /// Version as string
    constexpr const char* String = "1.0.0";
    
    /// Version as integer (major * 10000 + minor * 100 + patch)
    constexpr int AsInt = Major * 10000 + Minor * 100 + Patch;
}

/// Initialize the Yu library with sensible defaults
/// Call this at application startup
inline void Initialize() {
    // Set base path to executable directory
    io::InitializeWithExecutablePath();
    
    // Log initialization
    LogInfo("Yu library initialized");
    YU_LOG_DEBUG("Version: {}", version::String);
}

/// Shutdown the Yu library
/// Call this before application exit for clean shutdown
inline void Shutdown() {
    // Print memory report if there are leaks
    auto leaks = mem::CheckLeaks();
    if (leaks > 0) {
        LogWarning("Memory leaks detected!");
        mem::PrintMemoryReport();
    }
    
    LogInfo("Yu library shutdown");
}

} // namespace yu

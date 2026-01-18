/**
 * @file memory.h
 * @brief Tagged memory allocation system with tracking and debugging
 * 
 * Features:
 * - Tagged allocations for categorizing memory usage
 * - Allocation tracking (size, location, timestamp)
 * - Memory statistics and leak detection
 * - Debug visualization of memory state
 * - Thread-safe operations
 * - Zero-overhead in release builds (optional)
 */

#pragma once

#include <string>
#include <string_view>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <source_location>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <atomic>
#include <chrono>
#include <functional>
#include <format>

namespace yu {
namespace mem {

// ============================================================================
// Configuration
// ============================================================================

/// Enable/disable memory tracking (can be disabled in release for zero overhead)
#ifndef YU_MEMORY_TRACKING_ENABLED
    #ifdef NDEBUG
        #define YU_MEMORY_TRACKING_ENABLED 0
    #else
        #define YU_MEMORY_TRACKING_ENABLED 1
    #endif
#endif

/// Maximum tag name length
constexpr std::size_t MaxTagNameLength = 64;

// ============================================================================
// Memory Tag System
// ============================================================================

/// Memory tag identifier (string-based for flexibility)
using TagId = std::uint32_t;

/// Predefined memory tags
namespace Tags {
    constexpr TagId General    = 0;
    constexpr TagId Graphics   = 1;
    constexpr TagId Audio      = 2;
    constexpr TagId Physics    = 3;
    constexpr TagId AI         = 4;
    constexpr TagId Network    = 5;
    constexpr TagId UI         = 6;
    constexpr TagId Gameplay   = 7;
    constexpr TagId Resource   = 8;
    constexpr TagId Temporary  = 9;
    constexpr TagId UserStart  = 100;  // User-defined tags start here
}

// ============================================================================
// Custom Allocator Support
// ============================================================================

/// Custom allocation function signature
/// @param size Number of bytes to allocate
/// @return Pointer to allocated memory, or nullptr on failure
using AllocFunc = void* (*)(std::size_t size);

/// Custom reallocation function signature
/// @param ptr Pointer to previously allocated memory (nullptr for new allocation)
/// @param size New size in bytes
/// @return Pointer to reallocated memory, or nullptr on failure
using ReallocFunc = void* (*)(void* ptr, std::size_t size);

/// Custom deallocation function signature
/// @param ptr Pointer to memory to free
using DeallocFunc = void (*)(void* ptr);

/// Set custom allocator functions for all memory operations
/// @param alloc Custom allocation function (nullptr to use default malloc)
/// @param realloc Custom reallocation function (nullptr to use default realloc)
/// @param dealloc Custom deallocation function (nullptr to use default free)
/// @note All three functions should be provided together for consistency.
///       If any is nullptr, the default implementation will be used for that operation.
/// @note This should be called before any allocations are made.
/// @note Thread-safe: uses atomic operations internally.
void SetAllocator(AllocFunc alloc, ReallocFunc realloc, DeallocFunc dealloc) noexcept;

/// Reset to default allocator (malloc/realloc/free)
void ResetAllocator() noexcept;

/// Check if a custom allocator is currently set
/// @return true if custom allocator is set, false if using defaults
[[nodiscard]] bool HasCustomAllocator() noexcept;

/// Reallocate memory with tag tracking
/// @param ptr Pointer to previously allocated memory (nullptr for new allocation)
/// @param size New size in bytes
/// @param tag Memory tag for categorization (used only for new allocations)
/// @param loc Source location (auto-captured)
/// @return Pointer to reallocated memory, or nullptr on failure
[[nodiscard]] void* Reallocate(void* ptr, std::size_t size, TagId tag = Tags::General,
                                const std::source_location& loc = std::source_location::current());


/// Memory allocation type
enum class AllocationType : std::uint8_t {
    Heap,       // Standard heap allocation (new/malloc)
    Stack,      // Stack-based (for tracking purposes only)
    Pool,       // Pool allocator
    Arena,      // Arena allocator
    Custom      // Custom allocator
};

/// Convert AllocationType to string
[[nodiscard]] constexpr std::string_view AllocationTypeToString(AllocationType type) noexcept {
    switch (type) {
        case AllocationType::Heap:   return "Heap";
        case AllocationType::Stack:  return "Stack";
        case AllocationType::Pool:   return "Pool";
        case AllocationType::Arena:  return "Arena";
        case AllocationType::Custom: return "Custom";
        default:                     return "Unknown";
    }
}

// ============================================================================
// Allocation Record
// ============================================================================

/// Information about a single allocation
struct AllocationRecord {
    void*           address{nullptr};
    std::size_t     size{0};
    std::size_t     alignment{0};
    TagId           tag{Tags::General};
    AllocationType  type{AllocationType::Heap};
    const char*     file{nullptr};
    std::uint32_t   line{0};
    std::chrono::steady_clock::time_point timestamp;
    
    [[nodiscard]] std::string ToString() const;
};

// ============================================================================
// Tag Statistics
// ============================================================================

/// Statistics for a memory tag
struct TagStats {
    std::string     name;
    TagId           id{0};
    std::size_t     currentBytes{0};
    std::size_t     peakBytes{0};
    std::size_t     totalAllocated{0};
    std::size_t     totalFreed{0};
    std::uint64_t   allocationCount{0};
    std::uint64_t   freeCount{0};
    
    [[nodiscard]] std::size_t GetActiveAllocations() const noexcept {
        return allocationCount > freeCount ? allocationCount - freeCount : 0;
    }
};

// ============================================================================
// Memory Tracker
// ============================================================================

/// Main memory tracking system
class MemoryTracker {
public:
    /// Get singleton instance
    [[nodiscard]] static MemoryTracker& Instance() noexcept;
    
    /// Register a new tag name
    /// @param id Tag identifier
    /// @param name Human-readable tag name
    void RegisterTag(TagId id, std::string_view name);
    
    /// Get tag name by ID
    [[nodiscard]] std::string_view GetTagName(TagId id) const;
    
    /// Record an allocation
    void RecordAllocation(void* ptr, std::size_t size, std::size_t alignment,
                          TagId tag, AllocationType type,
                          const std::source_location& loc = std::source_location::current());
    
    /// Record a deallocation
    void RecordDeallocation(void* ptr);
    
    /// Get statistics for a specific tag
    [[nodiscard]] TagStats GetTagStats(TagId tag) const;
    
    /// Get all tag statistics
    [[nodiscard]] std::vector<TagStats> GetAllTagStats() const;
    
    /// Get total memory currently allocated
    [[nodiscard]] std::size_t GetTotalAllocatedBytes() const noexcept;
    
    /// Get peak memory usage
    [[nodiscard]] std::size_t GetPeakAllocatedBytes() const noexcept;
    
    /// Get all active allocations
    [[nodiscard]] std::vector<AllocationRecord> GetActiveAllocations() const;
    
    /// Get active allocations for a specific tag
    [[nodiscard]] std::vector<AllocationRecord> GetActiveAllocations(TagId tag) const;
    
    /// Check for memory leaks (returns count of leaked allocations)
    [[nodiscard]] std::size_t CheckLeaks() const;
    
    /// Generate a detailed memory report
    [[nodiscard]] std::string GenerateReport() const;
    
    /// Print memory report to console
    void PrintReport() const;
    
    /// Reset all tracking data
    void Reset();
    
    /// Enable/disable tracking at runtime
    void SetEnabled(bool enabled) noexcept { m_enabled = enabled; }
    [[nodiscard]] bool IsEnabled() const noexcept { return m_enabled; }
    
    // Prevent copying
    MemoryTracker(const MemoryTracker&) = delete;
    MemoryTracker& operator=(const MemoryTracker&) = delete;

private:
    MemoryTracker();
    
    mutable std::mutex m_mutex;
    std::unordered_map<void*, AllocationRecord> m_allocations;
    std::unordered_map<TagId, TagStats> m_tagStats;
    std::unordered_map<TagId, std::string> m_tagNames;
    std::atomic<std::size_t> m_totalAllocated{0};
    std::atomic<std::size_t> m_peakAllocated{0};
    bool m_enabled{true};
};

// ============================================================================
// Tagged Allocation Functions
// ============================================================================

/// Allocate memory with tag tracking
/// @param size Number of bytes to allocate
/// @param tag Memory tag for categorization
/// @param loc Source location (auto-captured)
/// @return Pointer to allocated memory, or nullptr on failure
[[nodiscard]] void* Allocate(std::size_t size, TagId tag = Tags::General,
                              const std::source_location& loc = std::source_location::current());

/// Allocate aligned memory with tag tracking
/// @param size Number of bytes to allocate
/// @param alignment Required alignment
/// @param tag Memory tag for categorization
/// @param loc Source location (auto-captured)
/// @return Pointer to allocated memory, or nullptr on failure
[[nodiscard]] void* AllocateAligned(std::size_t size, std::size_t alignment, 
                                     TagId tag = Tags::General,
                                     const std::source_location& loc = std::source_location::current());

/// Free previously allocated memory
/// @param ptr Pointer to memory to free
void Free(void* ptr);

/// Free aligned memory
/// @param ptr Pointer to aligned memory to free
void FreeAligned(void* ptr);

// ============================================================================
// Typed Allocation Helpers
// ============================================================================

/// Create a new object with tag tracking
template<typename T, typename... Args>
[[nodiscard]] T* New(TagId tag, Args&&... args) {
    void* mem = Allocate(sizeof(T), tag);
    if (!mem) return nullptr;
    return new(mem) T(std::forward<Args>(args)...);
}

/// Create a new object with default tag
template<typename T, typename... Args>
[[nodiscard]] T* New(Args&&... args) {
    return New<T>(Tags::General, std::forward<Args>(args)...);
}

/// Delete an object created with New
template<typename T>
void Delete(T* ptr) {
    if (ptr) {
        ptr->~T();
        Free(ptr);
    }
}

/// Create an array of objects
template<typename T>
[[nodiscard]] T* NewArray(std::size_t count, TagId tag = Tags::General) {
    void* mem = Allocate(sizeof(T) * count, tag);
    if (!mem) return nullptr;
    
    T* arr = static_cast<T*>(mem);
    for (std::size_t i = 0; i < count; ++i) {
        new(&arr[i]) T();
    }
    return arr;
}

/// Copy an array of objects from source
template<typename T>
[[nodiscard]] T* NewArray(const T* src, std::size_t count, TagId tag = Tags::General) {
    void* mem = Allocate(sizeof(T) * count, tag);
    if (!mem) return nullptr;
    
    T* arr = static_cast<T*>(mem);
    for (std::size_t i = 0; i < count; ++i) {
        new(&arr[i]) T(src[i]);
    }
    return arr;
}

/// Delete an array created with NewArray
template<typename T>
void DeleteArray(T* ptr, std::size_t count) {
    if (ptr) {
        for (std::size_t i = count; i > 0; --i) {
            ptr[i - 1].~T();
        }
        Free(ptr);
    }
}

// ============================================================================
// Smart Pointer Support
// ============================================================================

/// Custom deleter for use with std::unique_ptr
template<typename T>
struct TaggedDeleter {
    void operator()(T* ptr) const {
        Delete(ptr);
    }
};

/// Tagged unique pointer type
template<typename T>
using UniquePtr = std::unique_ptr<T, TaggedDeleter<T>>;

/// Create a tagged unique pointer
template<typename T, typename... Args>
[[nodiscard]] UniquePtr<T> MakeUnique(TagId tag, Args&&... args) {
    return UniquePtr<T>(New<T>(tag, std::forward<Args>(args)...));
}

/// Create a tagged unique pointer with default tag
template<typename T, typename... Args>
[[nodiscard]] UniquePtr<T> MakeUnique(Args&&... args) {
    return MakeUnique<T>(Tags::General, std::forward<Args>(args)...);
}

// ============================================================================
// Scoped Tracking Helpers
// ============================================================================

/// RAII guard for tracking stack allocations (informational only)
class StackAllocationGuard {
public:
    StackAllocationGuard(void* ptr, std::size_t size, TagId tag = Tags::General,
                         const std::source_location& loc = std::source_location::current());
    ~StackAllocationGuard();
    
    // Non-copyable, non-movable
    StackAllocationGuard(const StackAllocationGuard&) = delete;
    StackAllocationGuard& operator=(const StackAllocationGuard&) = delete;
    StackAllocationGuard(StackAllocationGuard&&) = delete;
    StackAllocationGuard& operator=(StackAllocationGuard&&) = delete;

private:
    void* m_ptr;
};

/// Macro for tracking stack allocations
#define YU_TRACK_STACK(var, tag) \
    ::yu::mem::StackAllocationGuard _stack_guard_##var(&var, sizeof(var), tag)

// ============================================================================
// Convenience Functions
// ============================================================================

/// Register a memory tag
inline void RegisterTag(TagId id, std::string_view name) {
    MemoryTracker::Instance().RegisterTag(id, name);
}

/// Get the current total allocated bytes
inline std::size_t GetTotalAllocated() noexcept {
    return MemoryTracker::Instance().GetTotalAllocatedBytes();
}

/// Get the peak memory usage
inline std::size_t GetPeakAllocated() noexcept {
    return MemoryTracker::Instance().GetPeakAllocatedBytes();
}

/// Print memory report to console
inline void PrintMemoryReport() {
    MemoryTracker::Instance().PrintReport();
}

/// Check for memory leaks
inline std::size_t CheckLeaks() {
    return MemoryTracker::Instance().CheckLeaks();
}

} // namespace mem
} // namespace yu

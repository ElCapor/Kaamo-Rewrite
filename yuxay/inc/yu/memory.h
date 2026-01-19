/**
 * @file memory.h
 * @brief Tagged memory allocation system with tracking and debugging
 * 
 * This header provides two memory trackers for different use cases:
 * 
 * 1. LightweightTracker (memory_lightweight.h)
 *    - Lock-free, zero internal allocations
 *    - Use when hooking external memory functions (DLL injection)
 *    - Fixed-size preallocated buffers
 *    - Minimal per-allocation overhead (~16 bytes per record)
 * 
 * 2. DetailedTracker (memory_detailed.h)
 *    - Full allocation records with source location and timestamps
 *    - Uses std::unordered_map internally (allocates memory)
 *    - Use for development/debugging when NOT hooking memory
 * 
 * The default MemoryTracker class now uses LightweightTracker internally
 * for safe operation during DLL injection scenarios.
 * 
 * For detailed debugging, use DetailedTracker directly:
 *   #include <yu/memory_detailed.h>
 *   yu::mem::DetailedTracker::Instance().RecordAllocation(...);
 */

#pragma once

#include "memory_lightweight.h"
#include "memory_detailed.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <atomic>
#include <memory>
#include <source_location>
#include <string_view>

#ifdef _WIN32
#include <malloc.h>  // For _aligned_malloc/_aligned_free
#endif

namespace yu {
namespace mem {

// ============================================================================
// Configuration
// ============================================================================

/// Enable/disable memory tracking (can be disabled for zero overhead)
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
// Unified Tag System (compatible with both trackers)
// ============================================================================

/// Memory tag identifier
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
// Allocation Type
// ============================================================================

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
// Custom Allocator Support
// ============================================================================

/// Custom allocation function signature
using AllocFunc = void* (*)(std::size_t size);

/// Custom reallocation function signature  
using ReallocFunc = void* (*)(void* ptr, std::size_t size);

/// Custom deallocation function signature
using DeallocFunc = void (*)(void* ptr);

namespace detail {
    inline std::atomic<AllocFunc>   g_customAlloc{nullptr};
    inline std::atomic<ReallocFunc> g_customRealloc{nullptr};
    inline std::atomic<DeallocFunc> g_customDealloc{nullptr};
}

/// Set custom allocator functions for all memory operations
/// @param alloc Custom allocation function (nullptr to use default malloc)
/// @param realloc Custom reallocation function (nullptr to use default realloc)
/// @param dealloc Custom deallocation function (nullptr to use default free)
/// @note All three functions should be provided together for consistency.
/// @note This should be called before any allocations are made.
/// @note Thread-safe: uses atomic operations internally.
inline void SetAllocator(AllocFunc alloc, ReallocFunc realloc, DeallocFunc dealloc) noexcept {
    detail::g_customAlloc.store(alloc, std::memory_order_release);
    detail::g_customRealloc.store(realloc, std::memory_order_release);
    detail::g_customDealloc.store(dealloc, std::memory_order_release);
}

/// Reset to default allocator (malloc/realloc/free)
inline void ResetAllocator() noexcept {
    detail::g_customAlloc.store(nullptr, std::memory_order_release);
    detail::g_customRealloc.store(nullptr, std::memory_order_release);
    detail::g_customDealloc.store(nullptr, std::memory_order_release);
}

/// Check if a custom allocator is currently set
[[nodiscard]] inline bool HasCustomAllocator() noexcept {
    return detail::g_customAlloc.load(std::memory_order_acquire) != nullptr ||
           detail::g_customRealloc.load(std::memory_order_acquire) != nullptr ||
           detail::g_customDealloc.load(std::memory_order_acquire) != nullptr;
}

// ============================================================================
// MemoryTracker - Unified Interface (wraps LightweightTracker)
// ============================================================================

/// Main memory tracking system
/// 
/// This class wraps LightweightTracker for production use in DLL injection.
/// It maintains the same interface as the original MemoryTracker for
/// backward compatibility.
/// 
/// For detailed debugging with source locations and timestamps,
/// use DetailedTracker instead.
class MemoryTracker {
public:
    /// Get singleton instance
    [[nodiscard]] static MemoryTracker& Instance() noexcept {
        static MemoryTracker instance;
        return instance;
    }
    
    /// Register a new tag name
    void RegisterTag(TagId id, std::string_view name) {
        m_tracker.RegisterTag(static_cast<std::uint16_t>(id), name.data());
    }
    
    /// Get tag name by ID
    [[nodiscard]] const char* GetTagName(TagId id) const {
        return m_tracker.GetTagName(static_cast<std::uint16_t>(id));
    }
    
    /// Record an allocation
    void RecordAllocation(void* ptr, std::size_t size, std::size_t /*alignment*/,
                          TagId tag, AllocationType type,
                          const std::source_location& /*loc*/ = std::source_location::current()) {
#if YU_MEMORY_TRACKING_ENABLED
        m_tracker.RecordAllocation(ptr, static_cast<std::uint32_t>(size),
            static_cast<std::uint16_t>(tag),
            static_cast<LightweightTracker::AllocationType>(type));
#endif
    }
    
    /// Record a deallocation
    void RecordDeallocation(void* ptr) {
#if YU_MEMORY_TRACKING_ENABLED
        m_tracker.RecordDeallocation(ptr);
#endif
    }
    
    /// Get total memory currently allocated
    [[nodiscard]] std::size_t GetTotalAllocatedBytes() const noexcept {
        return m_tracker.GetTotalBytes();
    }
    
    /// Get peak memory usage
    [[nodiscard]] std::size_t GetPeakAllocatedBytes() const noexcept {
        return m_tracker.GetPeakBytes();
    }
    
    /// Check for memory leaks (returns count of active allocations)
    [[nodiscard]] std::size_t CheckLeaks() const {
        return m_tracker.GetActiveCount();
    }
    
    /// Print memory report to console
    void PrintReport() const {
        m_tracker.PrintReport();
    }
    
    /// Reset all tracking data
    void Reset() {
        m_tracker.Reset();
    }
    
    /// Enable/disable tracking at runtime
    void SetEnabled(bool enabled) noexcept { 
        m_tracker.SetEnabled(enabled); 
    }
    
    [[nodiscard]] bool IsEnabled() const noexcept { 
        return m_tracker.IsEnabled(); 
    }
    
    /// Check if tracker is shutting down (always false for lightweight)
    [[nodiscard]] static bool IsShuttingDown() noexcept { 
        return false;
    }
    
    /// Get access to the underlying lightweight tracker
    [[nodiscard]] LightweightTracker& GetLightweight() noexcept {
        return m_tracker;
    }
    
    // Prevent copying
    MemoryTracker(const MemoryTracker&) = delete;
    MemoryTracker& operator=(const MemoryTracker&) = delete;
    
private:
    MemoryTracker() = default;
    LightweightTracker& m_tracker = LightweightTracker::Instance();
};

// ============================================================================
// Tagged Allocation Functions
// ============================================================================

/// Allocate memory with tag tracking
[[nodiscard]] inline void* Allocate(std::size_t size, TagId tag = Tags::General,
                                     const std::source_location& loc = std::source_location::current()) {
    if (size == 0) return nullptr;
    
    void* ptr = nullptr;
    AllocFunc customAlloc = detail::g_customAlloc.load(std::memory_order_acquire);
    AllocationType type = customAlloc ? AllocationType::Custom : AllocationType::Heap;
    
    if (customAlloc) {
        ptr = customAlloc(size);
    } else {
        ptr = std::malloc(size);
    }
    
    if (ptr) {
#if YU_MEMORY_TRACKING_ENABLED
        MemoryTracker::Instance().RecordAllocation(ptr, size, alignof(std::max_align_t), tag, type, loc);
#endif
    }
    return ptr;
}

/// Allocate aligned memory with tag tracking
[[nodiscard]] inline void* AllocateAligned(std::size_t size, std::size_t alignment, 
                                            TagId tag = Tags::General,
                                            const std::source_location& loc = std::source_location::current()) {
    if (size == 0) return nullptr;
    
    void* ptr = nullptr;
    AllocFunc customAlloc = detail::g_customAlloc.load(std::memory_order_acquire);
    AllocationType type = customAlloc ? AllocationType::Custom : AllocationType::Heap;
    
    if (customAlloc) {
        // Custom allocator: allocate extra space for alignment and store original pointer
        std::size_t totalSize = size + alignment + sizeof(void*);
        void* raw = customAlloc(totalSize);
        if (raw) {
            std::uintptr_t rawAddr = reinterpret_cast<std::uintptr_t>(raw);
            std::uintptr_t aligned = (rawAddr + sizeof(void*) + alignment - 1) & ~(alignment - 1);
            ptr = reinterpret_cast<void*>(aligned);
            reinterpret_cast<void**>(ptr)[-1] = raw;
        }
    } else {
#ifdef _WIN32
        ptr = _aligned_malloc(size, alignment);
#else
        ptr = std::aligned_alloc(alignment, size);
#endif
    }
    
    if (ptr) {
#if YU_MEMORY_TRACKING_ENABLED
        MemoryTracker::Instance().RecordAllocation(ptr, size, alignment, tag, type, loc);
#endif
    }
    return ptr;
}

/// Free previously allocated memory
inline void Free(void* ptr) {
    if (!ptr) return;
    
#if YU_MEMORY_TRACKING_ENABLED
    MemoryTracker::Instance().RecordDeallocation(ptr);
#endif
    
    DeallocFunc customDealloc = detail::g_customDealloc.load(std::memory_order_acquire);
    if (customDealloc) {
        customDealloc(ptr);
    } else {
        std::free(ptr);
    }
}

/// Free aligned memory
inline void FreeAligned(void* ptr) {
    if (!ptr) return;
    
#if YU_MEMORY_TRACKING_ENABLED
    MemoryTracker::Instance().RecordDeallocation(ptr);
#endif
    
    DeallocFunc customDealloc = detail::g_customDealloc.load(std::memory_order_acquire);
    if (customDealloc) {
        void* raw = reinterpret_cast<void**>(ptr)[-1];
        customDealloc(raw);
    } else {
#ifdef _WIN32
        _aligned_free(ptr);
#else
        std::free(ptr);
#endif
    }
}

/// Reallocate memory with tag tracking
[[nodiscard]] inline void* Reallocate(void* ptr, std::size_t size, TagId tag = Tags::General,
                                       const std::source_location& loc = std::source_location::current()) {
    if (size == 0) {
        Free(ptr);
        return nullptr;
    }
    
    if (!ptr) {
        return Allocate(size, tag, loc);
    }
    
#if YU_MEMORY_TRACKING_ENABLED
    MemoryTracker::Instance().RecordDeallocation(ptr);
#endif
    
    void* newPtr = nullptr;
    ReallocFunc customRealloc = detail::g_customRealloc.load(std::memory_order_acquire);
    AllocationType type = customRealloc ? AllocationType::Custom : AllocationType::Heap;
    
    if (customRealloc) {
        newPtr = customRealloc(ptr, size);
    } else {
        newPtr = std::realloc(ptr, size);
    }
    
    if (newPtr) {
#if YU_MEMORY_TRACKING_ENABLED
        MemoryTracker::Instance().RecordAllocation(newPtr, size, alignof(std::max_align_t), tag, type, loc);
#endif
    }
    return newPtr;
}

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
                         const std::source_location& loc = std::source_location::current()) 
        : m_ptr(ptr) {
#if YU_MEMORY_TRACKING_ENABLED
        MemoryTracker::Instance().RecordAllocation(ptr, size, alignof(std::max_align_t),
                                                    tag, AllocationType::Stack, loc);
#endif
    }
    
    ~StackAllocationGuard() {
#if YU_MEMORY_TRACKING_ENABLED
        MemoryTracker::Instance().RecordDeallocation(m_ptr);
#endif
    }
    
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

/// Register a memory tag (works with both trackers)
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

/**
 * @file memory.cpp
 * @brief Implementation of the tagged memory allocation system
 */

#include "yu/memory.h"
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <new>

namespace yu {
namespace mem {

// ============================================================================
// Custom Allocator Storage
// ============================================================================

namespace {
    std::atomic<AllocFunc>   g_customAlloc{nullptr};
    std::atomic<ReallocFunc> g_customRealloc{nullptr};
    std::atomic<DeallocFunc> g_customDealloc{nullptr};
}

void SetAllocator(AllocFunc alloc, ReallocFunc realloc, DeallocFunc dealloc) noexcept {
    g_customAlloc.store(alloc, std::memory_order_release);
    g_customRealloc.store(realloc, std::memory_order_release);
    g_customDealloc.store(dealloc, std::memory_order_release);
}

void ResetAllocator() noexcept {
    g_customAlloc.store(nullptr, std::memory_order_release);
    g_customRealloc.store(nullptr, std::memory_order_release);
    g_customDealloc.store(nullptr, std::memory_order_release);
}

bool HasCustomAllocator() noexcept {
    return g_customAlloc.load(std::memory_order_acquire) != nullptr ||
           g_customRealloc.load(std::memory_order_acquire) != nullptr ||
           g_customDealloc.load(std::memory_order_acquire) != nullptr;
}

// ============================================================================
// AllocationRecord Implementation
// ============================================================================

std::string AllocationRecord::ToString() const {
    auto& tracker = MemoryTracker::Instance();
    return std::format("Address: {:p}, Size: {} bytes, Tag: {}, Type: {}, Location: {}:{}",
                       address, size,
                       tracker.GetTagName(tag),
                       AllocationTypeToString(type),
                       file ? file : "unknown",
                       line);
}

// ============================================================================
// MemoryTracker Implementation
// ============================================================================

MemoryTracker::MemoryTracker() {
    // Register default tags
    m_tagNames[Tags::General]   = "General";
    m_tagNames[Tags::Graphics]  = "Graphics";
    m_tagNames[Tags::Audio]     = "Audio";
    m_tagNames[Tags::Physics]   = "Physics";
    m_tagNames[Tags::AI]        = "AI";
    m_tagNames[Tags::Network]   = "Network";
    m_tagNames[Tags::UI]        = "UI";
    m_tagNames[Tags::Gameplay]  = "Gameplay";
    m_tagNames[Tags::Resource]  = "Resource";
    m_tagNames[Tags::Temporary] = "Temporary";
}

MemoryTracker& MemoryTracker::Instance() noexcept {
    static MemoryTracker instance;
    return instance;
}

MemoryTracker::~MemoryTracker() {
    // Mark as shutting down BEFORE destroying members
    // This prevents re-entry during destruction of internal containers
    s_shuttingDown.store(true, std::memory_order_release);
}

void MemoryTracker::RegisterTag(TagId id, std::string_view name) {
    std::lock_guard lock(m_mutex);
    m_tagNames[id] = std::string(name);
}

std::string_view MemoryTracker::GetTagName(TagId id) const {
    std::lock_guard lock(m_mutex);
    auto it = m_tagNames.find(id);
    if (it != m_tagNames.end()) {
        return it->second;
    }
    return "Unknown";
}

// Thread-local re-entry guard to prevent infinite recursion
// when internal containers (unordered_map) allocate memory
thread_local bool t_insideTracker = false;

struct ReentryGuard {
    bool& flag;
    bool wasInside;
    ReentryGuard(bool& f) : flag(f), wasInside(f) { flag = true; }
    ~ReentryGuard() { flag = wasInside; }
    bool shouldSkip() const { return wasInside; }
};

void MemoryTracker::RecordAllocation(void* ptr, std::size_t size, std::size_t alignment,
                                      TagId tag, AllocationType type,
                                      const std::source_location& loc) {
#if YU_MEMORY_TRACKING_ENABLED
    if (!m_enabled || !ptr) return;
    
    // Don't try to lock mutex if we're shutting down - it may be destroyed
    if (s_shuttingDown.load(std::memory_order_acquire)) return;
    
    // Prevent infinite recursion: internal containers may allocate memory
    ReentryGuard guard(t_insideTracker);
    if (guard.shouldSkip()) return;
    
    AllocationRecord record{
        .address = ptr,
        .size = size,
        .alignment = alignment,
        .tag = tag,
        .type = type,
        .file = loc.file_name(),
        .line = loc.line(),
        .timestamp = std::chrono::steady_clock::now()
    };
    
    {
        std::lock_guard lock(m_mutex);
        m_allocations[ptr] = record;
        
        // Update tag stats
        auto& stats = m_tagStats[tag];
        if (stats.name.empty()) {
            auto it = m_tagNames.find(tag);
            stats.name = (it != m_tagNames.end()) ? it->second : "Unknown";
            stats.id = tag;
        }
        stats.currentBytes += size;
        stats.totalAllocated += size;
        stats.allocationCount++;
        stats.peakBytes = std::max(stats.peakBytes, stats.currentBytes);
    }
    
    // Update global stats (atomic, no lock needed)
    std::size_t newTotal = m_totalAllocated.fetch_add(size) + size;
    std::size_t currentPeak = m_peakAllocated.load();
    while (newTotal > currentPeak && 
           !m_peakAllocated.compare_exchange_weak(currentPeak, newTotal)) {
        // Retry if peak was updated by another thread
    }
#endif
}

void MemoryTracker::RecordDeallocation(void* ptr) {
#if YU_MEMORY_TRACKING_ENABLED
    if (!m_enabled || !ptr) return;
    
    // Don't try to lock mutex if we're shutting down - it may be destroyed
    if (s_shuttingDown.load(std::memory_order_acquire)) return;
    
    // Prevent infinite recursion: internal containers may allocate memory
    ReentryGuard guard(t_insideTracker);
    if (guard.shouldSkip()) return;
    
    std::lock_guard lock(m_mutex);
    
    auto it = m_allocations.find(ptr);
    if (it != m_allocations.end()) {
        const auto& record = it->second;
        std::size_t size = record.size;
        TagId tag = record.tag;
        
        // Update tag stats
        auto statsIt = m_tagStats.find(tag);
        if (statsIt != m_tagStats.end()) {
            statsIt->second.currentBytes -= std::min(statsIt->second.currentBytes, size);
            statsIt->second.totalFreed += size;
            statsIt->second.freeCount++;
        }
        
        m_totalAllocated.fetch_sub(size);
        m_allocations.erase(it);
    }
#endif
}

TagStats MemoryTracker::GetTagStats(TagId tag) const {
    std::lock_guard lock(m_mutex);
    auto it = m_tagStats.find(tag);
    if (it != m_tagStats.end()) {
        return it->second;
    }
    return TagStats{};
}

std::vector<TagStats> MemoryTracker::GetAllTagStats() const {
    std::lock_guard lock(m_mutex);
    std::vector<TagStats> result;
    result.reserve(m_tagStats.size());
    for (const auto& [id, stats] : m_tagStats) {
        result.push_back(stats);
    }
    return result;
}

std::size_t MemoryTracker::GetTotalAllocatedBytes() const noexcept {
    return m_totalAllocated.load();
}

std::size_t MemoryTracker::GetPeakAllocatedBytes() const noexcept {
    return m_peakAllocated.load();
}

std::vector<AllocationRecord> MemoryTracker::GetActiveAllocations() const {
    std::lock_guard lock(m_mutex);
    std::vector<AllocationRecord> result;
    result.reserve(m_allocations.size());
    for (const auto& [ptr, record] : m_allocations) {
        result.push_back(record);
    }
    return result;
}

std::vector<AllocationRecord> MemoryTracker::GetActiveAllocations(TagId tag) const {
    std::lock_guard lock(m_mutex);
    std::vector<AllocationRecord> result;
    for (const auto& [ptr, record] : m_allocations) {
        if (record.tag == tag) {
            result.push_back(record);
        }
    }
    return result;
}

std::size_t MemoryTracker::CheckLeaks() const {
    std::lock_guard lock(m_mutex);
    return m_allocations.size();
}

std::string MemoryTracker::GenerateReport() const {
    std::lock_guard lock(m_mutex);
    
    std::string report;
    report.reserve(4096);
    
    report += "=== YU Memory Report ===\n\n";
    report += std::format("Total Allocated: {} bytes\n", m_totalAllocated.load());
    report += std::format("Peak Allocated:  {} bytes\n", m_peakAllocated.load());
    report += std::format("Active Allocations: {}\n\n", m_allocations.size());
    
    report += "--- Tag Statistics ---\n";
    for (const auto& [id, stats] : m_tagStats) {
        if (stats.allocationCount > 0) {
            report += std::format("[{}] Current: {} bytes, Peak: {} bytes, "
                                  "Allocs: {}, Frees: {}\n",
                                  stats.name, stats.currentBytes, stats.peakBytes,
                                  stats.allocationCount, stats.freeCount);
        }
    }
    
    if (!m_allocations.empty()) {
        report += "\n--- Active Allocations ---\n";
        for (const auto& [ptr, record] : m_allocations) {
            report += record.ToString() + "\n";
        }
    }
    
    return report;
}

void MemoryTracker::PrintReport() const {
    std::cout << GenerateReport() << std::flush;
}

void MemoryTracker::Reset() {
    std::lock_guard lock(m_mutex);
    m_allocations.clear();
    m_tagStats.clear();
    m_totalAllocated = 0;
    m_peakAllocated = 0;
}

// ============================================================================
// Allocation Functions Implementation
// ============================================================================

void* Allocate(std::size_t size, TagId tag, const std::source_location& loc) {
    if (size == 0) return nullptr;
    
    void* ptr = nullptr;
    AllocFunc customAlloc = g_customAlloc.load(std::memory_order_acquire);
    if (customAlloc) {
        ptr = customAlloc(size);
    } else {
        ptr = std::malloc(size);
    }
    
    if (ptr) {
        MemoryTracker::Instance().RecordAllocation(ptr, size, alignof(std::max_align_t),
                                                    tag, customAlloc ? AllocationType::Custom : AllocationType::Heap, loc);
    }
    return ptr;
}

void* AllocateAligned(std::size_t size, std::size_t alignment, TagId tag,
                       const std::source_location& loc) {
    if (size == 0) return nullptr;
    
    void* ptr = nullptr;
    AllocFunc customAlloc = g_customAlloc.load(std::memory_order_acquire);
    
    if (customAlloc) {
        // Custom allocator: allocate extra space for alignment and store original pointer
        std::size_t totalSize = size + alignment + sizeof(void*);
        void* raw = customAlloc(totalSize);
        if (raw) {
            // Align the pointer, leaving room for the original pointer
            std::uintptr_t rawAddr = reinterpret_cast<std::uintptr_t>(raw);
            std::uintptr_t aligned = (rawAddr + sizeof(void*) + alignment - 1) & ~(alignment - 1);
            ptr = reinterpret_cast<void*>(aligned);
            // Store original pointer just before the aligned pointer
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
        MemoryTracker::Instance().RecordAllocation(ptr, size, alignment,
                                                    tag, customAlloc ? AllocationType::Custom : AllocationType::Heap, loc);
    }
    return ptr;
}

void Free(void* ptr) {
    if (!ptr) return;
    
    MemoryTracker::Instance().RecordDeallocation(ptr);
    
    DeallocFunc customDealloc = g_customDealloc.load(std::memory_order_acquire);
    if (customDealloc) {
        customDealloc(ptr);
    } else {
        std::free(ptr);
    }
}

void FreeAligned(void* ptr) {
    if (!ptr) return;
    
    MemoryTracker::Instance().RecordDeallocation(ptr);
    
    DeallocFunc customDealloc = g_customDealloc.load(std::memory_order_acquire);
    if (customDealloc) {
        // Retrieve original pointer stored before the aligned pointer
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

void* Reallocate(void* ptr, std::size_t size, TagId tag, const std::source_location& loc) {
    if (size == 0) {
        Free(ptr);
        return nullptr;
    }
    
    if (!ptr) {
        return Allocate(size, tag, loc);
    }
    
    // Record deallocation of old pointer
    MemoryTracker::Instance().RecordDeallocation(ptr);
    
    void* newPtr = nullptr;
    ReallocFunc customRealloc = g_customRealloc.load(std::memory_order_acquire);
    bool usingCustom = false;
    
    if (customRealloc) {
        newPtr = customRealloc(ptr, size);
        usingCustom = true;
    } else {
        newPtr = std::realloc(ptr, size);
    }
    
    if (newPtr) {
        MemoryTracker::Instance().RecordAllocation(newPtr, size, alignof(std::max_align_t),
                                                    tag, usingCustom ? AllocationType::Custom : AllocationType::Heap, loc);
    }
    return newPtr;
}

// ============================================================================
// StackAllocationGuard Implementation
// ============================================================================

StackAllocationGuard::StackAllocationGuard(void* ptr, std::size_t size, TagId tag,
                                           const std::source_location& loc)
    : m_ptr(ptr) {
#if YU_MEMORY_TRACKING_ENABLED
    MemoryTracker::Instance().RecordAllocation(ptr, size, alignof(std::max_align_t),
                                                tag, AllocationType::Stack, loc);
#endif
}

StackAllocationGuard::~StackAllocationGuard() {
#if YU_MEMORY_TRACKING_ENABLED
    MemoryTracker::Instance().RecordDeallocation(m_ptr);
#endif
}

} // namespace mem
} // namespace yu

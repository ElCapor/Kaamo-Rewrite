/**
 * @file memory_detailed.h
 * @brief Full-featured memory tracking with detailed allocation records
 * 
 * This tracker is designed for comprehensive debugging where:
 * - Full allocation records with source location are needed
 * - Detailed timestamps and allocation history are required
 * - Memory leak detection with full stack info is important
 * - Performance is secondary to information completeness
 * 
 * Note: This tracker DOES allocate memory internally. It should NOT be used
 * when hooking the application's memory functions. Use LightweightTracker instead.
 * 
 * Use this for:
 * - Development/debugging builds
 * - Memory analysis tools
 * - Post-mortem leak detection
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
#include <format>
#include <iostream>

namespace yu {
namespace mem {

// ============================================================================
// Detailed Tracker Configuration
// ============================================================================

/// Maximum tag name length
constexpr std::size_t DetailedMaxTagNameLength = 64;

// ============================================================================
// Memory Tag System
// ============================================================================

/// Memory tag identifier
using DetailedTagId = std::uint32_t;

/// Predefined memory tags for DetailedTracker
namespace DetailedTags {
    constexpr DetailedTagId General    = 0;
    constexpr DetailedTagId Graphics   = 1;
    constexpr DetailedTagId Audio      = 2;
    constexpr DetailedTagId Physics    = 3;
    constexpr DetailedTagId AI         = 4;
    constexpr DetailedTagId Network    = 5;
    constexpr DetailedTagId UI         = 6;
    constexpr DetailedTagId Gameplay   = 7;
    constexpr DetailedTagId Resource   = 8;
    constexpr DetailedTagId Temporary  = 9;
    constexpr DetailedTagId UserStart  = 100;  // User-defined tags start here
}

// ============================================================================
// Allocation Type
// ============================================================================

/// Memory allocation type
enum class DetailedAllocationType : std::uint8_t {
    Heap,       // Standard heap allocation (new/malloc)
    Stack,      // Stack-based (for tracking purposes only)
    Pool,       // Pool allocator
    Arena,      // Arena allocator
    Custom      // Custom allocator
};

/// Convert DetailedAllocationType to string
[[nodiscard]] constexpr std::string_view AllocationTypeToString(DetailedAllocationType type) noexcept {
    switch (type) {
        case DetailedAllocationType::Heap:   return "Heap";
        case DetailedAllocationType::Stack:  return "Stack";
        case DetailedAllocationType::Pool:   return "Pool";
        case DetailedAllocationType::Arena:  return "Arena";
        case DetailedAllocationType::Custom: return "Custom";
        default:                             return "Unknown";
    }
}

// ============================================================================
// Detailed Allocation Record
// ============================================================================

/// Full information about a single allocation
struct DetailedAllocationRecord {
    void*                   address{nullptr};
    std::size_t             size{0};
    std::size_t             alignment{0};
    DetailedTagId           tag{DetailedTags::General};
    DetailedAllocationType  type{DetailedAllocationType::Heap};
    const char*             file{nullptr};
    std::uint32_t           line{0};
    std::chrono::steady_clock::time_point timestamp;
    
    [[nodiscard]] std::string ToString() const {
        return std::format("Address: {:p}, Size: {} bytes, Tag: {}, Type: {}, Location: {}:{}",
                           address, size,
                           static_cast<std::uint32_t>(tag),
                           AllocationTypeToString(type),
                           file ? file : "unknown",
                           line);
    }
};

// ============================================================================
// Detailed Tag Statistics
// ============================================================================

/// Statistics for a memory tag
struct DetailedTagStats {
    std::string     name;
    DetailedTagId   id{0};
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
// Detailed Memory Tracker
// ============================================================================

/// Full-featured memory tracking system with detailed records
/// 
/// WARNING: This tracker allocates memory internally using std::unordered_map.
/// Do NOT use this tracker when hooking the application's memory allocator,
/// as it will cause infinite recursion. Use LightweightTracker instead.
class DetailedTracker {
public:
    /// Get singleton instance
    [[nodiscard]] static DetailedTracker& Instance() noexcept {
        static DetailedTracker instance;
        return instance;
    }
    
    /// Check if tracker is shutting down
    [[nodiscard]] static bool IsShuttingDown() noexcept { 
        return s_shuttingDown.load(std::memory_order_acquire); 
    }
    
    ~DetailedTracker() {
        s_shuttingDown.store(true, std::memory_order_release);
    }
    
    // ========================================================================
    // Tag Management
    // ========================================================================
    
    /// Register a new tag name
    void RegisterTag(DetailedTagId id, std::string_view name) {
        std::lock_guard lock(m_mutex);
        m_tagNames[id] = std::string(name);
    }
    
    /// Get tag name by ID
    [[nodiscard]] std::string_view GetTagName(DetailedTagId id) const {
        std::lock_guard lock(m_mutex);
        auto it = m_tagNames.find(id);
        if (it != m_tagNames.end()) {
            return it->second;
        }
        return "Unknown";
    }
    
    // ========================================================================
    // Allocation Recording
    // ========================================================================
    
    /// Record an allocation
    void RecordAllocation(void* ptr, std::size_t size, std::size_t alignment,
                          DetailedTagId tag, DetailedAllocationType type,
                          const std::source_location& loc = std::source_location::current()) {
        if (!m_enabled.load(std::memory_order_relaxed) || !ptr) return;
        if (s_shuttingDown.load(std::memory_order_acquire)) return;
        
        // Prevent recursion from internal allocations
        if (s_insideTracker) return;
        s_insideTracker = true;
        
        DetailedAllocationRecord record{
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
        
        // Update global stats (atomic)
        std::size_t newTotal = m_totalAllocated.fetch_add(size, std::memory_order_relaxed) + size;
        std::size_t currentPeak = m_peakAllocated.load(std::memory_order_relaxed);
        while (newTotal > currentPeak && 
               !m_peakAllocated.compare_exchange_weak(currentPeak, newTotal,
                    std::memory_order_relaxed, std::memory_order_relaxed)) {
        }
        
        s_insideTracker = false;
    }
    
    /// Record a deallocation
    void RecordDeallocation(void* ptr) {
        if (!m_enabled.load(std::memory_order_relaxed) || !ptr) return;
        if (s_shuttingDown.load(std::memory_order_acquire)) return;
        
        // Prevent recursion from internal allocations
        if (s_insideTracker) return;
        s_insideTracker = true;
        
        std::lock_guard lock(m_mutex);
        
        auto it = m_allocations.find(ptr);
        if (it != m_allocations.end()) {
            const auto& record = it->second;
            std::size_t size = record.size;
            DetailedTagId tag = record.tag;
            
            // Update tag stats
            auto statsIt = m_tagStats.find(tag);
            if (statsIt != m_tagStats.end()) {
                auto& stats = statsIt->second;
                stats.currentBytes -= std::min(stats.currentBytes, size);
                stats.totalFreed += size;
                stats.freeCount++;
            }
            
            m_totalAllocated.fetch_sub(size, std::memory_order_relaxed);
            m_allocations.erase(it);
        }
        
        s_insideTracker = false;
    }
    
    // ========================================================================
    // Statistics
    // ========================================================================
    
    /// Get statistics for a specific tag
    [[nodiscard]] DetailedTagStats GetTagStats(DetailedTagId tag) const {
        std::lock_guard lock(m_mutex);
        auto it = m_tagStats.find(tag);
        if (it != m_tagStats.end()) {
            return it->second;
        }
        return DetailedTagStats{};
    }
    
    /// Get all tag statistics
    [[nodiscard]] std::vector<DetailedTagStats> GetAllTagStats() const {
        std::lock_guard lock(m_mutex);
        std::vector<DetailedTagStats> result;
        result.reserve(m_tagStats.size());
        for (const auto& [id, stats] : m_tagStats) {
            result.push_back(stats);
        }
        return result;
    }
    
    /// Get total memory currently allocated
    [[nodiscard]] std::size_t GetTotalAllocatedBytes() const noexcept {
        return m_totalAllocated.load(std::memory_order_relaxed);
    }
    
    /// Get peak memory usage
    [[nodiscard]] std::size_t GetPeakAllocatedBytes() const noexcept {
        return m_peakAllocated.load(std::memory_order_relaxed);
    }
    
    /// Get all active allocations
    [[nodiscard]] std::vector<DetailedAllocationRecord> GetActiveAllocations() const {
        std::lock_guard lock(m_mutex);
        std::vector<DetailedAllocationRecord> result;
        result.reserve(m_allocations.size());
        for (const auto& [ptr, record] : m_allocations) {
            result.push_back(record);
        }
        return result;
    }
    
    /// Get active allocations for a specific tag
    [[nodiscard]] std::vector<DetailedAllocationRecord> GetActiveAllocations(DetailedTagId tag) const {
        std::lock_guard lock(m_mutex);
        std::vector<DetailedAllocationRecord> result;
        for (const auto& [ptr, record] : m_allocations) {
            if (record.tag == tag) {
                result.push_back(record);
            }
        }
        return result;
    }
    
    /// Check for memory leaks (returns count of leaked allocations)
    [[nodiscard]] std::size_t CheckLeaks() const {
        std::lock_guard lock(m_mutex);
        return m_allocations.size();
    }
    
    // ========================================================================
    // Reporting
    // ========================================================================
    
    /// Generate a detailed memory report
    [[nodiscard]] std::string GenerateReport() const {
        std::lock_guard lock(m_mutex);
        
        std::string report;
        report.reserve(4096);
        
        report += "=== YU Detailed Memory Report ===\n\n";
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
    
    /// Print memory report to console
    void PrintReport() const {
        std::cout << GenerateReport() << std::flush;
    }
    
    // ========================================================================
    // Control
    // ========================================================================
    
    /// Reset all tracking data
    void Reset() {
        std::lock_guard lock(m_mutex);
        m_allocations.clear();
        m_tagStats.clear();
        m_totalAllocated.store(0, std::memory_order_relaxed);
        m_peakAllocated.store(0, std::memory_order_relaxed);
    }
    
    /// Enable/disable tracking at runtime
    void SetEnabled(bool enabled) noexcept { 
        m_enabled.store(enabled, std::memory_order_relaxed); 
    }
    
    [[nodiscard]] bool IsEnabled() const noexcept { 
        return m_enabled.load(std::memory_order_relaxed); 
    }
    
    // Prevent copying
    DetailedTracker(const DetailedTracker&) = delete;
    DetailedTracker& operator=(const DetailedTracker&) = delete;
    
private:
    DetailedTracker() {
        // Register default tags
        m_tagNames[DetailedTags::General]   = "General";
        m_tagNames[DetailedTags::Graphics]  = "Graphics";
        m_tagNames[DetailedTags::Audio]     = "Audio";
        m_tagNames[DetailedTags::Physics]   = "Physics";
        m_tagNames[DetailedTags::AI]        = "AI";
        m_tagNames[DetailedTags::Network]   = "Network";
        m_tagNames[DetailedTags::UI]        = "UI";
        m_tagNames[DetailedTags::Gameplay]  = "Gameplay";
        m_tagNames[DetailedTags::Resource]  = "Resource";
        m_tagNames[DetailedTags::Temporary] = "Temporary";
    }
    
    static inline std::atomic<bool> s_shuttingDown{false};
    static inline thread_local bool s_insideTracker{false};
    
    mutable std::mutex m_mutex;
    std::unordered_map<void*, DetailedAllocationRecord> m_allocations;
    std::unordered_map<DetailedTagId, DetailedTagStats> m_tagStats;
    std::unordered_map<DetailedTagId, std::string> m_tagNames;
    std::atomic<std::size_t> m_totalAllocated{0};
    std::atomic<std::size_t> m_peakAllocated{0};
    std::atomic<bool> m_enabled{true};
};

// ============================================================================
// Convenience Functions
// ============================================================================

/// Get the detailed tracker instance
inline DetailedTracker& GetDetailedTracker() noexcept {
    return DetailedTracker::Instance();
}

} // namespace mem
} // namespace yu

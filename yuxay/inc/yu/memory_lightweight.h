/**
 * @file memory_lightweight.h
 * @brief Lightweight lock-free memory tracking for DLL injection
 * 
 * This tracker is designed for high-performance scenarios where:
 * - Zero internal allocations are required (uses preallocated buffers)
 * - Lock-free operations are needed (uses atomics and spinlocks)
 * - Minimal per-allocation overhead is acceptable
 * - The tracker must not interfere with hooked memory functions
 * 
 * Use this for production DLL injection where you're hooking game memory.
 * For detailed debugging with full allocation records, use DetailedTracker instead.
 */

#pragma once

// Platform includes first
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <Windows.h>
    #include <intrin.h>
    #define YU_PAUSE() _mm_pause()
#else
    #include <unistd.h>
    #define YU_PAUSE() __builtin_ia32_pause()
#endif

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace yu {
namespace mem {

// ============================================================================
// Configuration
// ============================================================================

/// Configuration for the lightweight tracker
struct LightweightConfig {
    /// Maximum number of concurrent allocations that can be tracked
    /// Should be at least 2x the expected peak active allocations for good hash table performance
    /// At 50% load factor, linear probing is efficient; above 70% it degrades rapidly
    static constexpr std::size_t MaxAllocations = 262144;  // 256K slots
    
    /// Maximum number of tags supported (kept small for static array)
    static constexpr std::size_t MaxTags = 64;
    
    /// Maximum length of tag names (stored inline)
    static constexpr std::size_t MaxTagNameLength = 32;
    
    /// Number of probes before giving up on insertion (hash collision handling)
    /// Increase this if you see dropped allocations even with low load factor
    static constexpr std::size_t MaxProbes = 64;
    
    /// Use dynamic allocation for records buffer (VirtualAlloc on Windows)
    /// This avoids compiler heap issues with large static arrays
    static constexpr bool UseDynamicBuffer = true;
};

// ============================================================================
// TTAS Spinlock (Test-and-Test-and-Set)
// ============================================================================

/// Lightweight spinlock using TTAS pattern with PAUSE instruction
/// Only used for report generation, not for hot path operations
class Spinlock {
public:
    Spinlock() noexcept = default;
    
    void lock() noexcept {
        for (;;) {
            // Optimistically try to acquire
            if (!m_locked.exchange(true, std::memory_order_acquire)) {
                return;
            }
            // Spin on read to reduce cache coherency traffic
            while (m_locked.load(std::memory_order_relaxed)) {
                YU_PAUSE();
            }
        }
    }
    
    bool try_lock() noexcept {
        return !m_locked.load(std::memory_order_relaxed) &&
               !m_locked.exchange(true, std::memory_order_acquire);
    }
    
    void unlock() noexcept {
        m_locked.store(false, std::memory_order_release);
    }
    
private:
    std::atomic<bool> m_locked{false};
};

/// RAII lock guard for Spinlock
class SpinlockGuard {
public:
    explicit SpinlockGuard(Spinlock& lock) noexcept : m_lock(lock) {
        m_lock.lock();
    }
    ~SpinlockGuard() noexcept {
        m_lock.unlock();
    }
    
    SpinlockGuard(const SpinlockGuard&) = delete;
    SpinlockGuard& operator=(const SpinlockGuard&) = delete;
    
private:
    Spinlock& m_lock;
};

// ============================================================================
// Compact Allocation Record
// ============================================================================

/// Minimal allocation record - 16 bytes on 32-bit, 24 bytes on 64-bit
struct CompactRecord {
    std::atomic<void*> address{nullptr};  // nullptr = free slot
    std::uint32_t      size{0};
    std::uint16_t      tag{0};
    std::uint8_t       flags{0};          // AllocationType in lower 4 bits
    std::uint8_t       reserved{0};
    
    bool isEmpty() const noexcept {
        return address.load(std::memory_order_relaxed) == nullptr;
    }
};

// ============================================================================
// Atomic Tag Statistics
// ============================================================================

/// Per-tag statistics using only atomic operations
struct alignas(64) AtomicTagStats {  // Cache-line aligned to prevent false sharing
    std::atomic<std::size_t>  currentBytes{0};
    std::atomic<std::size_t>  peakBytes{0};
    std::atomic<std::uint64_t> allocCount{0};
    std::atomic<std::uint64_t> freeCount{0};
    char name[LightweightConfig::MaxTagNameLength]{};
    std::atomic<bool> registered{false};
    
    void updatePeak() noexcept {
        std::size_t current = currentBytes.load(std::memory_order_relaxed);
        std::size_t peak = peakBytes.load(std::memory_order_relaxed);
        while (current > peak) {
            if (peakBytes.compare_exchange_weak(peak, current, std::memory_order_relaxed)) {
                break;
            }
        }
    }
};

// ============================================================================
// Lightweight Memory Tracker
// ============================================================================

/// Lock-free memory tracker with zero internal allocations
/// 
/// This tracker uses:
/// - Fixed-size hash table with linear probing for allocation records
/// - Atomic operations for all statistics updates
/// - Spinlock only for report generation (cold path)
/// - No std::string, std::unordered_map, or any allocating containers
class LightweightTracker {
public:
    using TagId = std::uint16_t;
    
    /// Predefined tags (same as DetailedTracker for compatibility)
    struct Tags {
        static constexpr TagId General   = 0;
        static constexpr TagId Graphics  = 1;
        static constexpr TagId Audio     = 2;
        static constexpr TagId Physics   = 3;
        static constexpr TagId AI        = 4;
        static constexpr TagId Network   = 5;
        static constexpr TagId UI        = 6;
        static constexpr TagId Gameplay  = 7;
        static constexpr TagId Resource  = 8;
        static constexpr TagId Temporary = 9;
        static constexpr TagId UserStart = 100;
    };
    
    /// Allocation type (stored in flags)
    enum class AllocationType : std::uint8_t {
        Heap   = 0,
        Stack  = 1,
        Pool   = 2,
        Arena  = 3,
        Custom = 4
    };
    
    /// Get singleton instance
    static LightweightTracker& Instance() noexcept {
        static LightweightTracker instance;
        return instance;
    }
    
    // ========================================================================
    // Hot Path - Lock-Free Operations
    // ========================================================================
    
    /// Record an allocation (lock-free)
    /// @param ptr Pointer to allocated memory
    /// @param size Size in bytes
    /// @param tag Memory tag for categorization
    /// @param type Allocation type
    void RecordAllocation(void* ptr, std::uint32_t size, TagId tag = 0,
                          AllocationType type = AllocationType::Heap) noexcept {
        if (!ptr || !m_records || !m_enabled.load(std::memory_order_relaxed)) return;
        
        // Hash-based slot selection
        std::size_t hash = reinterpret_cast<std::uintptr_t>(ptr);
        hash = hash ^ (hash >> 16);  // Better distribution
        std::size_t idx = hash % LightweightConfig::MaxAllocations;
        
        // Linear probing with CAS
        for (std::size_t probe = 0; probe < LightweightConfig::MaxProbes; ++probe) {
            auto& slot = m_records[(idx + probe) % LightweightConfig::MaxAllocations];
            void* expected = nullptr;
            
            if (slot.address.compare_exchange_strong(expected, ptr,
                    std::memory_order_acq_rel, std::memory_order_relaxed)) {
                // Successfully claimed this slot
                slot.size = size;
                slot.tag = tag;
                slot.flags = static_cast<std::uint8_t>(type);
                
                // Update statistics atomically
                m_totalBytes.fetch_add(size, std::memory_order_relaxed);
                m_activeCount.fetch_add(1, std::memory_order_relaxed);
                
                if (tag < LightweightConfig::MaxTags) {
                    m_tagStats[tag].currentBytes.fetch_add(size, std::memory_order_relaxed);
                    m_tagStats[tag].allocCount.fetch_add(1, std::memory_order_relaxed);
                    m_tagStats[tag].updatePeak();
                }
                
                // Update global peak
                updateGlobalPeak();
                return;
            }
        }
        // Table full or too many collisions - silently drop (acceptable for tracking)
        m_droppedAllocations.fetch_add(1, std::memory_order_relaxed);
    }
    
    /// Record a deallocation (lock-free)
    /// @param ptr Pointer to freed memory
    void RecordDeallocation(void* ptr) noexcept {
        if (!ptr || !m_records || !m_enabled.load(std::memory_order_relaxed)) return;
        
        // Find the record
        std::size_t hash = reinterpret_cast<std::uintptr_t>(ptr);
        hash = hash ^ (hash >> 16);
        std::size_t idx = hash % LightweightConfig::MaxAllocations;
        
        for (std::size_t probe = 0; probe < LightweightConfig::MaxProbes; ++probe) {
            auto& slot = m_records[(idx + probe) % LightweightConfig::MaxAllocations];
            
            if (slot.address.load(std::memory_order_relaxed) == ptr) {
                // Found the record - capture data before clearing
                std::uint32_t size = slot.size;
                TagId tag = slot.tag;
                
                // Clear the slot with CAS to handle races
                void* expected = ptr;
                if (slot.address.compare_exchange_strong(expected, nullptr,
                        std::memory_order_acq_rel, std::memory_order_relaxed)) {
                    // Successfully freed this slot
                    m_totalBytes.fetch_sub(size, std::memory_order_relaxed);
                    m_activeCount.fetch_sub(1, std::memory_order_relaxed);
                    
                    if (tag < LightweightConfig::MaxTags) {
                        auto& stats = m_tagStats[tag];
                        std::size_t current = stats.currentBytes.load(std::memory_order_relaxed);
                        if (current >= size) {
                            stats.currentBytes.fetch_sub(size, std::memory_order_relaxed);
                        } else {
                            stats.currentBytes.store(0, std::memory_order_relaxed);
                        }
                        stats.freeCount.fetch_add(1, std::memory_order_relaxed);
                    }
                }
                return;
            }
            
            // Empty slot means not found
            if (slot.isEmpty()) break;
        }
        // Not found - could be allocation we couldn't track
    }
    
    // ========================================================================
    // Statistics - Lock-Free Reads
    // ========================================================================
    
    /// Get total bytes currently allocated
    [[nodiscard]] std::size_t GetTotalBytes() const noexcept {
        return m_totalBytes.load(std::memory_order_relaxed);
    }
    
    /// Get peak bytes ever allocated
    [[nodiscard]] std::size_t GetPeakBytes() const noexcept {
        return m_peakBytes.load(std::memory_order_relaxed);
    }
    
    /// Get number of active allocations
    [[nodiscard]] std::size_t GetActiveCount() const noexcept {
        return m_activeCount.load(std::memory_order_relaxed);
    }
    
    /// Get bytes for a specific tag
    [[nodiscard]] std::size_t GetTagBytes(TagId tag) const noexcept {
        if (tag >= LightweightConfig::MaxTags) return 0;
        return m_tagStats[tag].currentBytes.load(std::memory_order_relaxed);
    }
    
    /// Get allocation count for a specific tag
    [[nodiscard]] std::uint64_t GetTagAllocCount(TagId tag) const noexcept {
        if (tag >= LightweightConfig::MaxTags) return 0;
        return m_tagStats[tag].allocCount.load(std::memory_order_relaxed);
    }
    
    /// Get number of dropped allocations (table was full)
    [[nodiscard]] std::size_t GetDroppedCount() const noexcept {
        return m_droppedAllocations.load(std::memory_order_relaxed);
    }
    
    // ========================================================================
    // Tag Management - Lock-Free
    // ========================================================================
    
    /// Register a tag name (lock-free, but should be called during init)
    void RegisterTag(TagId id, const char* name) noexcept {
        if (id >= LightweightConfig::MaxTags || !name) return;
        
        auto& stats = m_tagStats[id];
        bool expected = false;
        if (stats.registered.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
            // Copy name safely
            std::size_t len = 0;
            while (name[len] && len < LightweightConfig::MaxTagNameLength - 1) {
                stats.name[len] = name[len];
                ++len;
            }
            stats.name[len] = '\0';
        }
    }
    
    /// Get tag name
    [[nodiscard]] const char* GetTagName(TagId id) const noexcept {
        if (id >= LightweightConfig::MaxTags) return "Unknown";
        if (!m_tagStats[id].registered.load(std::memory_order_relaxed)) return "Unregistered";
        return m_tagStats[id].name;
    }
    
    // ========================================================================
    // Control
    // ========================================================================
    
    /// Enable/disable tracking
    void SetEnabled(bool enabled) noexcept {
        m_enabled.store(enabled, std::memory_order_relaxed);
    }
    
    [[nodiscard]] bool IsEnabled() const noexcept {
        return m_enabled.load(std::memory_order_relaxed);
    }
    
    /// Reset all tracking data (uses spinlock for safety)
    void Reset() noexcept {
        SpinlockGuard guard(m_reportLock);
        
        if (m_records) {
            for (std::size_t i = 0; i < LightweightConfig::MaxAllocations; ++i) {
                m_records[i].address.store(nullptr, std::memory_order_relaxed);
                m_records[i].size = 0;
                m_records[i].tag = 0;
                m_records[i].flags = 0;
            }
        }
        
        for (std::size_t i = 0; i < LightweightConfig::MaxTags; ++i) {
            m_tagStats[i].currentBytes.store(0, std::memory_order_relaxed);
            m_tagStats[i].peakBytes.store(0, std::memory_order_relaxed);
            m_tagStats[i].allocCount.store(0, std::memory_order_relaxed);
            m_tagStats[i].freeCount.store(0, std::memory_order_relaxed);
        }
        
        m_totalBytes.store(0, std::memory_order_relaxed);
        m_peakBytes.store(0, std::memory_order_relaxed);
        m_activeCount.store(0, std::memory_order_relaxed);
        m_droppedAllocations.store(0, std::memory_order_relaxed);
    }
    
    // ========================================================================
    // Report Generation (Cold Path - Uses Spinlock)
    // ========================================================================
    
    /// Generate a simple report into a preallocated buffer
    /// @param buffer Output buffer
    /// @param bufferSize Size of output buffer
    /// @return Number of characters written (excluding null terminator)
    std::size_t GenerateReport(char* buffer, std::size_t bufferSize) const noexcept {
        if (!buffer || bufferSize == 0) return 0;
        
        SpinlockGuard guard(m_reportLock);
        
        std::size_t written = 0;
        auto append = [&](const char* str) {
            while (*str && written < bufferSize - 1) {
                buffer[written++] = *str++;
            }
        };
        
        auto appendNum = [&](std::size_t num) {
            char temp[32];
            int i = 0;
            if (num == 0) {
                temp[i++] = '0';
            } else {
                while (num > 0 && i < 31) {
                    temp[i++] = '0' + (num % 10);
                    num /= 10;
                }
            }
            while (--i >= 0 && written < bufferSize - 1) {
                buffer[written++] = temp[i];
            }
        };
        
        append("=== YU Lightweight Memory Report ===\n");
        append("Total: "); appendNum(m_totalBytes.load()); append(" bytes\n");
        append("Peak:  "); appendNum(m_peakBytes.load()); append(" bytes\n");
        append("Active: "); appendNum(m_activeCount.load()); append(" allocations\n");
        append("Dropped: "); appendNum(m_droppedAllocations.load()); append(" (table full)\n\n");
        
        append("--- Tag Statistics ---\n");
        for (std::size_t i = 0; i < LightweightConfig::MaxTags; ++i) {
            const auto& stats = m_tagStats[i];
            if (stats.allocCount.load(std::memory_order_relaxed) > 0) {
                append("[");
                if (stats.registered.load(std::memory_order_relaxed)) {
                    append(stats.name);
                } else {
                    append("Tag "); appendNum(i);
                }
                append("] ");
                append("Current: "); appendNum(stats.currentBytes.load()); append(" bytes, ");
                append("Peak: "); appendNum(stats.peakBytes.load()); append(" bytes, ");
                append("Allocs: "); appendNum(stats.allocCount.load()); append(", ");
                append("Frees: "); appendNum(stats.freeCount.load()); append("\n");
            }
        }
        
        buffer[written] = '\0';
        return written;
    }
    
    /// Print report to stdout (for debugging)
    void PrintReport() const noexcept {
        char buffer[4096];
        GenerateReport(buffer, sizeof(buffer));
        // Write directly to avoid any allocation
        #ifdef _WIN32
        WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), buffer, 
                     static_cast<DWORD>(std::strlen(buffer)), nullptr, nullptr);
        #else
        write(1, buffer, std::strlen(buffer));
        #endif
    }
    
    /// Write report to a file (no allocations)
    /// @param filename Path to the output file
    /// @return true if file was written successfully
    bool WriteReportToFile(const char* filename) const noexcept {
        if (!filename) return false;
        
        #ifdef _WIN32
        HANDLE hFile = CreateFileA(filename, GENERIC_WRITE, 0, nullptr,
                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) return false;
        
        // Write basic report
        char buffer[8192];
        std::size_t len = GenerateReport(buffer, sizeof(buffer));
        DWORD written = 0;
        WriteFile(hFile, buffer, static_cast<DWORD>(len), &written, nullptr);
        
        // Write active allocations section
        const char* header = "\n--- Active Allocations ---\n";
        WriteFile(hFile, header, static_cast<DWORD>(std::strlen(header)), &written, nullptr);
        
        SpinlockGuard guard(m_reportLock);
        
        std::size_t count = 0;
        for (std::size_t i = 0; i < LightweightConfig::MaxAllocations && count < 10000; ++i) {
            const auto& slot = m_records[i];
            void* addr = slot.address.load(std::memory_order_relaxed);
            if (addr != nullptr) {
                // Format: Address: 0xXXXXXXXX, Size: NNNN, Tag: TTT
                char line[128];
                std::size_t pos = 0;
                
                // "Address: 0x"
                const char* prefix = "Address: 0x";
                while (*prefix) line[pos++] = *prefix++;
                
                // Hex address
                std::uintptr_t addrVal = reinterpret_cast<std::uintptr_t>(addr);
                char hexChars[] = "0123456789ABCDEF";
                for (int shift = (sizeof(void*) * 8) - 4; shift >= 0; shift -= 4) {
                    line[pos++] = hexChars[(addrVal >> shift) & 0xF];
                }
                
                // ", Size: "
                const char* sizePfx = ", Size: ";
                while (*sizePfx) line[pos++] = *sizePfx++;
                
                // Size as decimal
                std::uint32_t size = slot.size;
                char numBuf[16];
                int numPos = 0;
                if (size == 0) {
                    numBuf[numPos++] = '0';
                } else {
                    while (size > 0) {
                        numBuf[numPos++] = '0' + (size % 10);
                        size /= 10;
                    }
                }
                while (--numPos >= 0) line[pos++] = numBuf[numPos];
                
                // ", Tag: "
                const char* tagPfx = ", Tag: ";
                while (*tagPfx) line[pos++] = *tagPfx++;
                
                // Tag name or number
                TagId tag = slot.tag;
                if (tag < LightweightConfig::MaxTags && 
                    m_tagStats[tag].registered.load(std::memory_order_relaxed)) {
                    const char* tagName = m_tagStats[tag].name;
                    while (*tagName && pos < 120) line[pos++] = *tagName++;
                } else {
                    std::uint16_t t = tag;
                    numPos = 0;
                    if (t == 0) {
                        numBuf[numPos++] = '0';
                    } else {
                        while (t > 0) {
                            numBuf[numPos++] = '0' + (t % 10);
                            t /= 10;
                        }
                    }
                    while (--numPos >= 0) line[pos++] = numBuf[numPos];
                }
                
                line[pos++] = '\n';
                WriteFile(hFile, line, static_cast<DWORD>(pos), &written, nullptr);
                ++count;
            }
        }
        
        // Write total count
        char footer[64];
        std::size_t footerPos = 0;
        const char* totalPfx = "\nTotal listed: ";
        while (*totalPfx) footer[footerPos++] = *totalPfx++;
        char numBuf[16];
        int numPos = 0;
        std::size_t c = count;
        if (c == 0) {
            numBuf[numPos++] = '0';
        } else {
            while (c > 0) {
                numBuf[numPos++] = '0' + (c % 10);
                c /= 10;
            }
        }
        while (--numPos >= 0) footer[footerPos++] = numBuf[numPos];
        footer[footerPos++] = '\n';
        WriteFile(hFile, footer, static_cast<DWORD>(footerPos), &written, nullptr);
        
        CloseHandle(hFile);
        return true;
        #else
        // POSIX implementation
        int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) return false;
        
        char buffer[8192];
        std::size_t len = GenerateReport(buffer, sizeof(buffer));
        write(fd, buffer, len);
        
        close(fd);
        return true;
        #endif
    }
    
    /// Get number of active allocations for iteration
    [[nodiscard]] std::size_t CountActiveAllocations() const noexcept {
        std::size_t count = 0;
        for (std::size_t i = 0; i < LightweightConfig::MaxAllocations; ++i) {
            if (m_records[i].address.load(std::memory_order_relaxed) != nullptr) {
                ++count;
            }
        }
        return count;
    }
    
    /// Iterate over all active allocations (callback-based, no allocations)
    /// @param callback Function called for each allocation: void(void* addr, uint32_t size, uint16_t tag)
    template<typename Callback>
    void ForEachAllocation(Callback&& callback) const noexcept {
        SpinlockGuard guard(m_reportLock);
        for (std::size_t i = 0; i < LightweightConfig::MaxAllocations; ++i) {
            const auto& slot = m_records[i];
            void* addr = slot.address.load(std::memory_order_relaxed);
            if (addr != nullptr) {
                callback(addr, slot.size, slot.tag);
            }
        }
    }
    
    // Prevent copying
    LightweightTracker(const LightweightTracker&) = delete;
    LightweightTracker& operator=(const LightweightTracker&) = delete;
    
    ~LightweightTracker() noexcept {
        // Free the dynamically allocated records buffer
        if (m_records) {
            #ifdef _WIN32
            VirtualFree(m_records, 0, MEM_RELEASE);
            #else
            std::free(m_records);
            #endif
            m_records = nullptr;
        }
    }
    
private:
    LightweightTracker() noexcept {
        // Allocate records buffer using OS allocation (bypasses CRT which may be hooked)
        #ifdef _WIN32
        m_records = static_cast<CompactRecord*>(
            VirtualAlloc(nullptr, 
                        sizeof(CompactRecord) * LightweightConfig::MaxAllocations,
                        MEM_COMMIT | MEM_RESERVE, 
                        PAGE_READWRITE));
        #else
        m_records = static_cast<CompactRecord*>(
            std::aligned_alloc(alignof(CompactRecord),
                              sizeof(CompactRecord) * LightweightConfig::MaxAllocations));
        #endif
        
        // Zero-initialize the buffer (VirtualAlloc does this on Windows, but be explicit)
        if (m_records) {
            std::memset(m_records, 0, sizeof(CompactRecord) * LightweightConfig::MaxAllocations);
        }
        
        // Initialize default tag names
        RegisterTag(Tags::General, "General");
        RegisterTag(Tags::Graphics, "Graphics");
        RegisterTag(Tags::Audio, "Audio");
        RegisterTag(Tags::Physics, "Physics");
        RegisterTag(Tags::AI, "AI");
        RegisterTag(Tags::Network, "Network");
        RegisterTag(Tags::UI, "UI");
        RegisterTag(Tags::Gameplay, "Gameplay");
        RegisterTag(Tags::Resource, "Resource");
        RegisterTag(Tags::Temporary, "Temporary");
    }
    
    void updateGlobalPeak() noexcept {
        std::size_t current = m_totalBytes.load(std::memory_order_relaxed);
        std::size_t peak = m_peakBytes.load(std::memory_order_relaxed);
        while (current > peak) {
            if (m_peakBytes.compare_exchange_weak(peak, current, std::memory_order_relaxed)) {
                break;
            }
        }
    }
    
    // Dynamically allocated hash table for allocation records (allocated via VirtualAlloc)
    CompactRecord* m_records{nullptr};
    
    // Per-tag statistics (cache-line aligned)
    AtomicTagStats m_tagStats[LightweightConfig::MaxTags]{};
    
    // Global statistics
    alignas(64) std::atomic<std::size_t> m_totalBytes{0};
    std::atomic<std::size_t> m_peakBytes{0};
    std::atomic<std::size_t> m_activeCount{0};
    std::atomic<std::size_t> m_droppedAllocations{0};
    
    // Control
    std::atomic<bool> m_enabled{true};
    
    // Only for report generation (cold path)
    mutable Spinlock m_reportLock;
};

// ============================================================================
// Convenience Functions
// ============================================================================

/// Get the lightweight tracker instance
inline LightweightTracker& GetLightweightTracker() noexcept {
    return LightweightTracker::Instance();
}

} // namespace mem
} // namespace yu

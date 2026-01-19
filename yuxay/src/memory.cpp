/**
 * @file memory.cpp
 * @brief Implementation of memory tracking system (minimal - most is header-only)
 * 
 * The new memory system is largely header-only:
 * - LightweightTracker: Fully header-only for lock-free operation
 * - DetailedTracker: Mostly header-only with mutex-based synchronization
 * - MemoryTracker: Wrapper that uses LightweightTracker, header-only
 * 
 * This file exists for compatibility and any non-template implementations
 * that may be added in the future.
 */
#define NOMINMAX
#include "yu/memory.h"

// All implementations are now in the headers:
// - memory.h: Main API, MemoryTracker wrapper, allocation functions
// - memory_lightweight.h: LightweightTracker, Spinlock, CompactRecord
// - memory_detailed.h: DetailedTracker with full records

// This file can be used for:
// 1. Explicit template instantiations if needed
// 2. Any future non-inline implementations
// 3. Static variable definitions if required by the linker

namespace yu {
namespace mem {

// Currently nothing needs to be here - everything is inline or header-only

} // namespace mem
} // namespace yu

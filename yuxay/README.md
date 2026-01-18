# Yu Library

A lightweight, modern C++23 utility library designed for game development. Provides essential features without heavy dependencies.

## Features

- **Logging** - Multi-level logging with file output support
- **File I/O** - Read/write bytes and strings with base path management
- **Memory Tracking** - Tagged allocation system with leak detection
- **RAII Helpers** - Scope guards, defer macros, and resource wrappers

## Requirements

- C++23 compatible compiler (GCC 13+, Clang 17+, MSVC 2022+)
- xmake build system
- Supported platforms: Windows, Linux

## Quick Start

```cpp
#include <yu/yu.h>

int main() {
    // Initialize the library
    yu::Initialize();
    
    // Your code here...
    
    // Shutdown (checks for memory leaks)
    yu::Shutdown();
    return 0;
}
```

## Building

```bash
# Build the library
xmake build yu

# Build in debug mode (enables memory tracking)
xmake f -m debug
xmake build yu

# Build in release mode (optimized, minimal tracking)
xmake f -m release
xmake build yu
```

---

## Logging

The logging system provides multi-level logging with console and file output.

### Log Levels

- `Debug` - Detailed debugging information
- `Info` - General information
- `Warning` - Warning messages
- `Error` - Error messages

### Basic Usage

```cpp
#include <yu/log.h>

// Simple logging functions
yu::LogDebug("This is a debug message");
yu::LogInfo("Application started");
yu::LogWarning("Something might be wrong");
yu::LogError("An error occurred");

// Formatted logging with macros
YU_LOG_INFO("Player {} has {} health", playerName, health);
YU_LOG_DEBUG("Position: ({}, {})", x, y);
YU_LOG_ERROR("Failed to load resource: {}", resourcePath);
```

### File Logging

```cpp
// Enable file logging
yu::SetLogFile("logs/game.log");

// Set minimum log level
yu::SetLogLevel(yu::LogLevel::Info);  // Ignores Debug messages

// Disable console output (file only)
yu::Logger::Instance().SetConsoleOutput(false);
```

### Color Output

By default, log messages use ANSI color codes for better readability. Colors are automatically detected based on terminal capabilities:

- **Windows**: Virtual Terminal Processing is enabled if supported (Windows 10+)
- **Unix/Linux**: Checks if stdout is a TTY and respects the `NO_COLOR` environment variable

```cpp
// Check if colors are supported
bool hasColors = yu::Logger::DetectColorSupport();

// Manually disable colors (e.g., for terminals without ANSI support)
yu::Logger::Instance().SetColorOutput(false);

// Re-enable colors
yu::Logger::Instance().SetColorOutput(true);

// Check current color state
bool colorsEnabled = yu::Logger::Instance().IsColorOutput();
```

> **Note**: Colors are only used for console output, not file output.

### Log Output Format

```
[2026-01-11 14:30:45.123] [INFO] [main.cpp:42] Application started
[2026-01-11 14:30:45.125] [DEBUG] [game.cpp:100] Loading resources...
```

---

## File I/O

The I/O library provides simple file operations with base path support.

### Setting Base Path

```cpp
#include <yu/io.h>

// Set base path to executable directory (recommended)
yu::io::InitializeWithExecutablePath();

// Or set a custom base path
yu::io::SetBasePath("assets/");

// All relative paths are now resolved from the base path
```

### Reading Files

```cpp
// Read entire file as bytes
auto bytesResult = yu::io::ReadBytes("data/config.bin");
if (bytesResult) {
    const auto& bytes = *bytesResult;
    // Use bytes...
} else {
    YU_LOG_ERROR("Failed to read file: {}", 
                  yu::io::IOErrorToString(bytesResult.error()));
}

// Read file as string
auto textResult = yu::io::ReadString("config.txt");
if (textResult) {
    std::string content = *textResult;
    // Process text...
}

// Read limited bytes (first 1KB)
auto limited = yu::io::ReadBytesLimited("large_file.dat", 1024);

// Read byte range (offset 100, 50 bytes)
auto range = yu::io::ReadBytesRange("data.bin", 100, 50);
```

### Writing Files

```cpp
// Write string to file
yu::io::WriteString("output.txt", "Hello, World!");

// Write bytes
std::vector<std::uint8_t> data = {0x01, 0x02, 0x03};
yu::io::WriteBytes("data.bin", data);

// Append to file
yu::io::WriteString("log.txt", "New entry\n", yu::io::WriteMode::Append);

// Create new only (fail if exists)
auto result = yu::io::WriteString("unique.txt", "content", 
                                   yu::io::WriteMode::CreateNew);
```

### File System Utilities

```cpp
auto& fs = yu::io::FileSystem::Instance();

// Check file existence
if (fs.Exists("config.json")) {
    // File exists
}

// Check if path is directory or file
if (fs.IsDirectory("assets/")) { }
if (fs.IsFile("data.bin")) { }

// Get file size
auto sizeResult = fs.GetFileSize("large.dat");
if (sizeResult) {
    std::uintmax_t size = *sizeResult;
}

// Create directories
fs.CreateDirectories("output/logs/debug/");
```

---

## Memory Tracking

The memory system provides tagged allocation tracking for debugging and profiling.

### Custom Allocators

You can supply custom allocation functions to be used globally instead of the default `malloc`/`realloc`/`free`:

```cpp
#include <yu/memory.h>

// Define custom allocator functions
void* MyAlloc(std::size_t size) {
    // Your custom allocation logic
    return my_custom_malloc(size);
}

void* MyRealloc(void* ptr, std::size_t size) {
    // Your custom reallocation logic
    return my_custom_realloc(ptr, size);
}

void MyDealloc(void* ptr) {
    // Your custom deallocation logic
    my_custom_free(ptr);
}

int main() {
    // Set custom allocator before any allocations
    yu::mem::SetAllocator(MyAlloc, MyRealloc, MyDealloc);
    
    // All subsequent allocations will use custom functions
    void* mem = yu::mem::Allocate(1024);
    yu::mem::Free(mem);
    
    // Check if custom allocator is active
    if (yu::mem::HasCustomAllocator()) {
        // Custom allocator is in use
    }
    
    // Reset to default allocator
    yu::mem::ResetAllocator();
}
```

**Notes:**
- Call `SetAllocator()` before making any allocations for consistent behavior
- If any function pointer is `nullptr`, the default implementation is used for that operation
- Custom allocators are thread-safe (uses atomic operations)
- When using custom allocators, allocations are tracked with `AllocationType::Custom`

### Reallocation

```cpp
// Reallocate memory (resize existing allocation)
void* mem = yu::mem::Allocate(100, yu::mem::Tags::General);
mem = yu::mem::Reallocate(mem, 200, yu::mem::Tags::General);  // Grow to 200 bytes
yu::mem::Free(mem);

// Reallocate with nullptr acts like Allocate
void* newMem = yu::mem::Reallocate(nullptr, 50);  // Same as Allocate(50)

// Reallocate with size 0 acts like Free
yu::mem::Reallocate(newMem, 0);  // Frees the memory
```

### Predefined Tags

```cpp
yu::mem::Tags::General    // Default general allocations
yu::mem::Tags::Graphics   // Graphics/rendering
yu::mem::Tags::Audio      // Audio system
yu::mem::Tags::Physics    // Physics simulation
yu::mem::Tags::AI         // AI/pathfinding
yu::mem::Tags::Network    // Networking
yu::mem::Tags::UI         // User interface
yu::mem::Tags::Gameplay   // Game logic
yu::mem::Tags::Resource   // Resource/asset loading
yu::mem::Tags::Temporary  // Temporary allocations
```

### Custom Tags

```cpp
// Define custom tags (start from UserStart)
constexpr yu::mem::TagId MyGameTag = yu::mem::Tags::UserStart;
constexpr yu::mem::TagId EntityTag = yu::mem::Tags::UserStart + 1;

// Register tag names
yu::mem::RegisterTag(MyGameTag, "MyGame");
yu::mem::RegisterTag(EntityTag, "Entities");
```

### Allocation Functions

```cpp
#include <yu/memory.h>

// Allocate raw memory with tag
void* rawMem = yu::mem::Allocate(1024, yu::mem::Tags::Graphics);
yu::mem::Free(rawMem);

// Aligned allocation
void* aligned = yu::mem::AllocateAligned(256, 64, yu::mem::Tags::Physics);
yu::mem::FreeAligned(aligned);

// Create typed objects
struct Player { std::string name; int health; };
Player* player = yu::mem::New<Player>(yu::mem::Tags::Gameplay, "Hero", 100);
yu::mem::Delete(player);

// Create arrays
int* values = yu::mem::NewArray<int>(100, yu::mem::Tags::Temporary);
yu::mem::DeleteArray(values, 100);
```

### Smart Pointers

```cpp
// Tagged unique pointer
auto entity = yu::mem::MakeUnique<Entity>(yu::mem::Tags::Gameplay, args...);
// Automatically freed when out of scope
```

### Track Stack Allocations

```cpp
void ProcessData() {
    int buffer[1024];
    YU_TRACK_STACK(buffer, yu::mem::Tags::Temporary);
    // Stack allocation is now tracked
}  // Tracking ends when guard goes out of scope
```

### Memory Reports

```cpp
// Print memory report to console
yu::mem::PrintMemoryReport();

// Get total allocated bytes
std::size_t total = yu::mem::GetTotalAllocated();
std::size_t peak = yu::mem::GetPeakAllocated();

// Check for leaks
std::size_t leaks = yu::mem::CheckLeaks();
if (leaks > 0) {
    YU_LOG_ERROR("Memory leaks detected: {} allocations", leaks);
}

// Get detailed statistics
auto stats = yu::mem::MemoryTracker::Instance().GetTagStats(yu::mem::Tags::Graphics);
YU_LOG_INFO("Graphics memory: {} bytes ({} allocations)", 
            stats.currentBytes, stats.allocationCount);

// Get all active allocations
auto allocs = yu::mem::MemoryTracker::Instance().GetActiveAllocations();
for (const auto& alloc : allocs) {
    YU_LOG_DEBUG("{}", alloc.ToString());
}
```

### Sample Report Output

```
=== YU Memory Report ===

Total Allocated: 15360 bytes
Peak Allocated:  24576 bytes
Active Allocations: 3

--- Tag Statistics ---
[Graphics] Current: 8192 bytes, Peak: 16384 bytes, Allocs: 10, Frees: 8
[Gameplay] Current: 4096 bytes, Peak: 4096 bytes, Allocs: 2, Frees: 0
[Temporary] Current: 3072 bytes, Peak: 4096 bytes, Allocs: 5, Frees: 4

--- Active Allocations ---
Address: 0x1234, Size: 4096 bytes, Tag: Graphics, Type: Heap, Location: renderer.cpp:42
Address: 0x5678, Size: 4096 bytes, Tag: Gameplay, Type: Heap, Location: player.cpp:15
```

---

## RAII Helpers

Utilities for automatic resource management.

### Scope Guards

```cpp
#include <yu/raii.h>

void ProcessFile() {
    FILE* file = fopen("data.txt", "r");
    
    // Guard ensures cleanup on any exit path
    auto guard = yu::MakeScopeGuard([&] { 
        fclose(file); 
    });
    
    // Process file...
    if (error) {
        return;  // File automatically closed
    }
    
    // Success - dismiss guard if you DON'T want cleanup
    // guard.Dismiss();
}  // File automatically closed
```

### Defer Macro (Go-style)

```cpp
void LoadResources() {
    auto* resource = LoadResource();
    YU_DEFER { UnloadResource(resource); };
    
    auto* texture = LoadTexture();
    YU_DEFER { UnloadTexture(texture); };
    
    // Work with resources...
    // Both automatically cleaned up in reverse order
}
```

### Scope Success / Scope Fail

```cpp
void Transaction() {
    BeginTransaction();
    
    // Only commits if no exception
    auto success = yu::MakeScopeSuccess([] { CommitTransaction(); });
    
    // Only rolls back on exception
    auto fail = yu::MakeScopeFail([] { RollbackTransaction(); });
    
    PerformOperations();  // May throw
}
```

### Handle Wrapper

```cpp
// Define traits for your handle type
template<>
struct yu::HandleTraits<HANDLE> {
    static constexpr HANDLE InvalidValue() { return INVALID_HANDLE_VALUE; }
    static void Close(HANDLE h) { CloseHandle(h); }
};

// Use the handle wrapper
yu::Handle<HANDLE> fileHandle(CreateFile(...));
if (fileHandle) {
    // Use fileHandle.Get()
}
// Automatically closed on scope exit
```

### Cleanup Stack

```cpp
yu::CleanupStack cleanup;

void* mem1 = malloc(100);
cleanup.Push([=] { free(mem1); });

void* mem2 = malloc(200);
cleanup.Push([=] { free(mem2); });

// All pushed actions execute in reverse order when stack is destroyed
// Or call cleanup.ExecuteAll() manually
```

### Non-Copyable Classes

```cpp
// Make your class non-copyable but movable
class MyResource : public yu::NonCopyable {
    // Can't copy, but can move
};

// Make it completely immovable
class Singleton : public yu::NonCopyableNonMovable {
    // Can't copy or move
};
```

### Optional Reference

```cpp
// Optional reference (like optional<T&>)
yu::OptionalRef<Player> FindPlayer(const std::string& name) {
    auto it = players.find(name);
    if (it != players.end()) {
        return it->second;  // Returns reference
    }
    return std::nullopt;
}

auto player = FindPlayer("Hero");
if (player) {
    player->TakeDamage(10);
}
```

### Lazy Initialization

```cpp
// Thread-safe lazy initialization
yu::Lazy<ExpensiveResource> resource([] {
    return ExpensiveResource::Load("path/to/resource");
});

// First access creates the object
resource->Use();  // Loaded on first call

// Subsequent accesses return cached value
resource->UseAgain();
```

---

## Complete Example

```cpp
#include <yu/yu.h>

// Custom memory tag for our game
constexpr auto ShipTag = yu::mem::Tags::UserStart;

struct Ship {
    float x, y;
    int health;
    
    Ship(float x, float y) : x(x), y(y), health(100) {}
};

int main() {
    // Initialize library
    yu::Initialize();
    yu::SetLogFile("game.log");
    yu::mem::RegisterTag(ShipTag, "Ships");
    
    YU_LOG_INFO("Game starting...");
    
    // Create game objects with tracked memory
    auto playerShip = yu::mem::MakeUnique<Ship>(ShipTag, 100.0f, 200.0f);
    
    // Load configuration
    if (auto config = yu::io::ReadString("config.json")) {
        YU_LOG_DEBUG("Loaded config: {} bytes", config->size());
    }
    
    // Use defer for cleanup
    YU_DEFER { YU_LOG_INFO("Cleanup complete"); };
    
    // Game loop...
    playerShip->health -= 10;
    YU_LOG_INFO("Player health: {}", playerShip->health);
    
    // Print memory stats
    YU_LOG_DEBUG("Memory used: {} bytes", yu::mem::GetTotalAllocated());
    
    // Shutdown (checks for leaks)
    yu::Shutdown();
    return 0;
}
```

---

## Performance Notes

- Memory tracking can be disabled in release builds by defining `YU_MEMORY_TRACKING_ENABLED=0`
- In release mode, allocation functions become thin wrappers around `malloc`/`free`
- Logging uses `std::format` for efficient string formatting
- File I/O uses standard library streams with buffering

## License

Part of the RayShip project.

#include <hooks.h>
#include <Windows.h>
#include <detours.h>
#include <gof2/globals.hpp>
#include <abyss/offsets/offsets.hpp>
#include <yu/yu.h>
#include <yu/memory_lightweight.h>
#include <game.h>

#include <abyss/stdlib.h>

// Get reference to lightweight tracker for direct access (faster, no wrapper overhead)
static yu::mem::LightweightTracker& g_tracker = yu::mem::LightweightTracker::Instance();

std::uintptr_t __stdcall InitHook(std::uintptr_t a1, std::uintptr_t a2, std::uintptr_t a3) {
    YU_LOG_INFO("InitHook called with args: {}, {}, {}", a1, a2, a3);
    auto ret = gof2::globals::init(a1, a2, a3);
    game::globals_initialized = true;
    return ret;
}

// void* operator new(std::size_t size) {
//     void* ptr = abyss::stdlib::malloc(size);
//     return ptr;
// }

// void* operator new[](std::size_t size) {
//     return abyss::stdlib::newarray(size);
// }

// void operator delete(void* ptr) noexcept {
//     abyss::stdlib::free(ptr);
// }

// void operator delete[](void* ptr) noexcept {
//     abyss::stdlib::deletearray(ptr);
// }

void* operator new(std::size_t size) {
    if (!game::yu_ready)
    {
        void* ptr = _malloc_base(size);
        game::whitelisted_allocs[(reinterpret_cast<std::uintptr_t>(ptr) >> 4) % 65536] = ptr;
        return ptr;
    }
    void* ptr = abyss::stdlib::malloc(size);
    // Direct call to lightweight tracker - lock-free, zero allocations
    g_tracker.RecordAllocation(ptr, static_cast<std::uint32_t>(size));
    return ptr;
}

void* operator new[](std::size_t size) {
    if (!game::yu_ready)
    {
        void* ptr = _malloc_base(size);
        game::whitelisted_allocs[(reinterpret_cast<std::uintptr_t>(ptr) >> 4) % 65536] = ptr;
        return ptr;
    }
    void* ptr = abyss::stdlib::newarray(size);
    g_tracker.RecordAllocation(ptr, static_cast<std::uint32_t>(size));
    return ptr;
}

void operator delete(void* ptr) noexcept {
    if (!ptr) return;
    if (!game::yu_ready || game::is_whitelisted_alloc(ptr))
    {
        _free_base(ptr);
        return;
    }
    // Direct call to lightweight tracker - lock-free, zero allocations
    g_tracker.RecordDeallocation(ptr);
    abyss::stdlib::free(ptr);
}

void operator delete[](void* ptr) noexcept {
    if (!ptr) return;
    if (!game::yu_ready || game::is_whitelisted_alloc(ptr))
    {
        _free_base(ptr);
        return;
    }
    g_tracker.RecordDeallocation(ptr);
    abyss::stdlib::deletearray(ptr);
}


void* malloc_hook(std::size_t size) {
    void* addr = abyss::stdlib::malloc(size);
    // Direct call to lightweight tracker - lock-free, zero allocations
    g_tracker.RecordAllocation(addr, static_cast<std::uint32_t>(size));
    return addr;
}

void* realloc_hook(void* ptr, std::size_t newSize) {
    g_tracker.RecordDeallocation(ptr);
    void* addr = abyss::stdlib::realloc(ptr, newSize);
    g_tracker.RecordAllocation(addr, static_cast<std::uint32_t>(newSize));
    return addr;
}

void free_hook(void* ptr) {
    g_tracker.RecordDeallocation(ptr);
    abyss::stdlib::free(ptr);
}


void hooks::InstallHooks() {
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)gof2::globals::init, (PVOID)InitHook);
    DetourTransactionCommit();
}

void hooks::EarlyMemoryHookSetup() 
{
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)abyss::stdlib::malloc, (PVOID)malloc_hook);
    DetourAttach(&(PVOID&)abyss::stdlib::realloc, (PVOID)realloc_hook);
    DetourAttach(&(PVOID&)abyss::stdlib::free, (PVOID)free_hook);
    DetourTransactionCommit();
}
#include <hooks.h>
#include <Windows.h>
#include <detours.h>
#include <gof2/globals.hpp>
#include <abyss/offsets/offsets.hpp>
#include <yu/yu.h>
#include <yu/memory.h>
#include <source_location>
#include <malloc.h>
#include <game.h>

#include <abyss/stdlib.h>

std::uintptr_t __stdcall InitHook(std::uintptr_t a1, std::uintptr_t a2, std::uintptr_t a3) {
    // Custom behavior can be added here
    YU_LOG_INFO("InitHook called with args: {}, {}, {}", a1, a2, a3);
    game::globals_initialized = true;
    // Call the original function
    return gof2::globals::init(a1, a2, a3);
}

void* operator new(std::size_t size) {
    if (!game::globals_initialized) {
        return _malloc_base(size);
    }
    return abyss::stdlib::malloc(size);
}

void* operator new[](std::size_t size) {
    if (!game::globals_initialized) {
        return _malloc_base(size);
    }
    void* addr = abyss::stdlib::newarray(size);
    yu::mem::MemoryTracker::Instance().RecordAllocation(addr, size,std::alignment_of_v<std::max_align_t>,yu::mem::Tags::General, yu::mem::AllocationType::Heap);
    return addr;
}

void operator delete(void* ptr) noexcept {
    if (!game::globals_initialized) {
        return _free_base(ptr);
    }
    abyss::stdlib::free(ptr);
}

void operator delete[](void* ptr) noexcept {
    if (!game::globals_initialized) {
        return _free_base(ptr);
    }
    yu::mem::MemoryTracker::Instance().RecordDeallocation(ptr);
    return abyss::stdlib::deletearray(ptr);
}

void* malloc_hook(std::size_t size) {
    //YU_LOG_DEBUG("malloc_hook called with size: {}", size);
    void* addr = abyss::stdlib::malloc(size);
    yu::mem::MemoryTracker::Instance().RecordAllocation(addr, size, std::alignment_of_v<std::max_align_t>, yu::mem::Tags::General, yu::mem::AllocationType::Heap);
    return addr;
}

void* realloc_hook(void* ptr, std::size_t newSize) {
    //YU_LOG_DEBUG("realloc_hook called with ptr: {}, newSize: {}", (void*)ptr, newSize);
    yu::mem::MemoryTracker::Instance().RecordDeallocation(ptr);
    void* addr = abyss::stdlib::realloc(ptr, newSize);
    yu::mem::MemoryTracker::Instance().RecordAllocation(addr, newSize, std::alignment_of_v<std::max_align_t>, yu::mem::Tags::General, yu::mem::AllocationType::Heap);
    return addr;
}

void free_hook(void* ptr) {
    //YU_LOG_DEBUG("free_hook called with ptr: {}", (void*)ptr);
    yu::mem::MemoryTracker::Instance().RecordDeallocation(ptr);
    abyss::stdlib::free(ptr);
}


void hooks::InstallHooks() {
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)gof2::globals::init, (PVOID)InitHook);
    DetourAttach(&(PVOID&)abyss::stdlib::malloc, (PVOID)malloc_hook);
    DetourAttach(&(PVOID&)abyss::stdlib::realloc, (PVOID)realloc_hook);
    DetourAttach(&(PVOID&)abyss::stdlib::free, (PVOID)free_hook);
    DetourTransactionCommit();
}
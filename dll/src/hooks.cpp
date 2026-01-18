#include <hooks.h>
#include <Windows.h>
#include <detours.h>
#include <gof2/globals.hpp>
#include <abyss/offsets/offsets.hpp>
#include <yu/yu.h>


std::uintptr_t __stdcall InitHook(std::uintptr_t a1, std::uintptr_t a2, std::uintptr_t a3) {
    // Custom behavior can be added here
    YU_LOG_INFO("InitHook called with args: {}, {}, {}", a1, a2, a3);
    // Call the original function
    return gof2::globals::init(a1, a2, a3);
}

void hooks::InstallHooks() {
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)gof2::globals::init, (PVOID)InitHook);
    DetourTransactionCommit();
}
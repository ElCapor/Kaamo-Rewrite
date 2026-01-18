#include <entry_point.h>
#include <utils.h>
#include <yu/yu.h>
#include <abyss/offsets/offsets.hpp>
#include <hooks.h>
#include <game.h>
#include <abyss/stdlib.h>

namespace kaamo {
    void EntryPoint() {
        utils::OpenConsole();
        yu::Initialize();
        yu::SetLogFile("kaamo.log");
        YU_LOG_INFO("Kaamo DLL initialized");
        yu::mem::RegisterTag(101, "AEString");
        yu::mem::RegisterTag(102, "AEArray");
        yu::mem::SetAllocator(abyss::stdlib::malloc, abyss::stdlib::realloc, abyss::stdlib::free);

        hooks::InstallHooks();

        while (!game::quit) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }


        yu::mem::PrintMemoryReport();
        yu::Shutdown();
    }
}

bool game::globals_initialized = false;
bool game::quit = false;
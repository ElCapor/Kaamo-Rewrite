#include <entry_point.h>
#include <utils.h>
#include <yu/yu.h>
#include <yu/memory_lightweight.h>
#include <abyss/offsets/offsets.hpp>
#include <hooks.h>
#include <game.h>
#include <abyss/stdlib.h>
#include <iostream>
#include <Windows.h>
#include <abyss/PaintCanvas.h>

namespace kaamo
{
    void EntryPoint()
    {
        hooks::EarlyMemoryHookSetup();
        utils::OpenConsole();
        yu::Initialize();
        yu::Logger::Instance().SetColorOutput(false);
        yu::SetLogFile("kaamo.log");
        YU_LOG_INFO("Kaamo DLL initialized");
        
        // Use lightweight tracker directly for tag registration
        auto& tracker = yu::mem::LightweightTracker::Instance();
        tracker.RegisterTag(101, "AEString");
        tracker.RegisterTag(102, "AEArray");

        hooks::InstallHooks();
        
        YU_LOG_INFO("Allocation tests");
        {
            // Test allocations using the game's allocator directly
            void* ptr1 = abyss::stdlib::malloc(128);
            void* ptr2 = abyss::stdlib::malloc(256);
            ptr1 = abyss::stdlib::realloc(ptr1, 512);
            abyss::stdlib::free(ptr1);
            abyss::stdlib::free(ptr2);
        }

        std::this_thread::sleep_for(std::chrono::seconds(2));
        tracker.PrintReport();
        game::yu_ready = true;



        while (!game::quit)
        {
            if (GetAsyncKeyState(VK_DELETE) & 0x8000)
            {
                //tracker.PrintReport();
                
                auto canvas = abyss::canvas;
                YU_LOG_INFO("Canvas has {} transforms", canvas->transforms.Size());
            }
            if (GetAsyncKeyState(VK_F5) & 0x8000)
            {
                game::quit = true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Write final memory report to file before shutdown
        YU_LOG_INFO("Game closing - writing final memory report...");
        if (tracker.WriteReportToFile("memory_report.txt")) {
            YU_LOG_INFO("Memory report saved to memory_report.txt");
        } else {
            YU_LOG_ERROR("Failed to write memory report to file");
        }
        
        yu::Shutdown();
    }
}

bool game::globals_initialized = false;
bool game::quit = false;
bool game::yu_ready = false;
void* game::whitelisted_allocs[65536] = {};

bool game::is_whitelisted_alloc(void* ptr) {
    for (void* addr : whitelisted_allocs) {
        if (addr == ptr) {
            return true;
        }
    }
    return false;
}
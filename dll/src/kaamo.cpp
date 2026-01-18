#include <entry_point.h>
#include <utils.h>
#include <yu/yu.h>
#include <abyss/offsets/offsets.hpp>
#include <hooks.h>
#include <game.h>
#include <abyss/stdlib.h>
#include <iostream>
#include <Windows.h>

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
        yu::mem::RegisterTag(101, "AEString");
        yu::mem::RegisterTag(102, "AEArray");
        yu::mem::SetAllocator(abyss::stdlib::malloc, abyss::stdlib::realloc, abyss::stdlib::free);

        hooks::InstallHooks();
        
        std::cin.ignore();

        YU_LOG_INFO("Allocation tests");
        {
            void *ptr1 = yu::mem::Allocate(128, 200);
            void *ptr2 = yu::mem::Allocate(256, 201);
            ptr1 = yu::mem::Reallocate(ptr1, 512, 200);
            yu::mem::Free(ptr1);
            yu::mem::Free(ptr2);

            void *ptr3 = new int[100];
            delete[] static_cast<int *>(ptr3);
        }

        std::this_thread::sleep_for(std::chrono::seconds(2));
        yu::mem::PrintMemoryReport();
        game::yu_ready = true;

        while (!game::quit)
        {
            if (GetAsyncKeyState(VK_DELETE) & 0x8000)
            {
                yu::mem::PrintMemoryReport();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        yu::Shutdown();
    }
}

bool game::globals_initialized = false;
bool game::quit = false;
bool game::yu_ready = false;
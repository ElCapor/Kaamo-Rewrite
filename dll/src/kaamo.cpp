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
#include <dx9hook/d9hook.hpp>
#include <dx9hook/d9draw.hpp>
#include <dx9hook/dinput.hpp>
#include <abyss/AEArray.h>
#include <abyss/Transform.h>
#include <functional>


class KaamoWidget : public d9widget
{
public:
    void Init() override
    {
        // Initialization code for the widget
    }

    void Render(float dt) override
    {
        ImGui::Begin("Kaamo Overlay");
        ImGui::Text("Kaamo DLL is active.");

        // Render canvas transforms
        abyss::PaintCanvas* canvas = *reinterpret_cast<abyss::PaintCanvas**>(abyss::offsets::globals::canvas);
        if (canvas)
        {
            RenderAEArray("Canvas Transforms", canvas->transforms);
        }
        else
        {
            ImGui::Text("Canvas not available");
        }

        ImGui::End();
    }

private:
    template <typename T>
    void RenderAEArray(const char* label, const abyss::Array<T>& arr, std::function<void(const T&, uint32_t)> customRenderer = nullptr)
    {
        if (ImGui::TreeNode(label))
        {
            ImGui::Text("Size: %u", arr.Size());
            ImGui::Text("Capacity: %u", arr.Capacity());

            if (arr.Size() > 0 && ImGui::TreeNode("Elements"))
            {
                for (uint32_t i = 0; i < arr.Size(); ++i)
                {
                    ImGui::PushID(i);
                    if (ImGui::TreeNode("Element", "Element %u", i))
                    {
                        if (customRenderer)
                        {
                            customRenderer(arr[i], i);
                        }
                        else
                        {
                            // Default rendering
                            RenderElement(arr[i]);
                        }
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
                ImGui::TreePop();
            }
            ImGui::TreePop();
        }
    }

    template <typename T>
    void RenderElement(const T& element)
    {
        if constexpr (std::is_pointer_v<T>)
        {
            ImGui::Text("Pointer: %p", (void*)element);
            // Try to cast to known types
            if constexpr (std::is_same_v<std::remove_pointer_t<T>, abyss::Transform>)
            {
                RenderTransform(static_cast<abyss::Transform*>(element));
            }
        }
        else if constexpr (std::is_arithmetic_v<T>)
        {
            ImGui::Text("Value: %f", static_cast<double>(element));
        }
        else if constexpr (std::is_same_v<T, std::string>)
        {
            ImGui::Text("String: %s", element.c_str());
        }
        else if constexpr (std::is_same_v<T, std::uintptr_t>)
        {
            ImGui::Text("uintptr_t: %p", (void*)element);
        }
        else
        {
            // For non-pointer types, show basic info
            ImGui::Text("Value at index (non-pointer)");
            
        }
    }

    void RenderTransform(abyss::Transform* transform)
    {
        if (!transform)
        {
            ImGui::Text("Null transform");
            return;
        }
        ImGui::Text("Transform pointer: %p", (void*)transform);
        RenderAEArray("Meshes", transform->meshes);
        // Add more fields if Transform has them, e.g.:
        // ImGui::Text("Position: %.2f, %.2f, %.2f", transform->position.x, ...);
        // If Transform has an array field, render it recursively
        // RenderAEArray("Child Transforms", transform->children);
    }
};

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

        d9::HookDirectX();
        d9::HookWindow();
        dinput::InitHook();
        d9draw::RegisterWidget(new KaamoWidget());

        while (!game::quit)
        {
            if (GetAsyncKeyState(VK_DELETE) & 0x8000)
            {
                //tracker.PrintReport();
                
                abyss::PaintCanvas* canvas = *reinterpret_cast<abyss::PaintCanvas**>(abyss::offsets::globals::canvas);
                YU_LOG_INFO("Canvas {}", reinterpret_cast<void*>(canvas));
                YU_LOG_INFO("Transforms count: {}", canvas ? canvas->transforms.Size() : 0);
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
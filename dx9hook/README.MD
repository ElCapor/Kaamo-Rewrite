# dx9hook

A DirectX 9 hooking library for injecting ImGui overlays into Windows applications, primarily designed for game modification and debugging purposes.

## Features

- **DirectX 9 Hooking**: Hooks `EndScene` and `Reset` functions to inject custom rendering
- **ImGui Integration**: Full ImGui support with DirectX 9 backend
- **Input Handling**: Hooks window messages and DirectInput for seamless input capture
- **Widget System**: Extensible widget-based architecture for UI components
- **Hotkeys**: Built-in toggle (DELETE key) and unload (F8 key) functionality
- **Thread-Safe**: Uses Microsoft Detours for safe function hooking

## Dependencies

- **ImGui**: Immediate Mode GUI library
- **Microsoft Detours**: For function hooking
- **DirectX 9**: Windows DirectX 9 SDK
- **DirectInput**: For input device hooking

## Building

This library uses [xmake](https://xmake.io/) as the build system.

```bash
xmake build
```

The build configuration includes:
- C++23 standard
- Static library output
- Windows system libraries (d3d9, dxguid, user32, etc.)
- Debug mode adds `DX9HOOK_DEBUG` define

## Usage

### Basic Setup

1. Include the library headers:
```cpp
#include <dx9hook/d9hook.hpp>
#include <dx9hook/d9draw.hpp>
#include <dx9hook/dinput.hpp>
```

2. Initialize the hooks in your DLL entry point:
```cpp
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
    {
        d9::hDDLModule = hModule;
        d9::HookDirectX();
        d9::HookWindow();
        dinput::InitHook();
    }
    return TRUE;
}
```

### Creating Widgets

Extend the `d9widget` class to create custom UI components:

```cpp
class MyWidget : public d9widget
{
public:
    void Init() override
    {
        // Initialize your widget
    }

    void Render(float dt) override
    {
        ImGui::Begin("My Widget");
        ImGui::Text("Hello from dx9hook!");
        ImGui::End();
    }
};
```

Register your widget:
```cpp
d9draw::RegisterWidget(new MyWidget());
```

### API Reference

#### d9 Class (DirectX 9 Hooking)

- `static void HookDirectX()`: Hooks EndScene and Reset functions
- `static void UnHookDirectX()`: Removes DirectX hooks
- `static void HookWindow()`: Hooks window procedure for input
- `static void UnHookWindow()`: Removes window hook
- `static bool WantsMouse()`: Returns whether ImGui wants mouse input

#### d9draw Class (ImGui Rendering)

- `static void RegisterWidget(d9widget* widget)`: Registers a widget for rendering
- `static HRESULT APIENTRY hkEndScene(LPDIRECT3DDEVICE9 D3D9Device)`: Hooked EndScene function

#### dinput Namespace (DirectInput Hooking)

- `void InitHook()`: Initializes DirectInput hooks
- Various hook functions for device state and data

### Hotkeys

- **DELETE**: Toggle ImGui display on/off
- **F8**: Unload the library and remove all hooks

## Architecture

The library consists of three main components:

1. **d9hook**: Core DirectX 9 and window hooking functionality
2. **d9draw**: ImGui rendering and widget management
3. **dinput**: DirectInput device hooking for input capture

## Thread Safety

The hooking operations use Microsoft Detours which provides thread-safe function interception. However, ImGui rendering occurs in the game's render thread, so widget implementations should be thread-aware if accessing shared data.

## Limitations

- Windows only (due to DirectX 9 and Win32 API dependencies)
- Requires applications using DirectX 9
- May conflict with other hooking libraries
- Anti-cheat systems may detect the hooks

## License

See the main project LICENSE file for licensing information.
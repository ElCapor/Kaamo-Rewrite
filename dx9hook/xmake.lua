target("dx9hook")
    set_kind("static")
    set_languages("c++23")
    add_files("src/*.cpp")
    add_includedirs("inc", {public = true})
    add_packages("imgui", "microsoft-detours")
    if is_plat("windows") then
        add_syslinks("dinput8","d3d9", "dxguid", "ws2_32", "mswsock", "advapi32", "shell32", "user32", "gdi32")
    end

    if is_mode("debug") then
        add_defines("DX9HOOK_DEBUG")
    end
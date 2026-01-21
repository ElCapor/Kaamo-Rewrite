
local game_path = "D:/Games/Galaxy On Fire 2"

target("kaamoclubmodapi")
    set_kind("shared")
    add_files("src/**.cpp")
    add_includedirs("include")
    add_packages("lua", "sol2", "minhook", "microsoft-detours", "imgui")
    add_deps("abyss", "yu", "dx9hook")
    add_syslinks("user32")
    set_languages("c++23")
    add_defines("NOMINMAX")

    after_build(function (target)
        os.cp(target:targetfile(), "build")
        os.cp(target:targetfile(), game_path .. "/kaamoclubmodapi.dll")
    end)
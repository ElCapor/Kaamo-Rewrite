--! Debug/Release modes and x86 architecture
add_rules("mode.debug", "mode.release")
set_arch("x86")

--! Dependencies
add_requires("lua", {configs = {arch = "x86"}})
add_requires("sol2", {configs = {arch = "x86"}})
add_requires("minhook", {configs = {arch = "x86"}})
add_requires("microsoft-detours")
add_requires("imgui", {configs= {dx9=true, win32=true}})


--! Targets
includes("abyss", "dll", "yuxay", "boost-ut", "dx9hook")







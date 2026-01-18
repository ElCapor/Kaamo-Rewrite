target("proxydll")
    set_kind("phony")
    
    on_build(function (target)
        import("core.project.config")        
        local mode = config.get("mode")
        
        local args = {"build"}
        table.insert(args, "--target")
        table.insert(args, "i686-pc-windows-msvc")
        
        if mode == "release" then
            table.insert(args, "--release")
        end
        
        os.vrunv("cargo", args, {curdir = "proxydll"})                
        os.cp("proxydll/target/i686-pc-windows-msvc/" .. (mode == "release" and "release" or "debug") .. "/d3d9.dll", "build")
    end)
    
    on_clean(function (target)
        os.vrunv("cargo", {"clean"}, {curdir = "proxydll"})
        os.tryrm("build/d3d9.dll")
    end)

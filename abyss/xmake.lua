target("abyss")
    set_kind("static")
    set_languages("cxx23")
    add_files("src/**.cpp")
    add_includedirs("include", {public = true})
    add_defines("NOMINMAX")
    add_deps("yu")

for _, testfile in ipairs(os.files("tests/*.cpp")) do
    local testname = path.basename(testfile)
    target(testname)
        set_languages("c++23")
        set_kind("binary")
        set_default(false)
        add_files(testfile)
        add_deps("abyss", "boost.ut")
        add_tests("default", {
            output = true,
            verbose = true
        })
end

-- Documentation generation task
task("docs")
    set_category("action")
    on_run(function ()
        local abyss_dir = path.join(os.projectdir(), "abyss")
        local doxyfile = path.join(abyss_dir, "Doxyfile")
        if not os.exists(doxyfile) then
            print("Doxyfile not found!")
            return
        end
        os.cd(abyss_dir)
        print("Generating Doxygen documentation...")
        local ret = os.exec("doxygen Doxyfile")
        local output_dir = path.join(abyss_dir, "docs", "html")
        if os.exists(output_dir) and os.exists(path.join(output_dir, "index.html")) then
            print("Documentation generated successfully in abyss/docs/html/")
        else
            print("Failed to generate documentation")
        end
    end)
    set_menu {
        usage = "xmake docs",
        description = "Generate Doxygen documentation"
    }
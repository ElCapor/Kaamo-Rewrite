target("abyss")
    set_kind("static")
    set_languages("cxx23")
    add_files("src/**.cpp")
    add_includedirs("include", {public = true})
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
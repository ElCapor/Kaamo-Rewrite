#include <utils.h>
#include <Windows.h>
#include <iostream>

namespace kaamo::utils {
    void OpenConsole() {
        AllocConsole();
        FILE* dummyfile;
        freopen_s(&dummyfile, "CONOUT$", "w", stdout);
        freopen_s(&dummyfile, "CONIN$", "r", stdin);
    }
}

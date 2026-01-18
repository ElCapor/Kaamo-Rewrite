#include <cstdint>
#include <abyss/offsets/offsets.hpp>

namespace gof2 {
    namespace globals {
        using init_t = std::uintptr_t (__stdcall*)(std::uintptr_t, std::uintptr_t, std::uintptr_t);
        inline init_t init = reinterpret_cast<init_t>(abyss::offsets::functions::globals::init);
    }
}
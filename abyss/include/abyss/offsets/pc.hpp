#include <cstdint>
namespace abyss::offsets {
    namespace globals {
    }

    namespace functions {
        namespace globals {
            constexpr std::uintptr_t init = 0x0044B20C;
        }

        namespace stdlib {
            constexpr std::uintptr_t malloc = 0x0052D150;
            constexpr std::uintptr_t free = 0x0052D14C;
            constexpr std::uintptr_t realloc = 0x0052D154;
        }
    }
}
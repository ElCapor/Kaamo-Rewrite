#include <cstddef>
#include <abyss/offsets/offsets.hpp>

namespace abyss {
    namespace stdlib {
        using malloc_t = void *(*)(std::size_t size);
        using free_t = void (*)(void *ptr);
        using realloc_t = void *(*)(void *ptr, std::size_t newSize);

        inline malloc_t malloc = reinterpret_cast<malloc_t>(abyss::offsets::functions::stdlib::malloc);
        inline free_t free = reinterpret_cast<free_t>(abyss::offsets::functions::stdlib::free);
        inline realloc_t realloc = reinterpret_cast<realloc_t>(abyss::offsets::functions::stdlib::realloc);
    }
}
#include <cstddef>
#include <abyss/offsets/offsets.hpp>

namespace abyss {
    namespace stdlib {
        using malloc_t = void *(__cdecl*)(std::size_t size);
        using free_t = void (__cdecl*)(void *ptr);
        using realloc_t = void *(__cdecl*)(void *ptr, std::size_t newSize);
        using newarray_t = void *(__cdecl*)(std::size_t size);
        using deletearray_t = void (__cdecl*)(void* ptr);

        inline malloc_t malloc = *reinterpret_cast<malloc_t*>(abyss::offsets::functions::stdlib::malloc);
        inline free_t free = *reinterpret_cast<free_t*>(abyss::offsets::functions::stdlib::free);
        inline realloc_t realloc = *reinterpret_cast<realloc_t*>(abyss::offsets::functions::stdlib::realloc);
        inline newarray_t newarray = *reinterpret_cast<newarray_t*>(abyss::offsets::functions::stdlib::newarray);
        inline deletearray_t deletearray = *reinterpret_cast<deletearray_t*>(abyss::offsets::functions::stdlib::deletearray);
    }
}
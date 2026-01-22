#ifndef TRANSFORM_H
#define TRANSFORM_H

#include <abyss/AEArray.h>

namespace abyss
{
    class Transform
    {
    public:
        char pad_0[0x3C];
        abyss::Array<std::uintptr_t> meshes; //0x3C
    };
    
} // namespace abyss

#endif // TRANSFORM_H
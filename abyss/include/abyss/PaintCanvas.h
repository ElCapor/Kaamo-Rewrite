#include <abyss/Transform.h>

namespace abyss
{
    class PaintCanvas {
    public:
        char pad_1[0x168];
        abyss::Array<abyss::Transform*> transforms; // at offset 0x168
    };

    PaintCanvas* canvas = *reinterpret_cast<PaintCanvas**>(abyss::offsets::globals::canvas);
}
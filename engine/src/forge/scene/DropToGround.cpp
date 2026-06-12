#include "DropToGround.h"

#include <algorithm>

namespace forge {

float DropOffsetY(const AABB& box, const std::vector<AABB>& others)
{
    float supportY = 0.0f; // the ground plane is always a support
    for (const AABB& o : others) {
        if (!o.Valid())
            continue;
        bool xzOverlap = o.min.x < box.max.x && o.max.x > box.min.x && o.min.z < box.max.z &&
                         o.max.z > box.min.z;
        if (!xzOverlap)
            continue;
        if (o.max.y > box.max.y)
            continue; // beside us, not underneath
        supportY = std::max(supportY, o.max.y);
    }
    return supportY - box.min.y;
}

} // namespace forge

#pragma once

#include "forge/core/Geometry.h"

#include <vector>

namespace forge {

// World-space Y offset that rests `box` on the highest support among `others`,
// falling back to the ground plane y=0. A candidate supports the box when
// their XZ footprints overlap AND the candidate's top is not above the box's
// top — a taller neighbor standing beside the box must not teleport it upward.
// Works in both directions: positive offset lifts a sunken object, negative
// drops a floating one.
float DropOffsetY(const AABB& box, const std::vector<AABB>& others);

} // namespace forge

#include "test_framework.h"

#include <forge/scene/DropToGround.h>

namespace forge::test {

void RunDropToGroundTests()
{
    // --- empty scene: floating box drops to the ground plane ------------------
    {
        AABB box{{-0.5f, 2.0f, -0.5f}, {0.5f, 3.0f, 0.5f}};
        CHECK(ApproxEq(DropOffsetY(box, {}), -2.0f)); // bottom 2.0 -> 0.0
    }

    // --- sunken box lifts up to the ground plane ------------------------------
    {
        AABB box{{-0.5f, -0.4f, -0.5f}, {0.5f, 0.6f, 0.5f}};
        CHECK(ApproxEq(DropOffsetY(box, {}), 0.4f));
    }

    // --- already resting: zero offset ------------------------------------------
    {
        AABB box{{-0.5f, 0.0f, -0.5f}, {0.5f, 1.0f, 0.5f}};
        CHECK(ApproxEq(DropOffsetY(box, {}), 0.0f));
    }

    // --- stacks on a lower box whose footprint overlaps ------------------------
    {
        AABB box{{-0.5f, 5.0f, -0.5f}, {0.5f, 6.0f, 0.5f}};
        AABB table{{-2.0f, 0.0f, -2.0f}, {2.0f, 1.5f, 2.0f}};
        CHECK(ApproxEq(DropOffsetY(box, {table}), -3.5f)); // bottom 5.0 -> 1.5
    }

    // --- taller neighbor overlapping in XZ is NOT a support --------------------
    {
        AABB box{{-0.5f, 2.0f, -0.5f}, {0.5f, 3.0f, 0.5f}};
        AABB wall{{0.3f, 0.0f, -1.0f}, {1.5f, 8.0f, 1.0f}}; // overlaps box in XZ, much taller
        CHECK(ApproxEq(DropOffsetY(box, {wall}), -2.0f)); // ignores the wall, lands on ground
    }

    // --- non-overlapping XZ footprint is ignored --------------------------------
    {
        AABB box{{-0.5f, 2.0f, -0.5f}, {0.5f, 3.0f, 0.5f}};
        AABB far{{5.0f, 0.0f, 5.0f}, {7.0f, 1.0f, 7.0f}};
        CHECK(ApproxEq(DropOffsetY(box, {far}), -2.0f));
    }

    // --- picks the HIGHEST valid support among several ---------------------------
    {
        AABB box{{-0.5f, 5.0f, -0.5f}, {0.5f, 6.0f, 0.5f}};
        AABB low{{-1.0f, 0.0f, -1.0f}, {1.0f, 0.5f, 1.0f}};
        AABB high{{-1.0f, 0.0f, -1.0f}, {1.0f, 2.0f, 1.0f}};
        CHECK(ApproxEq(DropOffsetY(box, {low, high}), -3.0f)); // rests on top of `high`
    }

    // --- invalid AABBs in others are skipped -------------------------------------
    {
        AABB box{{-0.5f, 1.0f, -0.5f}, {0.5f, 2.0f, 0.5f}};
        CHECK(ApproxEq(DropOffsetY(box, {AABB{}}), -1.0f));
    }
}

} // namespace forge::test

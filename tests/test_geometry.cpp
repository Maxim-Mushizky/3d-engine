#include "test_framework.h"

#include "forge/core/Geometry.h"

namespace forge::test {

// Ray fired down -Z from the origin should hit a unit box sitting ahead of it,
// and report a positive entry distance roughly equal to the gap to the near face.
void TestRayHitsAABB()
{
    Ray ray;
    ray.origin = {0.0f, 0.0f, 0.0f};
    ray.direction = {0.0f, 0.0f, -1.0f};

    AABB box;
    box.min = {-1.0f, -1.0f, -6.0f};
    box.max = {1.0f, 1.0f, -4.0f};

    float t = 0.0f;
    CHECK(RayIntersectsAABB(ray, box, t));
    CHECK(t > 0.0f);
    CHECK(ApproxEq(t, 4.0f)); // distance to the near (-4) face
}

// The same ray pointed away from the box must miss.
void TestRayMissesAABB()
{
    Ray ray;
    ray.origin = {0.0f, 0.0f, 0.0f};
    ray.direction = {0.0f, 0.0f, 1.0f}; // +Z, box is at -Z

    AABB box;
    box.min = {-1.0f, -1.0f, -6.0f};
    box.max = {1.0f, 1.0f, -4.0f};

    float t = 0.0f;
    CHECK(!RayIntersectsAABB(ray, box, t));
}

// Möller–Trumbore: a ray through the centroid of a triangle in the z=-2 plane hits.
void TestRayHitsTriangle()
{
    Ray ray;
    ray.origin = {0.25f, 0.25f, 0.0f};
    ray.direction = {0.0f, 0.0f, -1.0f};

    vec3 v0{0.0f, 0.0f, -2.0f};
    vec3 v1{1.0f, 0.0f, -2.0f};
    vec3 v2{0.0f, 1.0f, -2.0f};

    float t = 0.0f;
    CHECK(RayIntersectsTriangle(ray, v0, v1, v2, t));
    CHECK(ApproxEq(t, 2.0f));
}

void RunGeometryTests()
{
    TestRayHitsAABB();
    TestRayMissesAABB();
    TestRayHitsTriangle();
}

} // namespace forge::test

#include "test_framework.h"

#include <forge/core/Geometry.h>
#include <forge/raytrace/BVH.h>

#include <vector>

namespace forge::test {

namespace {

BVHTriangle MakeTri(const vec3& a, const vec3& b, const vec3& c, int material = 0)
{
    BVHTriangle t;
    t.v0 = a;
    t.v1 = b;
    t.v2 = c;
    t.n0 = t.n1 = t.n2 = vec3(0, 1, 0);
    t.material = material;
    t.centroid = (a + b + c) / 3.0f;
    return t;
}

// Deterministic LCG — the suite must not depend on platform rand().
struct Lcg {
    uint32_t state = 12345u;
    float Next() // [0, 1)
    {
        state = state * 1664525u + 1013904223u;
        return (float)(state >> 8) / 16777216.0f;
    }
};

// A scene shaped like the path tracer's worst case: a huge ground quad plus a
// dense cluster of small triangles.
std::vector<BVHTriangle> MakeScene()
{
    std::vector<BVHTriangle> tris;
    const float kExtent = 300.0f;
    tris.push_back(MakeTri({-kExtent, 0, -kExtent}, {kExtent, 0, kExtent}, {kExtent, 0, -kExtent}));
    tris.push_back(MakeTri({-kExtent, 0, -kExtent}, {-kExtent, 0, kExtent}, {kExtent, 0, kExtent}));
    Lcg rng;
    for (int i = 0; i < 200; ++i) {
        vec3 base(rng.Next() * 4.0f - 2.0f, rng.Next() * 2.0f + 0.2f, rng.Next() * 4.0f - 2.0f);
        vec3 e1(rng.Next() * 0.4f - 0.2f, rng.Next() * 0.4f - 0.2f, rng.Next() * 0.4f - 0.2f);
        vec3 e2(rng.Next() * 0.4f - 0.2f, rng.Next() * 0.4f - 0.2f, rng.Next() * 0.4f - 0.2f);
        tris.push_back(MakeTri(base, base + e1, base + e2));
    }
    return tris;
}

// Structural invariants: every interior node's children are adjacent and in
// range, every triangle lands in exactly one leaf, leaf bounds contain their
// triangles, child bounds nest inside the parent.
void CheckTree(const BVH& bvh, const std::vector<BVHTriangle>& tris)
{
    const auto& nodes = bvh.Nodes();
    CHECK(!nodes.empty());
    std::vector<int> covered(tris.size(), 0);
    for (size_t ni = 0; ni < nodes.size(); ++ni) {
        const BVHNode& n = nodes[ni];
        CHECK(n.min.x <= n.max.x && n.min.y <= n.max.y && n.min.z <= n.max.z);
        if (n.count > 0) { // leaf
            CHECK(n.leftFirst >= 0 && n.leftFirst + n.count <= (int)tris.size());
            for (int i = n.leftFirst; i < n.leftFirst + n.count; ++i) {
                ++covered[i];
                const BVHTriangle& t = tris[i];
                for (const vec3& v : {t.v0, t.v1, t.v2}) {
                    CHECK(v.x >= n.min.x - 1e-4f && v.x <= n.max.x + 1e-4f);
                    CHECK(v.y >= n.min.y - 1e-4f && v.y <= n.max.y + 1e-4f);
                    CHECK(v.z >= n.min.z - 1e-4f && v.z <= n.max.z + 1e-4f);
                }
            }
        } else { // interior: children at leftFirst, leftFirst+1
            CHECK(n.leftFirst > (int)ni && n.leftFirst + 1 < (int)nodes.size());
            for (int c = 0; c < 2; ++c) {
                const BVHNode& child = nodes[n.leftFirst + c];
                CHECK(child.min.x >= n.min.x - 1e-4f && child.max.x <= n.max.x + 1e-4f);
                CHECK(child.min.y >= n.min.y - 1e-4f && child.max.y <= n.max.y + 1e-4f);
                CHECK(child.min.z >= n.min.z - 1e-4f && child.max.z <= n.max.z + 1e-4f);
            }
        }
    }
    for (size_t i = 0; i < covered.size(); ++i)
        CHECK(covered[i] == 1);
}

// Closest hit through the node array, mirroring the GPU traversal semantics.
float TraceBVH(const BVH& bvh, const std::vector<BVHTriangle>& tris, const Ray& ray)
{
    const auto& nodes = bvh.Nodes();
    float best = 1e30f;
    if (nodes.empty())
        return best;
    int stack[64];
    int sp = 0;
    stack[sp++] = 0;
    while (sp > 0) {
        const BVHNode& n = nodes[stack[--sp]];
        float tBox;
        if (!RayIntersectsAABB(ray, AABB{n.min, n.max}, tBox) || tBox >= best)
            continue;
        if (n.count > 0) {
            for (int i = n.leftFirst; i < n.leftFirst + n.count; ++i) {
                float t;
                if (RayIntersectsTriangle(ray, tris[i].v0, tris[i].v1, tris[i].v2, t))
                    best = std::min(best, t);
            }
        } else {
            stack[sp++] = n.leftFirst;
            stack[sp++] = n.leftFirst + 1;
        }
    }
    return best;
}

float TraceBrute(const std::vector<BVHTriangle>& tris, const Ray& ray)
{
    float best = 1e30f;
    for (const BVHTriangle& tri : tris) {
        float t;
        if (RayIntersectsTriangle(ray, tri.v0, tri.v1, tri.v2, t))
            best = std::min(best, t);
    }
    return best;
}

} // namespace

void RunBvhTests()
{
    // --- empty input builds an empty tree --------------------------------------
    {
        std::vector<BVHTriangle> tris;
        BVH bvh;
        bvh.Build(tris);
        CHECK(bvh.Nodes().empty());
    }

    // --- single triangle: one leaf holding it ----------------------------------
    {
        std::vector<BVHTriangle> tris{MakeTri({0, 0, 0}, {1, 0, 0}, {0, 1, 0})};
        BVH bvh;
        bvh.Build(tris);
        CHECK(bvh.Nodes().size() == 1);
        CHECK(bvh.Nodes()[0].count == 1);
        CheckTree(bvh, tris);
    }

    // --- coincident centroids: oversized leaf instead of infinite recursion -----
    {
        std::vector<BVHTriangle> tris;
        for (int i = 0; i < 10; ++i)
            tris.push_back(MakeTri({-1, 0, -1}, {1, 0, -1}, {0, 0, 2})); // identical
        BVH bvh;
        bvh.Build(tris);
        CheckTree(bvh, tris);
    }

    // --- ground quad + cluster: tree is valid and traversal matches brute force --
    {
        std::vector<BVHTriangle> tris = MakeScene();
        BVH bvh;
        bvh.Build(tris);
        CheckTree(bvh, tris);

        Lcg rng{777u};
        for (int i = 0; i < 100; ++i) {
            Ray ray;
            ray.origin = vec3(rng.Next() * 8.0f - 4.0f, rng.Next() * 4.0f + 0.5f, rng.Next() * 8.0f - 4.0f);
            vec3 target(rng.Next() * 4.0f - 2.0f, rng.Next() * 2.0f, rng.Next() * 4.0f - 2.0f);
            ray.direction = glm::normalize(target - ray.origin);
            CHECK(ApproxEq(TraceBVH(bvh, tris, ray), TraceBrute(tris, ray), 1e-3f));
        }
    }
}

} // namespace forge::test

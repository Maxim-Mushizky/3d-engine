#include "BVH.h"

#include <algorithm>
#include <cfloat>

namespace forge {

static constexpr int kLeafSize = 4;
static constexpr int kBins = 12;

static float SurfaceArea(const AABB& b)
{
    if (!b.Valid())
        return 0.0f;
    vec3 e = b.max - b.min;
    return 2.0f * (e.x * e.y + e.y * e.z + e.z * e.x);
}

static void ExpandTri(AABB& box, const BVHTriangle& t)
{
    box.Expand(t.v0);
    box.Expand(t.v1);
    box.Expand(t.v2);
}

void BVH::Build(std::vector<BVHTriangle>& tris)
{
    m_Nodes.clear();
    if (tris.empty())
        return;
    m_Nodes.reserve(tris.size() * 2);
    m_Nodes.emplace_back();
    FillNode(0, tris, 0, (int)tris.size());
}

void BVH::FillNode(size_t nodeIndex, std::vector<BVHTriangle>& tris, int first, int count)
{
    AABB bounds, centroidBounds;
    for (int i = first; i < first + count; ++i) {
        ExpandTri(bounds, tris[i]);
        centroidBounds.Expand(tris[i].centroid);
    }

    if (count <= kLeafSize) {
        m_Nodes[nodeIndex] = BVHNode{bounds.min, first, bounds.max, count};
        return;
    }

    // Binned SAH: bin centroids on each axis, sweep prefix/suffix bounds for the
    // partition minimizing SA_left * N_left + SA_right * N_right. Median split
    // (the previous strategy) makes long overlapping siblings — costly to traverse
    // when a huge ground quad shares a node with dense clustered geometry.
    vec3 extent = centroidBounds.max - centroidBounds.min;
    int bestAxis = -1, bestBin = -1;
    float bestCost = FLT_MAX;
    for (int axis = 0; axis < 3; ++axis) {
        if (extent[axis] < 1e-8f)
            continue;
        AABB binBox[kBins];
        int binN[kBins] = {};
        float scale = (float)kBins / extent[axis];
        for (int i = first; i < first + count; ++i) {
            int b = std::min(kBins - 1, (int)((tris[i].centroid[axis] - centroidBounds.min[axis]) * scale));
            ++binN[b];
            ExpandTri(binBox[b], tris[i]);
        }

        float rightArea[kBins];
        int rightN[kBins];
        AABB acc;
        int n = 0;
        for (int b = kBins - 1; b > 0; --b) {
            if (binBox[b].Valid()) {
                acc.Expand(binBox[b].min);
                acc.Expand(binBox[b].max);
            }
            n += binN[b];
            rightArea[b] = SurfaceArea(acc);
            rightN[b] = n;
        }

        AABB leftAcc;
        int leftN = 0;
        for (int s = 0; s < kBins - 1; ++s) { // split = bins [0..s] | [s+1..]
            if (binBox[s].Valid()) {
                leftAcc.Expand(binBox[s].min);
                leftAcc.Expand(binBox[s].max);
            }
            leftN += binN[s];
            if (leftN == 0 || rightN[s + 1] == 0)
                continue;
            float cost = SurfaceArea(leftAcc) * (float)leftN + rightArea[s + 1] * (float)rightN[s + 1];
            if (cost < bestCost) {
                bestCost = cost;
                bestAxis = axis;
                bestBin = s;
            }
        }
    }

    // No usable split (all centroids coincide) — emit an oversized leaf.
    if (bestAxis < 0) {
        m_Nodes[nodeIndex] = BVHNode{bounds.min, first, bounds.max, count};
        return;
    }

    float scale = (float)kBins / extent[bestAxis];
    float axisMin = centroidBounds.min[bestAxis];
    auto mid = std::partition(tris.begin() + first, tris.begin() + first + count, [&](const BVHTriangle& t) {
        int b = std::min(kBins - 1, (int)((t.centroid[bestAxis] - axisMin) * scale));
        return b <= bestBin;
    });
    int half = (int)(mid - (tris.begin() + first));
    // The sweep only scored splits with triangles on both sides, so a one-sided
    // partition means float drift — fall back to a median split on the same axis.
    if (half == 0 || half == count) {
        half = count / 2;
        std::nth_element(tris.begin() + first, tris.begin() + first + half, tris.begin() + first + count,
                         [axis = bestAxis](const BVHTriangle& a, const BVHTriangle& b) {
                             return a.centroid[axis] < b.centroid[axis];
                         });
    }

    // Children are allocated adjacently so the GPU can address them as left, left+1.
    int left = (int)m_Nodes.size();
    m_Nodes.emplace_back();
    m_Nodes.emplace_back();
    m_Nodes[nodeIndex] = BVHNode{bounds.min, left, bounds.max, 0};

    FillNode((size_t)left, tris, first, half);
    FillNode((size_t)left + 1, tris, first + half, count - half);
}

} // namespace forge

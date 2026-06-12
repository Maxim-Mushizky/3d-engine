#include "MeshRemesh.h"

#include "MeshEdit.h"
#include "forge/core/Log.h"

#include <mc_tables.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <unordered_map>
#include <vector>

namespace forge {

namespace {

// Closest distance from point to triangle (Ericson, Real-Time Collision Detection).
float PointTriDistance(const vec3& p, const vec3& a, const vec3& b, const vec3& c)
{
    vec3 ab = b - a, ac = c - a, ap = p - a;
    float d1 = glm::dot(ab, ap), d2 = glm::dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f)
        return glm::length(ap);

    vec3 bp = p - b;
    float d3 = glm::dot(ab, bp), d4 = glm::dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3)
        return glm::length(bp);

    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
        return glm::length(ap - ab * (d1 / (d1 - d3)));

    vec3 cp = p - c;
    float d5 = glm::dot(ab, cp), d6 = glm::dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6)
        return glm::length(cp);

    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
        return glm::length(ap - ac * (d2 / (d2 - d6)));

    float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return glm::length(p - (b + (c - b) * w));
    }

    float denom = 1.0f / (va + vb + vc);
    return glm::length(p - (a + ab * (vb * denom) + ac * (vc * denom)));
}

} // namespace

std::shared_ptr<Mesh> VoxelRemesh(const Mesh& mesh, int resolution)
{
    const auto& verts = mesh.Vertices();
    const auto& idx = mesh.Indices();
    if (idx.size() < 3)
        return nullptr;

    resolution = std::clamp(resolution, 16, 200);
    vec3 bmin = mesh.Bounds().min, bmax = mesh.Bounds().max;
    vec3 extent = bmax - bmin;
    float maxExtent = std::max({extent.x, extent.y, extent.z});
    if (maxExtent < 1e-6f)
        return nullptr;
    const float cell = maxExtent / (float)resolution;
    const int pad = 3;
    const vec3 origin = bmin - vec3((float)pad * cell);
    const int ni = (int)std::ceil(extent.x / cell) + 2 * pad + 1;
    const int nj = (int)std::ceil(extent.y / cell) + 2 * pad + 1;
    const int nk = (int)std::ceil(extent.z / cell) + 2 * pad + 1;
    auto nodeIndex = [&](int i, int j, int k) { return (size_t)(k * nj + j) * ni + i; };
    auto nodePos = [&](int i, int j, int k) {
        return origin + vec3((float)i, (float)j, (float)k) * cell;
    };

    // --- narrow-band unsigned distance (±2 cells around each triangle) -------
    const float band = 2.0f * cell;
    std::vector<float> dist((size_t)ni * nj * nk, FLT_MAX);
    for (size_t t = 0; t + 2 < idx.size(); t += 3) {
        const vec3& a = verts[idx[t]].position;
        const vec3& b = verts[idx[t + 1]].position;
        const vec3& c = verts[idx[t + 2]].position;
        vec3 tmin = glm::min(a, glm::min(b, c)) - vec3(band);
        vec3 tmax = glm::max(a, glm::max(b, c)) + vec3(band);
        int i0 = std::max((int)std::floor((tmin.x - origin.x) / cell), 0);
        int j0 = std::max((int)std::floor((tmin.y - origin.y) / cell), 0);
        int k0 = std::max((int)std::floor((tmin.z - origin.z) / cell), 0);
        int i1 = std::min((int)std::ceil((tmax.x - origin.x) / cell), ni - 1);
        int j1 = std::min((int)std::ceil((tmax.y - origin.y) / cell), nj - 1);
        int k1 = std::min((int)std::ceil((tmax.z - origin.z) / cell), nk - 1);
        for (int k = k0; k <= k1; ++k)
            for (int j = j0; j <= j1; ++j)
                for (int i = i0; i <= i1; ++i) {
                    float& d = dist[nodeIndex(i, j, k)];
                    d = std::min(d, PointTriDistance(nodePos(i, j, k), a, b, c));
                }
    }

    // --- sign by z-column crossing parity ------------------------------------
    // For every (i,j) node column collect the z values where the mesh crosses
    // it; nodes below an odd number of crossings are inside. Exact for closed
    // input, and avoids both winding numbers and band-aware flood fill.
    std::vector<std::vector<float>> crossings((size_t)ni * nj);
    for (size_t t = 0; t + 2 < idx.size(); t += 3) {
        const vec3& a = verts[idx[t]].position;
        const vec3& b = verts[idx[t + 1]].position;
        const vec3& c = verts[idx[t + 2]].position;
        vec2 a2{a.x, a.y}, b2{b.x, b.y}, c2{c.x, c.y};
        float area = (b2.x - a2.x) * (c2.y - a2.y) - (c2.x - a2.x) * (b2.y - a2.y);
        if (std::abs(area) < 1e-12f)
            continue; // edge-on triangle: contributes no parity
        vec2 tmin = glm::min(a2, glm::min(b2, c2)), tmax = glm::max(a2, glm::max(b2, c2));
        int i0 = std::max((int)std::ceil((tmin.x - origin.x) / cell), 0);
        int j0 = std::max((int)std::ceil((tmin.y - origin.y) / cell), 0);
        int i1 = std::min((int)std::floor((tmax.x - origin.x) / cell), ni - 1);
        int j1 = std::min((int)std::floor((tmax.y - origin.y) / cell), nj - 1);
        for (int j = j0; j <= j1; ++j) {
            for (int i = i0; i <= i1; ++i) {
                // Tiny irrational offset: sample columns never hit edges/verts exactly.
                vec2 p{origin.x + (float)i * cell + 1.2345e-4f * cell,
                       origin.y + (float)j * cell + 2.3457e-4f * cell};
                float w0 = ((b2.x - p.x) * (c2.y - p.y) - (c2.x - p.x) * (b2.y - p.y)) / area;
                float w1 = ((c2.x - p.x) * (a2.y - p.y) - (a2.x - p.x) * (c2.y - p.y)) / area;
                float w2 = 1.0f - w0 - w1;
                if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f)
                    continue;
                crossings[(size_t)j * ni + i].push_back(w0 * a.z + w1 * b.z + w2 * c.z);
            }
        }
    }

    std::vector<float> field((size_t)ni * nj * nk);
    for (int j = 0; j < nj; ++j) {
        for (int i = 0; i < ni; ++i) {
            auto& cs = crossings[(size_t)j * ni + i];
            std::sort(cs.begin(), cs.end());
            size_t next = 0;
            bool inside = false;
            for (int k = 0; k < nk; ++k) {
                float z = origin.z + (float)k * cell;
                while (next < cs.size() && cs[next] < z) {
                    inside = !inside;
                    ++next;
                }
                float d = dist[nodeIndex(i, j, k)];
                if (d == FLT_MAX)
                    d = 2.0f * band; // far node: only the sign matters
                field[nodeIndex(i, j, k)] = inside ? -d : d;
            }
        }
    }

    // --- marching cubes -------------------------------------------------------
    // Bourke layout: corners 0-3 on the k slab (i,j CCW), 4-7 on k+1.
    static const int cornerOff[8][3] = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
                                        {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}};
    static const int edgeCorner[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                                          {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};

    std::vector<Vertex> outVerts;
    std::vector<uint32_t> outIdx;
    std::unordered_map<uint64_t, uint32_t> edgeVert; // (nodeMin<<32|nodeMax) -> out index

    for (int k = 0; k + 1 < nk; ++k) {
        for (int j = 0; j + 1 < nj; ++j) {
            for (int i = 0; i + 1 < ni; ++i) {
                float val[8];
                size_t node[8];
                int cubeIndex = 0;
                for (int c = 0; c < 8; ++c) {
                    int ci = i + cornerOff[c][0], cj = j + cornerOff[c][1], ck = k + cornerOff[c][2];
                    node[c] = nodeIndex(ci, cj, ck);
                    val[c] = field[node[c]];
                    if (val[c] < 0.0f)
                        cubeIndex |= 1 << c;
                }
                int edges = mc::kEdgeTable[cubeIndex];
                if (edges == 0)
                    continue;

                uint32_t ev[12];
                for (int e = 0; e < 12; ++e) {
                    if (!(edges & (1 << e)))
                        continue;
                    int c0 = edgeCorner[e][0], c1 = edgeCorner[e][1];
                    uint64_t key = node[c0] < node[c1]
                                       ? ((uint64_t)node[c0] << 32) | node[c1]
                                       : ((uint64_t)node[c1] << 32) | node[c0];
                    auto [it, inserted] = edgeVert.try_emplace(key, (uint32_t)outVerts.size());
                    if (inserted) {
                        vec3 p0 = nodePos(i + cornerOff[c0][0], j + cornerOff[c0][1], k + cornerOff[c0][2]);
                        vec3 p1 = nodePos(i + cornerOff[c1][0], j + cornerOff[c1][1], k + cornerOff[c1][2]);
                        float t = val[c0] / (val[c0] - val[c1]); // both signs differ, denom != 0
                        outVerts.push_back({glm::mix(p0, p1, t), vec3(0, 1, 0), vec2(0)});
                    }
                    ev[e] = it->second;
                }
                for (int t = 0; mc::kTriTable[cubeIndex][t] != -1; t += 3) {
                    uint32_t v0 = ev[mc::kTriTable[cubeIndex][t]];
                    uint32_t v1 = ev[mc::kTriTable[cubeIndex][t + 1]];
                    uint32_t v2 = ev[mc::kTriTable[cubeIndex][t + 2]];
                    if (v0 == v1 || v1 == v2 || v2 == v0)
                        continue;
                    outIdx.insert(outIdx.end(), {v0, v1, v2});
                }
            }
        }
    }
    if (outIdx.empty())
        return nullptr;

    // --- Taubin smoothing (10x lambda 0.5 / mu -0.53): mandatory, else the
    // voxel staircase makes the result look worse than the input ---------------
    {
        std::vector<std::vector<uint32_t>> nbr(outVerts.size());
        for (size_t t = 0; t + 2 < outIdx.size(); t += 3) {
            for (int c = 0; c < 3; ++c) {
                uint32_t a = outIdx[t + c], b = outIdx[t + (c + 1) % 3];
                nbr[a].push_back(b);
                nbr[b].push_back(a);
            }
        }
        for (auto& n : nbr) {
            std::sort(n.begin(), n.end());
            n.erase(std::unique(n.begin(), n.end()), n.end());
        }
        std::vector<vec3> pos(outVerts.size()), next(outVerts.size());
        for (size_t v = 0; v < outVerts.size(); ++v)
            pos[v] = outVerts[v].position;
        auto pass = [&](float factor) {
            for (size_t v = 0; v < pos.size(); ++v) {
                if (nbr[v].empty()) {
                    next[v] = pos[v];
                    continue;
                }
                vec3 avg(0.0f);
                for (uint32_t n : nbr[v])
                    avg += pos[n];
                avg /= (float)nbr[v].size();
                next[v] = pos[v] + (avg - pos[v]) * factor;
            }
            pos.swap(next);
        };
        for (int it = 0; it < 10; ++it) {
            pass(0.5f);   // shrink
            pass(-0.53f); // re-inflate: net effect smooths without volume loss
        }
        for (size_t v = 0; v < outVerts.size(); ++v)
            outVerts[v].position = pos[v];
    }

    auto result = std::make_shared<Mesh>(std::move(outVerts), std::move(outIdx));
    MeshTopology topo = MeshTopology::Build(*result);
    RecomputeNormalsWelded(*result, topo);
    result->UploadVertices();
    FORGE_INFO("VoxelRemesh: %zu -> %zu tris (res %d)", idx.size() / 3,
               result->Indices().size() / 3, resolution);
    return result;
}

} // namespace forge

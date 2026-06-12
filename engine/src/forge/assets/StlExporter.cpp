#include "StlExporter.h"

#include "forge/core/Log.h"

#include <cmath>
#include <cstring>
#include <fstream>
#include <unordered_map>

namespace forge {

namespace {

struct ExportTri {
    vec3 v[3];
};

// Position weld for the watertight check: triangles index shared vertex slots
// only after co-located corners collapse to one id, otherwise every UV seam
// reads as a hole.
struct PosKey {
    int64_t x, y, z;
    bool operator==(const PosKey& o) const { return x == o.x && y == o.y && z == o.z; }
};
struct PosKeyHash {
    size_t operator()(const PosKey& k) const
    {
        return (size_t)(k.x * 73856093ll ^ k.y * 19349663ll ^ k.z * 83492791ll);
    }
};
PosKey QuantizeMm(const vec3& p)
{
    constexpr float kScale = 1e3f; // 1 µm cells on mm coordinates
    return {(int64_t)std::llround(p.x * kScale), (int64_t)std::llround(p.y * kScale),
            (int64_t)std::llround(p.z * kScale)};
}

void CheckWatertight(const std::vector<ExportTri>& tris, StlExportResult& result)
{
    std::unordered_map<PosKey, uint32_t, PosKeyHash> weld;
    weld.reserve(tris.size() * 3);
    uint32_t nextId = 0;
    auto vertId = [&](const vec3& p) {
        auto [it, inserted] = weld.try_emplace(QuantizeMm(p), nextId);
        if (inserted)
            ++nextId;
        return it->second;
    };

    // Per undirected edge: total adjacency count + how many ran a->b with a<b.
    struct EdgeInfo {
        uint32_t count = 0;
        uint32_t forward = 0;
    };
    std::unordered_map<uint64_t, EdgeInfo> edges;
    edges.reserve(tris.size() * 3);
    for (const ExportTri& t : tris) {
        uint32_t id[3] = {vertId(t.v[0]), vertId(t.v[1]), vertId(t.v[2])};
        if (id[0] == id[1] || id[1] == id[2] || id[2] == id[0])
            continue; // degenerate after weld
        for (int e = 0; e < 3; ++e) {
            uint32_t a = id[e], b = id[(e + 1) % 3];
            uint64_t key = a < b ? ((uint64_t)a << 32 | b) : ((uint64_t)b << 32 | a);
            EdgeInfo& info = edges[key];
            ++info.count;
            if (a < b)
                ++info.forward;
        }
    }

    for (const auto& [key, info] : edges) {
        if (info.count != 2)
            ++result.openEdges; // 1 = hole boundary, >2 = fin
        else if (info.forward != 1)
            ++result.flippedEdges; // both tris wind the same way -> one faces inward
    }
    result.watertight = result.openEdges == 0 && result.flippedEdges == 0;
}

} // namespace

StlExportResult ExportStl(const Scene& scene, const std::vector<UUID>& entities,
                          const std::string& path, float millimetersPerUnit)
{
    StlExportResult result;

    std::vector<ExportTri> tris;
    for (UUID id : entities) {
        const Entity* e = scene.Find(id);
        if (!e || !e->mesh || e->light.enabled)
            continue;

        mat4 world = scene.WorldTransform(e->id);
        bool flip = glm::determinant(mat3(world)) < 0.0f; // mirror-scaled: fix winding

        const auto& verts = e->mesh->Vertices();
        const auto& idx = e->mesh->Indices();
        for (size_t i = 0; i + 2 < idx.size(); i += 3) {
            vec3 p[3];
            for (int c = 0; c < 3; ++c) {
                vec3 w = vec3(world * vec4(verts[idx[i + c]].position, 1.0f));
                // Y-up scene -> Z-up STL: proper rotation, winding preserved.
                p[c] = vec3(w.x, -w.z, w.y) * millimetersPerUnit;
            }
            if (flip)
                std::swap(p[1], p[2]);
            // Slicers choke on zero-area facets; they carry no surface anyway.
            if (glm::length(glm::cross(p[1] - p[0], p[2] - p[0])) < 1e-12f)
                continue;
            tris.push_back({{p[0], p[1], p[2]}});
        }
    }

    if (tris.empty()) {
        result.error = "Nothing to export - no triangles in the selection";
        return result;
    }

    CheckWatertight(tris, result);

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        result.error = "Could not open file for writing: " + path;
        return result;
    }

    // 80-byte header. Must NOT start with "solid" or readers parse it as ASCII STL.
    char header[80] = {0};
    std::snprintf(header, sizeof(header), "Forge binary STL (Z-up, millimeters)");
    out.write(header, sizeof(header));
    uint32_t count = (uint32_t)tris.size();
    out.write((const char*)&count, sizeof(count));

    // 50 bytes per facet, written field-by-field: the packed layout (12+36+2)
    // doesn't match the struct's natural alignment.
    for (const ExportTri& t : tris) {
        vec3 n = glm::cross(t.v[1] - t.v[0], t.v[2] - t.v[0]);
        float len = glm::length(n);
        n = len > 1e-12f ? n / len : vec3(0.0f);
        out.write((const char*)&n.x, 12);
        for (int c = 0; c < 3; ++c)
            out.write((const char*)&t.v[c].x, 12);
        uint16_t attr = 0;
        out.write((const char*)&attr, sizeof(attr));
    }
    out.close();
    if (!out) {
        result.error = "Write failed (disk full or file locked): " + path;
        return result;
    }

    result.ok = true;
    result.triangles = count;
    FORGE_INFO("STL export: %u triangles -> %s (%s)", count, path.c_str(),
               result.watertight ? "watertight" : "NOT watertight");
    return result;
}

} // namespace forge

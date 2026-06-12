#include "MeshBoolean.h"

#include "forge/core/Log.h"

#include <manifold/manifold.h>

namespace forge {

namespace {

manifold::Manifold ToManifold(const Mesh& mesh, const mat4& world, std::string& error)
{
    manifold::MeshGL m;
    m.numProp = 3;

    const auto& verts = mesh.Vertices();
    const auto& idx = mesh.Indices();
    m.vertProperties.reserve(verts.size() * 3);
    for (const Vertex& v : verts) {
        vec3 p = vec3(world * vec4(v.position, 1.0f));
        m.vertProperties.push_back(p.x);
        m.vertProperties.push_back(p.y);
        m.vertProperties.push_back(p.z);
    }

    // Mirror-scaled transforms flip orientation; Manifold requires CCW-outward.
    bool flip = glm::determinant(mat3(world)) < 0.0f;
    m.triVerts.reserve(idx.size());
    for (size_t i = 0; i + 2 < idx.size(); i += 3) {
        m.triVerts.push_back(idx[i]);
        m.triVerts.push_back(idx[i + (flip ? 2 : 1)]);
        m.triVerts.push_back(idx[i + (flip ? 1 : 2)]);
    }

    // CRITICAL: our meshes duplicate vertices along UV seams, so the raw index
    // buffer is topologically open. Merge() welds co-located vertices into a
    // closed manifold — without it every primitive fails as NotManifold.
    m.Merge();

    manifold::Manifold result(m);
    if (result.Status() != manifold::Manifold::Error::NoError)
        error = "Mesh is not closed - booleans need watertight meshes";
    return result;
}

} // namespace

BooleanResult MeshBoolean(const Mesh& a, const mat4& worldA, const Mesh& b, const mat4& worldB,
                          BooleanOp op)
{
    BooleanResult out;

    std::string err;
    manifold::Manifold ma = ToManifold(a, worldA, err);
    if (!err.empty()) {
        out.error = err + " (first object)";
        return out;
    }
    manifold::Manifold mb = ToManifold(b, worldB, err);
    if (!err.empty()) {
        out.error = err + " (second object)";
        return out;
    }

    manifold::Manifold r = op == BooleanOp::Union      ? ma + mb
                           : op == BooleanOp::Subtract ? ma - mb
                                                       : ma ^ mb;
    if (r.IsEmpty()) {
        out.error = "Result is empty - the shapes don't overlap that way";
        return out;
    }

    // Normals from Manifold with a 30-degree sharp-edge split: cube edges stay
    // crisp, sphere surfaces stay smooth. GetMeshGL(normalIdx) duplicates
    // vertices where normals differ, which is exactly our renderer's layout.
    r = r.CalculateNormals(3, 30.0);
    manifold::MeshGL g = r.GetMeshGL(3);

    std::vector<Vertex> verts(g.NumVert());
    for (size_t i = 0; i < verts.size(); ++i) {
        const float* p = &g.vertProperties[i * g.numProp];
        verts[i].position = {p[0], p[1], p[2]};
        verts[i].normal = {p[3], p[4], p[5]};
        verts[i].uv = {0.0f, 0.0f};
    }
    std::vector<uint32_t> idx(g.triVerts.begin(), g.triVerts.end());

    out.mesh = std::make_shared<Mesh>(std::move(verts), std::move(idx));
    FORGE_INFO("Boolean: %zu + %zu -> %zu tris", a.Indices().size() / 3, b.Indices().size() / 3,
               out.mesh->Indices().size() / 3);
    return out;
}

} // namespace forge

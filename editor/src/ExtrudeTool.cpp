#include "ExtrudeTool.h"
#include "EditorCamera.h"

#include <forge/core/Log.h>

#include <imgui.h>

#include <cmath>
#include <unordered_map>

namespace forge {

bool ExtrudeTool::BeginDrag(Scene& scene, const RaycastHit& hit)
{
    Entity* e = scene.Find(hit.entity);
    if (!e || !e->mesh)
        return false;

    const Mesh& mesh = *e->mesh;
    const auto& verts = mesh.Vertices();
    const auto& idx = mesh.Indices();
    size_t triCount = idx.size() / 3;
    uint32_t seed = hit.triIndex / 3;
    if (seed >= triCount)
        return false;

    MeshTopology topo = MeshTopology::Build(mesh);
    MeshEdgeMap edges = MeshEdgeMap::Build(mesh, topo);

    auto faceNormal = [&](uint32_t t) {
        const vec3& p0 = verts[idx[t * 3]].position;
        return glm::cross(verts[idx[t * 3 + 1]].position - p0, verts[idx[t * 3 + 2]].position - p0);
    };
    auto centroid = [&](uint32_t t) {
        return (verts[idx[t * 3]].position + verts[idx[t * 3 + 1]].position +
                verts[idx[t * 3 + 2]].position) / 3.0f;
    };

    vec3 n0 = faceNormal(seed);
    float n0len = glm::length(n0);
    if (n0len < 1e-12f)
        return false; // degenerate seed
    n0 /= n0len;
    float d0 = glm::dot(n0, centroid(seed));
    float planeEps = 1e-3f * glm::length(mesh.Bounds().max - mesh.Bounds().min);
    const float cosTol = std::cos(glm::radians(2.0f));

    // Coplanar flood fill across group-space edges; both tests vs the SEED's
    // plane so acceptance can't drift around a smooth bend.
    std::vector<uint8_t> inRegion(triCount, 0);
    std::vector<uint32_t> stack{seed}, region;
    inRegion[seed] = 1;
    while (!stack.empty()) {
        uint32_t t = stack.back();
        stack.pop_back();
        region.push_back(t);
        for (int c = 0; c < 3; ++c) {
            uint32_t a = idx[t * 3 + c], b = idx[t * 3 + (c + 1) % 3];
            const MeshEdgeMap::Edge* edge = edges.Find(topo.weldGroup[a], topo.weldGroup[b]);
            if (!edge)
                continue;
            if (edge->tris.size() > 2)
                return false; // non-manifold edge: walls would be ambiguous
            for (uint32_t u : edge->tris) {
                if (u == t || inRegion[u])
                    continue;
                vec3 nu = faceNormal(u);
                float nuLen = glm::length(nu);
                if (nuLen < 1e-12f)
                    continue;
                if (glm::dot(nu / nuLen, n0) > cosTol &&
                    std::abs(glm::dot(n0, centroid(u)) - d0) < planeEps) {
                    inRegion[u] = 1;
                    stack.push_back(u);
                }
            }
        }
    }

    // Region edges with exactly one region tri are the boundary; the directed
    // edge order inside its region tri keeps the region on the left.
    std::unordered_map<uint64_t, int> regionEdgeCount;
    auto edgeKey = [&](uint32_t a, uint32_t b) {
        uint32_t ga = topo.weldGroup[a], gb = topo.weldGroup[b];
        if (ga > gb)
            std::swap(ga, gb);
        return ((uint64_t)ga << 32) | gb;
    };
    for (uint32_t t : region)
        for (int c = 0; c < 3; ++c)
            ++regionEdgeCount[edgeKey(idx[t * 3 + c], idx[t * 3 + (c + 1) % 3])];

    // Build the working mesh: cap = duplicated region verts, walls = fresh
    // verts per boundary edge (hard side edges by construction).
    std::vector<Vertex> newVerts = verts;
    std::vector<uint32_t> newIdx = idx;
    m_MovingVerts.clear();
    m_MovingBase.clear();
    std::unordered_map<uint32_t, uint32_t> capOf;
    auto capVert = [&](uint32_t raw) {
        auto [it, inserted] = capOf.try_emplace(raw, (uint32_t)newVerts.size());
        if (inserted) {
            newVerts.push_back(verts[raw]);
            m_MovingVerts.push_back(it->second);
            m_MovingBase.push_back(verts[raw].position);
        }
        return it->second;
    };
    for (uint32_t t : region)
        for (int c = 0; c < 3; ++c)
            newIdx[t * 3 + c] = capVert(idx[t * 3 + c]);

    m_WallIndexStart = newIdx.size();
    for (uint32_t t : region) {
        for (int c = 0; c < 3; ++c) {
            uint32_t a = idx[t * 3 + c], b = idx[t * 3 + (c + 1) % 3];
            if (regionEdgeCount[edgeKey(a, b)] != 1)
                continue; // interior region edge: no wall
            uint32_t bottomA = (uint32_t)newVerts.size();
            newVerts.push_back(verts[a]);
            uint32_t bottomB = (uint32_t)newVerts.size();
            newVerts.push_back(verts[b]);
            uint32_t topA = (uint32_t)newVerts.size();
            newVerts.push_back(verts[a]);
            uint32_t topB = (uint32_t)newVerts.size();
            newVerts.push_back(verts[b]);
            m_MovingVerts.push_back(topA);
            m_MovingBase.push_back(verts[a].position);
            m_MovingVerts.push_back(topB);
            m_MovingBase.push_back(verts[b].position);
            // (bottomA, bottomB, topB) + (bottomA, topB, topA): outward for +n0
            newIdx.insert(newIdx.end(), {bottomA, bottomB, topB, bottomA, topB, topA});
        }
    }

    m_Original = e->mesh;
    e->mesh = std::make_shared<Mesh>(std::move(newVerts), std::move(newIdx));
    m_WorkTopology = MeshTopology::Build(*e->mesh);

    mat4 world = scene.WorldTransform(hit.entity);
    m_Entity = hit.entity;
    m_NormalLocal = n0;
    m_LinePointWorld = hit.worldPos;
    m_LineDirWorld = glm::normalize(mat3(glm::transpose(glm::inverse(world))) * n0);
    m_WorldPerLocal = std::max(glm::length(mat3(world) * n0), 1e-6f);
    m_LastOffsetLocal = 0.0f;
    m_Dragging = true;
    FORGE_INFO("Extrude: region %zu tris, %zu wall edges", region.size(),
               (e->mesh->Indices().size() - m_WallIndexStart) / 6);
    return true;
}

void ExtrudeTool::ApplyOffset(Scene& scene, float localOffset)
{
    Entity* e = scene.Find(m_Entity);
    if (!e || !e->mesh)
        return;
    auto& verts = e->mesh->MutableVertices();
    for (size_t i = 0; i < m_MovingVerts.size(); ++i)
        verts[m_MovingVerts[i]].position = m_MovingBase[i] + m_NormalLocal * localOffset;
    RecomputeNormalsWelded(*e->mesh, m_WorkTopology);
    e->mesh->UploadVertices();
    m_LastOffsetLocal = localOffset;
}

std::unique_ptr<Command> ExtrudeTool::Commit(Scene& scene)
{
    m_Dragging = false;
    m_Armed = false;
    Entity* e = scene.Find(m_Entity);
    if (!e || !e->mesh) {
        m_Original.reset();
        return nullptr;
    }

    float minOffset = 1e-4f * glm::length(m_Original->Bounds().max - m_Original->Bounds().min);
    if (std::abs(m_LastOffsetLocal) < minOffset) {
        e->mesh = m_Original; // effectively a click: restore, no undo entry
        m_Original.reset();
        return nullptr;
    }

    // Rebuild as a fresh mesh so bounds match the moved cap. No winding fix is
    // needed for negative (push-in) offsets: the wall index order is consistent
    // with cap and rim for either sign — the wall-top positions carry the sign,
    // so the geometric orientation flips into the cavity automatically.
    std::vector<Vertex> verts = e->mesh->Vertices();
    std::vector<uint32_t> idx = e->mesh->Indices();
    auto finalMesh = std::make_shared<Mesh>(std::move(verts), std::move(idx));
    MeshTopology topo = MeshTopology::Build(*finalMesh);
    RecomputeNormalsWelded(*finalMesh, topo);
    finalMesh->UploadVertices();

    e->mesh = finalMesh;
    auto cmd = std::make_unique<MeshSwapCommand>(m_Entity, m_Original, finalMesh);
    m_Original.reset();
    return cmd;
}

std::unique_ptr<Command> ExtrudeTool::ExtrudeHit(Scene& scene, const RaycastHit& hit, float worldOffset)
{
    if (!BeginDrag(scene, hit))
        return nullptr;
    ApplyOffset(scene, worldOffset / m_WorldPerLocal);
    return Commit(scene);
}

std::unique_ptr<Command> ExtrudeTool::OnViewportFrame(Scene& scene, const EditorCamera& camera,
                                                      const vec2& viewportPos, const vec2& viewportSize,
                                                      bool viewportHovered)
{
    if (!Busy())
        return nullptr;

    ImGuiIO& io = ImGui::GetIO();
    vec2 uv{(io.MousePos.x - viewportPos.x) / viewportSize.x,
            (io.MousePos.y - viewportPos.y) / viewportSize.y};
    mat4 invVP = glm::inverse(camera.ViewProjection());
    vec4 pNear = invVP * vec4(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, -1.0f, 1.0f);
    vec4 pFar = invVP * vec4(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, 1.0f, 1.0f);
    Ray ray{vec3(pNear) / pNear.w, glm::normalize(vec3(pFar) / pFar.w - vec3(pNear) / pNear.w)};

    if (!m_Dragging) {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            Disarm();
            return nullptr;
        }
        if (viewportHovered && !io.KeyAlt && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            auto hit = scene.Raycast(ray);
            if (!hit || !BeginDrag(scene, *hit))
                Disarm(); // miss or unsupported geometry: drop out of the mode
        }
        return nullptr;
    }

    // Drag: parameter s of the closest point between the mouse ray and the
    // extrusion line hitPos + s * n. Degenerate when the ray is parallel to
    // the line -> keep the previous offset.
    float b = glm::dot(ray.direction, m_LineDirWorld);
    float denom = 1.0f - b * b;
    if (std::abs(denom) > 1e-5f) {
        vec3 w0 = ray.origin - m_LinePointWorld;
        float s = (glm::dot(m_LineDirWorld, w0) - b * glm::dot(ray.direction, w0)) / denom;
        ApplyOffset(scene, s / m_WorldPerLocal);
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        return Commit(scene);
    return nullptr;
}

} // namespace forge

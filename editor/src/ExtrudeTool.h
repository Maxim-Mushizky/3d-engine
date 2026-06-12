#pragma once

#include "CommandStack.h"

#include <forge/core/Math.h>
#include <forge/geometry/MeshEdit.h>
#include <forge/scene/Scene.h>

#include <memory>
#include <vector>

namespace forge {

class EditorCamera;

// SketchUp-style push-pull face extrude. Arm() from the toolbar; the next
// press on a flat region builds the cap + wall geometry once, dragging slides
// it along the face normal, release commits a single MeshSwapCommand.
// Known v1 limits (accepted): a curved surface extrudes just the picked
// triangle, rim shading smears (welded normals), pushing through the far side
// self-intersects.
class ExtrudeTool {
public:
    bool Armed() const { return m_Armed; }
    bool Busy() const { return m_Armed || m_Dragging; }
    void Arm() { m_Armed = true; }
    void Disarm() { m_Armed = false; }

    // Per-frame from the viewport. Returns the undo command when a drag commits.
    std::unique_ptr<Command> OnViewportFrame(Scene& scene, const EditorCamera& camera,
                                             const vec2& viewportPos, const vec2& viewportSize,
                                             bool viewportHovered);

    // Programmatic variant: extrude the face under `hit` by worldOffset at once.
    std::unique_ptr<Command> ExtrudeHit(Scene& scene, const RaycastHit& hit, float worldOffset);

private:
    bool BeginDrag(Scene& scene, const RaycastHit& hit);
    void ApplyOffset(Scene& scene, float localOffset);
    std::unique_ptr<Command> Commit(Scene& scene);

    bool m_Armed = false;
    bool m_Dragging = false;
    UUID m_Entity = 0;
    std::shared_ptr<Mesh> m_Original;    // pre-extrude mesh, for undo / zero-drag bail-out
    MeshTopology m_WorkTopology;         // topology of the working mesh (for normals)
    std::vector<uint32_t> m_MovingVerts; // cap + wall-top vertex indices
    std::vector<vec3> m_MovingBase;      // their positions at offset 0
    size_t m_WallIndexStart = 0;         // first wall index (winding flips on negative offset)
    vec3 m_NormalLocal{0.0f, 1.0f, 0.0f};
    vec3 m_LinePointWorld{0.0f};
    vec3 m_LineDirWorld{0.0f, 1.0f, 0.0f};
    float m_WorldPerLocal = 1.0f; // world-units moved per local-unit of offset
    float m_LastOffsetLocal = 0.0f;
};

} // namespace forge

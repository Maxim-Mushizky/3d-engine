#pragma once

#include "forge/core/Math.h"
#include "forge/renderer/Mesh.h"

#include <memory>
#include <string>

namespace forge {

enum class BooleanOp { Union, Subtract, Intersect };

struct BooleanResult {
    std::shared_ptr<Mesh> mesh; // null on failure
    std::string error;          // user-facing message when mesh is null
};

// CSG via the Manifold library. World transforms are baked into the inputs,
// so the result is in world space — give the result entity an identity
// transform. UVs are zeroed (cut faces have no meaningful UVs); normals are
// recomputed with a 30-degree sharp-edge split.
BooleanResult MeshBoolean(const Mesh& a, const mat4& worldA, const Mesh& b, const mat4& worldB,
                          BooleanOp op);

} // namespace forge

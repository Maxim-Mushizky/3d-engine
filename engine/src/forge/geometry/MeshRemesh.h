#pragma once

#include "forge/renderer/Mesh.h"

#include <memory>

namespace forge {

// Voxel remesh: rebuild the surface as a uniform all-quads-of-triangles shell.
// Narrow-band point-triangle distance + per-column parity for the sign
// (assumes reasonably closed input — true for our primitives and tools),
// marching cubes, then Taubin smoothing to kill the voxel staircase.
// UVs are destroyed (zeroed). resolution = cells along the longest axis.
std::shared_ptr<Mesh> VoxelRemesh(const Mesh& mesh, int resolution);

} // namespace forge

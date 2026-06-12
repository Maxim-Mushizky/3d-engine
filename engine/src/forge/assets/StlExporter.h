#pragma once

#include "forge/scene/Scene.h"

#include <string>
#include <vector>

namespace forge {

// Outcome of an STL export, including the watertight diagnosis (3D printers
// need closed surfaces; we always write the file and let the UI warn).
struct StlExportResult {
    bool ok = false;
    std::string error;       // set when !ok
    uint32_t triangles = 0;  // written to the file
    bool watertight = false; // every edge has exactly 2 tris with opposite winding
    uint32_t openEdges = 0;  // edges with 1 adjacent tri (holes) or >2 (fins)
    uint32_t flippedEdges = 0; // edge pairs whose windings agree (inconsistent facing)
};

// Binary STL of the given entities (light gizmos and meshless nodes are
// skipped). World transforms are baked; Y-up becomes Z-up via the rotation
// (x,y,z) -> (x,-z,y); positions scale by millimetersPerUnit (printers read mm).
StlExportResult ExportStl(const Scene& scene, const std::vector<UUID>& entities,
                          const std::string& path, float millimetersPerUnit = 100.0f);

} // namespace forge

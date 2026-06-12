#pragma once

#include "forge/renderer/Mesh.h"

#include <memory>
#include <string>

namespace forge {

// Procedural primitives, unit-sized, centered at origin, with normals and UVs.
class MeshFactory {
public:
    static std::shared_ptr<Mesh> Cube();
    static std::shared_ptr<Mesh> Sphere(uint32_t rings = 32, uint32_t sectors = 48);
    static std::shared_ptr<Mesh> Plane(float size = 1.0f, uint32_t subdivisions = 1);
    static std::shared_ptr<Mesh> Cylinder(uint32_t sectors = 48);
    static std::shared_ptr<Mesh> Cone(uint32_t sectors = 48);
    static std::shared_ptr<Mesh> Torus(float minorRadius = 0.15f, uint32_t majorSectors = 48, uint32_t minorSectors = 24);

    // Extruded 3D text from a TrueType font (quadratic outlines only — classic
    // .ttf; CFF/.otf fonts are rejected). Watertight by construction (front +
    // back caps share the wall ring positions). Height normalized to ~1 unit,
    // centered at origin; depth in the same normalized units. Returns null when
    // the font can't be read or no glyph produced geometry.
    static std::shared_ptr<Mesh> Text(const std::string& text, const std::string& fontPath,
                                      float depth = 0.25f);
};

} // namespace forge

#pragma once

#include <string>
#include <vector>

#include "raytracer/Scene.h"

namespace crt {

// Parses a .crtscene JSON file at `path` into `scene`. Returns false (and logs to stderr) on
// I/O or parse failure; `scene` may be partially populated in that case.
bool loadScene(const std::string& path, Scene& scene);

// Flattens every object's triangles into a single list for the acceleration tree, tagging
// each triangle with its owning object's index (via objectIndex) so material/emissive
// identity survives once triangles are pulled out of their Object grouping.
std::vector<CRTTriangle> flattenTriangles(const Scene& scene);

} // namespace crt

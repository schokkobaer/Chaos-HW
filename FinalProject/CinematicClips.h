#pragma once

#include <string>
#include <vector>

#include "raytracer/Scene.h"

namespace crt {

// Writes `pixels` (scene.imageWidth x scene.imageHeight) to `path` as a binary PPM (P6).
void writePPM(const std::string& path, const Scene& scene, const std::vector<CRTColor>& pixels);

// Renders the 90-frame orbit/dolly-zoom clip for known final-project scenes, reusing the
// scene's own already-loaded, tree-built state. No-op for scenes without a known configuration.
void renderKnownOrbitClip(const std::string& sceneStem, Scene& scene);

} // namespace crt

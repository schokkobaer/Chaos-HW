#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "raytracer/Scene.h"

namespace crt {

// Resolves `path` to an existing file: an absolute path is returned as-is if it exists;
// otherwise tries `path` relative to the current working directory, then each of
// `searchDirs[i] / path` in order. Falls back to `path` (with any leading slashes stripped) if
// nothing is found, so callers can still report a sensible location in an error message.
//
// Lets both scene-file and scene-resource (texture) paths be given relative to the FinalProject
// folder while working regardless of whether the binary is run from the repo root or its own
// build directory.
std::string resolveExistingPath(const std::string& path, const std::vector<std::filesystem::path>& searchDirs);

// Parses a .crtscene JSON file at `path` into `scene`. Returns false (and logs to stderr) on
// I/O or parse failure; `scene` may be partially populated in that case.
bool loadScene(const std::string& path, Scene& scene);

// Flattens every object's triangles into a single list for the acceleration tree, tagging
// each triangle with its owning object's index (via objectIndex) so material/emissive
// identity survives once triangles are pulled out of their Object grouping.
std::vector<CRTTriangle> flattenTriangles(const Scene& scene);

} // namespace crt

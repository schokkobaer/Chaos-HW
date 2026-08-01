#pragma once

#include "raytracer/Ray.h"
#include "raytracer/Vector.h"

namespace crt {

// A single triangle in a mesh, optionally tagged with which material/object
// it belongs to via colorIndex.
struct CRTTriangle {
    CRTVector v0, v1, v2;
    int colorIndex = 0;

    CRTTriangle(const CRTVector& v0, const CRTVector& v1, const CRTVector& v2, int colorIndex = 0)
        : v0(v0), v1(v1), v2(v2), colorIndex(colorIndex) {}

    CRTVector normal() const;
    double area() const;

    // Möller–Trumbore ray-triangle intersection.
    // Returns true if the ray hits the triangle, and writes the hit distance into t.
    bool intersect(const Ray& ray, double& t) const;
};

} // namespace crt

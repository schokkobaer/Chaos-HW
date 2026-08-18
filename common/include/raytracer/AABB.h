#pragma once

#include "raytracer/Ray.h"
#include "raytracer/Vector.h"
#include <limits>
#include <utility>

namespace crt {

// Axis-aligned bounding box used to cheaply reject rays before testing triangles.
struct AABB {
    CRTVector min{std::numeric_limits<double>::max(), std::numeric_limits<double>::max(), std::numeric_limits<double>::max()};
    CRTVector max{std::numeric_limits<double>::lowest(), std::numeric_limits<double>::lowest(), std::numeric_limits<double>::lowest()};

    // Grows the box so it also contains the given point.
    void expand(const CRTVector& point);

    // Slab-method ray/box test; only answers whether the ray hits the box, not where.
    bool intersects(const Ray& ray) const;

    // Splits the box into two halves at its midpoint along the given axis (0=x, 1=y, 2=z).
    std::pair<AABB, AABB> split(int axis) const;

    // Box/box overlap test (not ray/box) — used to decide which child box(es) a triangle falls into.
    bool overlaps(const AABB& other) const;
};

} // namespace crt

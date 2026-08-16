#pragma once

#include "raytracer/Ray.h"
#include "raytracer/Vector.h"
#include <limits>

namespace crt {

// Axis-aligned bounding box used to cheaply reject rays before testing triangles.
struct AABB {
    CRTVector min{std::numeric_limits<double>::max(), std::numeric_limits<double>::max(), std::numeric_limits<double>::max()};
    CRTVector max{std::numeric_limits<double>::lowest(), std::numeric_limits<double>::lowest(), std::numeric_limits<double>::lowest()};

    // Grows the box so it also contains the given point.
    void expand(const CRTVector& point);

    // Slab-method ray/box test; only answers whether the ray hits the box, not where.
    bool intersects(const Ray& ray) const;
};

} // namespace crt

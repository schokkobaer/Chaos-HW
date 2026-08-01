#pragma once

#include "raytracer/Vector.h"

namespace crt {

// A ray defined by an origin point and a (not necessarily normalized) direction.
struct Ray {
    CRTVector origin;
    CRTVector direction;

    Ray(const CRTVector& origin, const CRTVector& direction)
        : origin(origin), direction(direction) {}
};

} // namespace crt

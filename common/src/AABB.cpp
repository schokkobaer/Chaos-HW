#include "raytracer/AABB.h"
#include <algorithm>

namespace crt {

void AABB::expand(const CRTVector& point)
{
    min = CRTVector(std::min(min.x, point.x), std::min(min.y, point.y), std::min(min.z, point.z));
    max = CRTVector(std::max(max.x, point.x), std::max(max.y, point.y), std::max(max.z, point.z));
}

bool AABB::intersects(const Ray& ray) const
{
    const double origin[3] = {ray.origin.x, ray.origin.y, ray.origin.z};
    const double direction[3] = {ray.direction.x, ray.direction.y, ray.direction.z};
    const double boxMin[3] = {min.x, min.y, min.z};
    const double boxMax[3] = {max.x, max.y, max.z};

    double tMin = -std::numeric_limits<double>::infinity();
    double tMax = std::numeric_limits<double>::infinity();

    for (int axis = 0; axis < 3; ++axis)
    {
        if (direction[axis] == 0.0)
        {
            // Ray is parallel to this slab: it must already lie within it.
            if (origin[axis] < boxMin[axis] || origin[axis] > boxMax[axis])
            {
                return false;
            }
            continue;
        }

        const double invDir = 1.0 / direction[axis];
        double tNear = (boxMin[axis] - origin[axis]) * invDir;
        double tFar = (boxMax[axis] - origin[axis]) * invDir;
        if (tNear > tFar)
        {
            std::swap(tNear, tFar);
        }
        tMin = std::max(tMin, tNear);
        tMax = std::min(tMax, tFar);
        if (tMin > tMax)
        {
            return false;
        }
    }
    return tMax >= 0.0;
}

} // namespace crt

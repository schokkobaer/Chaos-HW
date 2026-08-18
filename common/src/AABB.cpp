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

std::pair<AABB, AABB> AABB::split(int axis) const
{
    const double boxMin[3] = {min.x, min.y, min.z};
    const double boxMax[3] = {max.x, max.y, max.z};
    const double mid = (boxMin[axis] + boxMax[axis]) * 0.5;

    AABB lowerHalf = *this;
    AABB upperHalf = *this;

    double lowerMax[3] = {boxMax[0], boxMax[1], boxMax[2]};
    lowerMax[axis] = mid;
    lowerHalf.max = CRTVector(lowerMax[0], lowerMax[1], lowerMax[2]);

    double upperMin[3] = {boxMin[0], boxMin[1], boxMin[2]};
    upperMin[axis] = mid;
    upperHalf.min = CRTVector(upperMin[0], upperMin[1], upperMin[2]);

    return {lowerHalf, upperHalf};
}

bool AABB::overlaps(const AABB& other) const
{
    return min.x <= other.max.x && other.min.x <= max.x
        && min.y <= other.max.y && other.min.y <= max.y
        && min.z <= other.max.z && other.min.z <= max.z;
}

} // namespace crt

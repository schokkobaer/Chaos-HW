#include "raytracer/Triangle.h"

namespace crt {

namespace {
constexpr double kEpsilon = 1e-9;
}

CRTVector CRTTriangle::getNormalVector() const 
{
    CRTVector edge1 = v1 - v0;
    CRTVector edge2 = v2 - v0;
    return edge1.cross(edge2).normalized();
}
CRTVector CRTTriangle::normal() const {
    return getNormalVector().normalized();
}

double CRTTriangle::area() const {
    CRTVector edge1 = v1 - v0;
    CRTVector edge2 = v2 - v0;
    return 0.5 * edge1.cross(edge2).length();
}

std::optional<TriangleHit> CRTTriangle::intersect(const Ray& ray) const {
    CRTVector edge1 = v1 - v0;
    CRTVector edge2 = v2 - v0;
    CRTVector h = ray.direction.cross(edge2);
    double a = edge1.dot(h);

    // Ray is parallel to the triangle.
    if (a > -kEpsilon && a < kEpsilon)
    {
        return std::nullopt;
    }
    double f = 1.0 / a;
    CRTVector s = ray.origin - v0;
    double u = f * s.dot(h);
    if (u < 0.0 || u > 1.0){
        return std::nullopt;
    }
    CRTVector q = s.cross(edge1);
    double v = f * ray.direction.dot(q);
    if (v < 0.0 || u + v > 1.0){
        return std::nullopt;
    }

    double t = f * edge2.dot(q);
    
    if (t > kEpsilon) {
        return TriangleHit{t, u, v};
    } else {
        return std::nullopt;
    }
}

} // namespace crt

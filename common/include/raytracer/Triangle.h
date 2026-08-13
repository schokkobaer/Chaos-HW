#pragma once

#include "raytracer/Ray.h"
#include "raytracer/Vector.h"
#include <optional>

namespace crt {

    // A single triangle hit record, containing the distance (t), and the barycentric coordinates (u,v)
    class TriangleHit {
    public:
    TriangleHit(double t, double u, double v)
        : t(t), u(u), v(v),hitNormal(0,0,0) {}  
    double t;
    double u;
    double v;
    void setHitNormal(const CRTVector& normal) 
    {
        hitNormal = normal;
        hitNormalIsSet = true;
    }

    std::optional<CRTVector> getHitNormal() const 
    {
        if (!hitNormalIsSet) 
        {
            return std::nullopt;
        }
        return hitNormal;
    }

    private:
        bool hitNormalIsSet{false};
        CRTVector hitNormal{0, 0, 0};
    };

// A single triangle in a mesh, optionally tagged with which material/object
// it belongs to via colorIndex.
struct CRTTriangle {
    CRTVector v0, v1, v2;
    CRTVector n0, n1, n2; // Vertex normals for smooth shading
    int colorIndex = 0;

    CRTTriangle(const CRTVector& v0, const CRTVector& v1, const CRTVector& v2 )
        : v0(v0), v1(v1), v2(v2) {}

    CRTVector normal() const;
    CRTVector getNormalVector() const;
    void setVertexNormals(const CRTVector& n0, const CRTVector& n1, const CRTVector& n2) {
        this->n0 = n0;
        this->n1 = n1;
        this->n2 = n2;
    }
    double area() const;

    // Möller–Trumbore ray-triangle intersection.
    // Returns true if the ray hits the triangle, and writes the hit distance into t.
    std::optional<TriangleHit> intersect(const Ray& ray) const;
};

} // namespace crt

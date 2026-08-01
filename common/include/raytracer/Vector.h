#pragma once

namespace crt {

// A simple 3D vector used throughout the ray tracer for points, directions
// and geometric operations (not to be confused with CRTColor).
struct CRTVector {
    double x, y, z;

    explicit CRTVector(double x = 0, double y = 0, double z = 0) : x(x), y(y), z(z) {}

    CRTVector normalized() const;
    CRTVector cross(const CRTVector& other) const;
    double dot(const CRTVector& other) const;
    double length() const;

    CRTVector operator+(const CRTVector& other) const;
    CRTVector operator-(const CRTVector& other) const;
    CRTVector operator*(double scalar) const;

    // Rotate around the world X/Y/Z axis by angleRad (right-hand rule).
    CRTVector rotateX(double angleRad) const;
    CRTVector rotateY(double angleRad) const;
    CRTVector rotateZ(double angleRad) const;

    // Rotate this vector around an arbitrary unit axis by angleRad (right-hand rule).
    CRTVector rotateAroundAxis(const CRTVector& axis, double angleRad) const;
};

// Rotate a point around a pivot by Euler angles (radians), X then Y then Z.
CRTVector rotateAround(const CRTVector& p, const CRTVector& pivot,
                       double angleXRad, double angleYRad, double angleZRad);

} // namespace crt

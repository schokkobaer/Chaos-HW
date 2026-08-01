#include "raytracer/Vector.h"

#include <cmath>

namespace crt {

CRTVector CRTVector::normalized() const {
    constexpr double kEpsilon = 1e-12;
    double len = std::sqrt(x * x + y * y + z * z);
    if (len <= kEpsilon) {
        return CRTVector();
    }
    return CRTVector(x / len, y / len, z / len);
}

CRTVector CRTVector::cross(const CRTVector& other) const {
    return CRTVector(
        y * other.z - z * other.y,
        z * other.x - x * other.z,
        x * other.y - y * other.x
    );
}

double CRTVector::dot(const CRTVector& other) const {
    return x * other.x + y * other.y + z * other.z;
}

double CRTVector::length() const {
    return std::sqrt(x * x + y * y + z * z);
}

CRTVector CRTVector::operator+(const CRTVector& other) const {
    return CRTVector(x + other.x, y + other.y, z + other.z);
}

CRTVector CRTVector::operator-(const CRTVector& other) const {
    return CRTVector(x - other.x, y - other.y, z - other.z);
}

CRTVector CRTVector::operator*(double scalar) const {
    return CRTVector(x * scalar, y * scalar, z * scalar);
}

CRTVector CRTVector::rotateX(double angleRad) const {
    double c = std::cos(angleRad);
    double s = std::sin(angleRad);
    return CRTVector(x, y * c - z * s, y * s + z * c);
}

CRTVector CRTVector::rotateY(double angleRad) const {
    double c = std::cos(angleRad);
    double s = std::sin(angleRad);
    return CRTVector(x * c + z * s, y, -x * s + z * c);
}

CRTVector CRTVector::rotateZ(double angleRad) const {
    double c = std::cos(angleRad);
    double s = std::sin(angleRad);
    return CRTVector(x * c - y * s, x * s + y * c, z);
}

CRTVector CRTVector::rotateAroundAxis(const CRTVector& axis, double angleRad) const {
    double c = std::cos(angleRad);
    double s = std::sin(angleRad);
    CRTVector u = axis.normalized();
    if (u.length() <= 1e-12) {
        return *this;
    }
    CRTVector parallel = u * dot(u);
    CRTVector perpendicular = *this - parallel;
    CRTVector crossProd = u.cross(*this);
    return parallel + perpendicular * c + crossProd * s;
}

CRTVector rotateAround(const CRTVector& p, const CRTVector& pivot,
                       double angleXRad, double angleYRad, double angleZRad) {
    CRTVector v = p - pivot;
    v = v.rotateX(angleXRad).rotateY(angleYRad).rotateZ(angleZRad);
    return v + pivot;
}

} // namespace crt

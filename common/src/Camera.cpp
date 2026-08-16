#include "raytracer/Camera.h"
#include "raytracer/Camera.h"

namespace crt {

Camera::Camera(const CRTVector& origin, const CRTVector& forward, const CRTVector& up)
    : origin(origin), forward(forward.normalized())
{
    right = this->forward.cross(up).normalized();
    this->up = right.cross(this->forward).normalized();
}

void Camera::recomputeBasis() {
    forward = forward.normalized();
    right = forward.cross(up).normalized();
    up = right.cross(forward).normalized();
}

void Camera::applyMatrix(const double m[9]) {
    auto transform = [&](const CRTVector& v) {
        return CRTVector(
            m[0] * v.x + m[1] * v.y + m[2] * v.z,
            m[3] * v.x + m[4] * v.y + m[5] * v.z,
            m[6] * v.x + m[7] * v.y + m[8] * v.z
        );
    };
    right = transform(right).normalized();
    up = transform(up).normalized();
    forward = transform(forward).normalized();
}

void Camera::dolly(double distance) {
    origin = origin + forward * distance;
}

void Camera::truck(double distance) {
    origin = origin + right * distance;
}

void Camera::pedestal(double distance) {
    origin = origin + up * distance;
}

void Camera::pan(double angleRad) {
    forward = forward.rotateY(angleRad);
    up = up.rotateY(angleRad);
    right = right.rotateY(angleRad);
}

void Camera::tilt(double angleRad) {
    forward = forward.rotateAroundAxis(right, angleRad);
    up = up.rotateAroundAxis(right, angleRad);
}

void Camera::roll(double angleRad) {
    up = up.rotateAroundAxis(forward, angleRad);
    right = right.rotateAroundAxis(forward, angleRad);
}

Ray Camera::generateRay(double xScreen, double yScreen) const {
    CRTVector dir = (forward + right * xScreen + up * yScreen).normalized();
    return Ray(origin, dir);
}

Ray Camera::generateRayForPixel(int colIdx, int rowIdx, int imageWidth, int imageHeight, double aspectRatio, double subX, double subY) const
{
    const double xRaster = static_cast<double>(colIdx) + subX;
	const double yRaster = static_cast<double>(rowIdx) + subY;

	const double xNdc = xRaster / static_cast<double>(imageWidth);
	const double yNdc = yRaster / static_cast<double>(imageHeight);

	double xScreen = (2.0 * xNdc) - 1.0;
	const double yScreen = 1.0 - (2.0 * yNdc);
	xScreen *= aspectRatio;

	return this->generateRay(xScreen, yScreen);
}
} // namespace crt

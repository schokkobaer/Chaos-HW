#pragma once

#include "raytracer/Ray.h"
#include "raytracer/Vector.h"

namespace crt {

// A right-handed camera basis (right, up, forward) plus an origin, with both
// "cinematic" movement controls (dolly/truck/pedestal/pan/tilt/roll) and
// direct rotation-matrix application (as used by JSON scene files).
struct Camera {
    CRTVector origin;
    CRTVector forward{0, 0, -1};
    CRTVector right{1, 0, 0};
    CRTVector up{0, 1, 0};

    Camera() = default;

    // Builds a camera at `origin` looking along `forward`, deriving a
    // right-handed orthonormal basis from `forward` and the given `up` hint.
    Camera(const CRTVector& origin, const CRTVector& forward, const CRTVector& up = CRTVector(0, 1, 0));

    // Re-orthonormalizes the basis from the current forward/up (useful after
    // manually assigning forward/up).
    void recomputeBasis();

    // Apply a row-major 3x3 rotation matrix (as read from a scene file) to
    // the camera's basis vectors.
    void applyMatrix(const double m[9]);

    // Move forward/backward along the camera's forward axis.
    void dolly(double distance);
    // Move left/right along the camera's right axis.
    void truck(double distance);
    // Move up/down along the camera's up axis.
    void pedestal(double distance);

    // Rotate around the world Y axis (yaw).
    void pan(double angleRad);
    // Rotate around the camera's right axis (pitch).
    void tilt(double angleRad);
    // Rotate around the camera's forward axis (roll).
    void roll(double angleRad);

    // Generate a ray through screen-space offsets xScreen/yScreen (typically
    // in [-1, 1], already scaled by aspect ratio/FOV by the caller).
    Ray generateRay(double xScreen, double yScreen) const;

    // Generate a ray given the pixel coordinates. subX/subY are the sub-pixel
    // sample offsets within the pixel (each in [0, 1), default 0.5 = pixel center).
    Ray generateRayForPixel(int colIdx, int rowIdx, int imageWidth, int imageHeight, double aspectRatio, double subX = 0.5, double subY = 0.5) const;
};

} // namespace crt

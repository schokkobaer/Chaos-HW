// Smoke test for the shared raytracer::common library (see /common).
// This isn't a full renderer yet - it just proves that CRTVector, Camera,
// Ray and CRTTriangle link and work correctly from a homework project, and
// renders a tiny gradient/triangle-silhouette PPM as visual proof.
#include <fstream>
#include <iostream>

#include "raytracer/Camera.h"
#include "raytracer/Triangle.h"
#include "raytracer/Vector.h"

using crt::Camera;
using crt::CRTTriangle;
using crt::CRTVector;

int main() {
    const int imageWidth = 320;
    const int imageHeight = 180;
    const double aspectRatio = static_cast<double>(imageWidth) / imageHeight;

    Camera camera(CRTVector(0, 0, 0), CRTVector(0, 0, -1), CRTVector(0, 1, 0));
    CRTTriangle triangle(
        CRTVector(-0.5, -0.5, -1),
        CRTVector(0.5, -0.5, -1),
        CRTVector(0.0, 0.5, -1));

    std::ofstream out("homework8_smoke_test.ppm");
    if (!out.is_open()) {
        std::cerr << "Failed to open homework8_smoke_test.ppm for writing\n";
        return 1;
    }

    out << "P3\n" << imageWidth << " " << imageHeight << "\n255\n";

    for (int y = 0; y < imageHeight; ++y) {
        for (int x = 0; x < imageWidth; ++x) {
            double ndcX = (2.0 * ((x + 0.5) / imageWidth) - 1.0) * aspectRatio;
            double ndcY = 1.0 - 2.0 * ((y + 0.5) / imageHeight);

            crt::Ray ray = camera.generateRay(ndcX, ndcY);

            double t;
            bool hit = triangle.intersect(ray, t);
            int shade = hit ? 255 : 20;
            out << shade << " " << shade << " " << shade << "\n";
        }
    }

    std::cout << "Wrote homework8_smoke_test.ppm (" << imageWidth << "x" << imageHeight << ")\n";
    std::cout << "Camera forward: (" << camera.forward.x << ", " << camera.forward.y
              << ", " << camera.forward.z << ")\n";
    return 0;
}

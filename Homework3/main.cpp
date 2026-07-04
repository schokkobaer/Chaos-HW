#include <fstream>
#include <cmath>

/// Output image resolution
static const int imageWidth = 1920;
static const int imageHeight = 1080;
static const int maxColorComponent = 255;
static const double aspectRatio = static_cast<double>(imageWidth) / imageHeight;

struct CRTVector {
    double x, y, z;

    CRTVector(double x = 0, double y = 0, double z = 0) : x(x), y(y), z(z) {}

    CRTVector normalized() const {
        double len = std::sqrt(x * x + y * y + z * z);
        return CRTVector(x / len, y / len, z / len);
    }
};

struct CRTColor {
    int r, g, b;

    CRTColor(int r = 0, int g = 0, int b = 0) : r(r), g(g), b(b) {}
};

struct Ray {
    CRTVector origin;
    CRTVector direction;

    Ray(const CRTVector& origin, const CRTVector& direction)
        : origin(origin), direction(direction) {}
};

int main()
{
    // Phase 1: generate rays and fill pixel buffer
    CRTColor pixels[imageHeight][imageWidth];
    const CRTVector cameraOrigin(0, 0, 0);

    for (int rowIdx = 0; rowIdx < imageHeight; ++rowIdx)
    {
        for (int colIdx = 0; colIdx < imageWidth; ++colIdx)
        {
            // Step 1: pixel center in raster coordinates
            double x_raster = colIdx + 0.5;
            double y_raster = rowIdx + 0.5;

            // Step 2: raster -> NDC [0, 1]
            double x_ndc = x_raster / imageWidth;
            double y_ndc = y_raster / imageHeight;

            // Step 3: NDC -> screen space [-1, 1]
            double x_screen = (2.0 * x_ndc) - 1.0;
            double y_screen = 1.0 - (2.0 * y_ndc);

            // Step 4: apply aspect ratio
            x_screen *= aspectRatio;

            // Step 5: build and normalize ray direction; camera faces -Z
            CRTVector rayDir = CRTVector(x_screen, y_screen, -1.0).normalized();
            Ray ray(cameraOrigin, rayDir);

            // Map normalized direction to color; abs() handles negative components
            pixels[rowIdx][colIdx] = CRTColor(
                static_cast<int>(std::abs(ray.direction.x) * maxColorComponent),
                static_cast<int>(std::abs(ray.direction.y) * maxColorComponent),
                static_cast<int>(std::abs(ray.direction.z) * maxColorComponent)
            );
        }
    }

    // Phase 2: write pixel buffer to PPM file
    std::ofstream ppmFileStream("crt_output_image.ppm", std::ios::out | std::ios::binary);
    ppmFileStream << "P3\n";
    ppmFileStream << imageWidth << " " << imageHeight << "\n";
    ppmFileStream << maxColorComponent << "\n";

    for (int rowIdx = 0; rowIdx < imageHeight; ++rowIdx)
    {
        for (int colIdx = 0; colIdx < imageWidth; ++colIdx)
        {
            const CRTColor& c = pixels[rowIdx][colIdx];
            ppmFileStream << c.r << " " << c.g << " " << c.b << "\t";
        }
        ppmFileStream << "\n";
    }

    ppmFileStream.close();
    return 0;
}
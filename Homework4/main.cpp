#include <fstream>
#include <cmath>
#include <vector>

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

    CRTVector cross(const CRTVector& other) const {
        return CRTVector(
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        );
    }

    double dot(const CRTVector& other) const {
        return x * other.x + y * other.y + z * other.z;
    }

    CRTVector operator-(const CRTVector& other) const {
        return CRTVector(x - other.x, y - other.y, z - other.z);
    }
    CRTVector operator+(const CRTVector& other) const {
        return CRTVector(x + other.x, y + other.y, z + other.z);
    }
    CRTVector operator*(double scalar) const {
        return CRTVector(x * scalar, y * scalar, z * scalar);
    }

    double length() const {
        return std::sqrt(x * x + y * y + z * z);
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



struct CRTTriangle {
    CRTVector v0, v1, v2;

    CRTTriangle(const CRTVector& v0, const CRTVector& v1, const CRTVector& v2)
        : v0(v0), v1(v1), v2(v2) {}

    double area() const {
        CRTVector edge1 = v1 - v0;
        CRTVector edge2 = v2 - v0;
        return 0.5 * edge1.cross(edge2).length();
    }

    // Möller–Trumbore ray-triangle intersection.
    // Returns true if the ray hits the triangle, and writes the hit distance into t.
    bool intersect(const Ray& ray, double& t) const {
        const double EPSILON = 1e-6;
        CRTVector edge1 = v1 - v0;
        CRTVector edge2 = v2 - v0;
        CRTVector h = ray.direction.cross(edge2);
        double a = edge1.dot(h);

        // Ray is parallel to the triangle.
        if (a > -EPSILON && a < EPSILON)
            return false;

        double f = 1.0 / a;
        CRTVector s = ray.origin - v0;
        double u = f * s.dot(h);
        if (u < 0.0 || u > 1.0)
            return false;

        CRTVector q = s.cross(edge1);
        double v = f * ray.direction.dot(q);
        if (v < 0.0 || u + v > 1.0)
            return false;

        t = f * edge2.dot(q);
        return t > EPSILON;
    }
};

int main()
{
    // Phase 1: generate rays and fill pixel buffer
    std::vector<CRTColor> pixels(imageHeight * imageWidth);
    const CRTVector cameraOrigin(0, 0, 0);

    // Place a triangle in front of the camera (camera faces -Z).
    CRTTriangle triangle(
        CRTVector(-1.0, -0.5, -3.0),
        CRTVector( 1.0, -0.5, -3.0),
        CRTVector( 0.0,  1.0, -3.0)
    );

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

            double t = 0.0;
            int pixelIndex = rowIdx * imageWidth + colIdx;
            if (triangle.intersect(ray, t))
            {
                // Hit the triangle: color it red.
                pixels[pixelIndex] = CRTColor(255, 0, 0);
            }
            else
            {
                // Background: map normalized direction to color.
                pixels[pixelIndex] = CRTColor(
                    static_cast<int>(std::abs(ray.direction.x) * maxColorComponent),
                    static_cast<int>(std::abs(ray.direction.y) * maxColorComponent),
                    static_cast<int>(std::abs(ray.direction.z) * maxColorComponent)
                );
            }
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
            const CRTColor& c = pixels[rowIdx * imageWidth + colIdx];
            ppmFileStream << c.r << " " << c.g << " " << c.b << "\t";
        }
        ppmFileStream << "\n";
    }

    ppmFileStream.close();
    return 0;
}
#include <fstream>
#include <cmath>
#include <vector>
#include <cstdio>

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

    CRTVector rotateX(double angleRad) const {
        double c = std::cos(angleRad);
        double s = std::sin(angleRad);
        return CRTVector(x, y * c - z * s, y * s + z * c);
    }

    CRTVector rotateY(double angleRad) const {
        double c = std::cos(angleRad);
        double s = std::sin(angleRad);
        return CRTVector(x * c + z * s, y, -x * s + z * c);
    }

    CRTVector rotateZ(double angleRad) const {
        double c = std::cos(angleRad);
        double s = std::sin(angleRad);
        return CRTVector(x * c - y * s, x * s + y * c, z);
    }

    // Rotate this vector around an arbitrary unit axis by angleRad (right-hand rule).
    CRTVector rotateAroundAxis(const CRTVector& axis, double angleRad) const {
        double c = std::cos(angleRad);
        double s = std::sin(angleRad);
        CRTVector u = axis.normalized();
        CRTVector parallel = u * dot(u);
        CRTVector perpendicular = *this - parallel;
        CRTVector crossProd = u.cross(*this);
        return parallel + perpendicular * c + crossProd * s;
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

// Rotate a point around a pivot by Euler angles (radians).
CRTVector rotateAround(const CRTVector& p, const CRTVector& pivot,
                       double ax, double ay, double az) {
    CRTVector v = p - pivot;
    v = v.rotateX(ax).rotateY(ay).rotateZ(az);
    return v + pivot;
}

struct Camera {
    CRTVector origin;
    CRTVector forward;
    CRTVector right;
    CRTVector up;

    Camera(const CRTVector& origin = CRTVector(0, 0, 0),
           const CRTVector& forward = CRTVector(0, 0, -1),
           const CRTVector& up = CRTVector(0, 1, 0))
        : origin(origin), forward(forward.normalized())
    {
        right = this->forward.cross(up).normalized();
        this->up = right.cross(this->forward).normalized();
    }

    void recomputeBasis() {
        forward = forward.normalized();
        right = forward.cross(up).normalized();
        up = right.cross(forward).normalized();
    }

    // Move forward/backward along the camera's Z axis.
    void dolly(double distance) {
        origin = origin + forward * distance;
    }

    // Move left/right along the camera's X axis.
    void truck(double distance) {
        origin = origin + right * distance;
    }

    // Move up/down along the camera's Y axis.
    void pedestal(double distance) {
        origin = origin + up * distance;
    }

    // Rotate around the world Y axis (yaw).
    void pan(double angleRad) {
        forward = forward.rotateY(angleRad);
        up = up.rotateY(angleRad);
        right = right.rotateY(angleRad);
    }

    // Rotate around the camera's right axis (pitch).
    void tilt(double angleRad) {
        forward = forward.rotateAroundAxis(right, angleRad);
        up = up.rotateAroundAxis(right, angleRad);
    }

    // Rotate around the camera's forward axis (roll).
    void roll(double angleRad) {
        up = up.rotateAroundAxis(forward, angleRad);
        right = right.rotateAroundAxis(forward, angleRad);
    }

    Ray generateRay(double xScreen, double yScreen) const {
        CRTVector dir = (forward + right * xScreen + up * yScreen).normalized();
        return Ray(origin, dir);
    }
};

int main()
{
    // Toggle between the original pyramids and a single simple triangle.
    const bool useSimpleTriangle = true;

    std::vector<CRTTriangle> triangles;
    std::vector<CRTColor> colors;

    if (useSimpleTriangle)
    {
        triangles = {
            CRTTriangle(
                CRTVector(-1.75, -1.75, -3),
                CRTVector( 1.75, -1.75, -3),
                CRTVector( 0.0,   1.75, -3)
            )
        };

        colors = {
            CRTColor(255, 128, 0)  // orange triangle
        };
    }
    else
    {
        // --- Square pyramid on the left ---
        // Build it standing up in local space: base in XZ plane, apex above +Y.
        CRTVector sqApexLocal(0.0,  1.2, 0.0);
        CRTVector sqFL(-0.7, -1.0, -0.7);
        CRTVector sqFR( 0.7, -1.0, -0.7);
        CRTVector sqBR( 0.7, -1.0,  0.7);
        CRTVector sqBL(-0.7, -1.0,  0.7);

        // --- Hexagonal pyramid on the right ---
        const double hr = 0.5;
        const double hh = 0.8660254037844386;
        CRTVector hexApexLocal(0.0, 1.2, 0.0);
        CRTVector hexV0( 0.7, -1.0,  0.0);
        CRTVector hexV1( 0.35, -1.0,  hh * 0.7);
        CRTVector hexV2(-0.35, -1.0,  hh * 0.7);
        CRTVector hexV3(-0.7, -1.0,  0.0);
        CRTVector hexV4(-0.35, -1.0, -hh * 0.7);
        CRTVector hexV5( 0.35, -1.0, -hh * 0.7);

        // Rotate local models so the base tilts toward the camera, then spin them.
        // rotateX(+0.9) tips the apex upward in screen space (camera looks from below).
        const double sqRotY = 0.4;
        const double hexRotY = -0.4;
        CRTVector sqOffset(-1.5, 0.0, -3.5);
        CRTVector hexOffset( 1.5, 0.0, -3.5);

        CRTVector sqApex = sqApexLocal.rotateX(0.9).rotateY(sqRotY) + sqOffset;
        sqFL = sqFL.rotateX(0.9).rotateY(sqRotY) + sqOffset;
        sqFR = sqFR.rotateX(0.9).rotateY(sqRotY) + sqOffset;
        sqBR = sqBR.rotateX(0.9).rotateY(sqRotY) + sqOffset;
        sqBL = sqBL.rotateX(0.9).rotateY(sqRotY) + sqOffset;

        CRTVector hexApex = hexApexLocal.rotateX(0.9).rotateY(hexRotY) + hexOffset;
        hexV0 = hexV0.rotateX(0.9).rotateY(hexRotY) + hexOffset;
        hexV1 = hexV1.rotateX(0.9).rotateY(hexRotY) + hexOffset;
        hexV2 = hexV2.rotateX(0.9).rotateY(hexRotY) + hexOffset;
        hexV3 = hexV3.rotateX(0.9).rotateY(hexRotY) + hexOffset;
        hexV4 = hexV4.rotateX(0.9).rotateY(hexRotY) + hexOffset;
        hexV5 = hexV5.rotateX(0.9).rotateY(hexRotY) + hexOffset;

        triangles = {
            // Square pyramid: 4 side faces + base
            CRTTriangle(sqApex, sqFL, sqFR),   // front
            CRTTriangle(sqApex, sqFR, sqBR),   // right
            CRTTriangle(sqApex, sqBR, sqBL),   // back
            CRTTriangle(sqApex, sqBL, sqFL),   // left
            CRTTriangle(sqFL,  sqBL, sqBR),    // base (two triangles)
            CRTTriangle(sqFL,  sqBR, sqFR),

            // Hexagonal pyramid: 6 side faces + base
            CRTTriangle(hexApex, hexV0, hexV1),
            CRTTriangle(hexApex, hexV1, hexV2),
            CRTTriangle(hexApex, hexV2, hexV3),
            CRTTriangle(hexApex, hexV3, hexV4),
            CRTTriangle(hexApex, hexV4, hexV5),
            CRTTriangle(hexApex, hexV5, hexV0),
            CRTTriangle(hexV0, hexV5, hexV4),  // base (two triangles)
            CRTTriangle(hexV0, hexV4, hexV3),
            CRTTriangle(hexV0, hexV3, hexV2),
            CRTTriangle(hexV0, hexV2, hexV1)
        };

        // Simple color palette: one color per face.
        colors = {
            CRTColor(255,   0,   0),  // sq front  - red
            CRTColor(  0, 255,   0),  // sq right  - green
            CRTColor(  0,   0, 255),  // sq back   - blue
            CRTColor(255, 255,   0),  // sq left   - yellow
            CRTColor(255,   0, 255),  // sq base 1 - magenta
            CRTColor(  0, 255, 255),  // sq base 2 - cyan

            CRTColor(255, 128,   0),  // hex face 1 - orange
            CRTColor(255, 255,   0),  // hex face 2 - yellow
            CRTColor(128, 255,   0),  // hex face 3 - lime
            CRTColor(  0, 255, 128),  // hex face 4 - spring green
            CRTColor(  0, 128, 255),  // hex face 5 - sky blue
            CRTColor(128,   0, 255),  // hex face 6 - purple
            CRTColor(255,   0, 128),  // hex base 1 - pink
            CRTColor(255,  64,  64),  // hex base 2 - light red
            CRTColor( 64, 255,  64),  // hex base 3 - light green
            CRTColor( 64,  64, 255)   // hex base 4 - light blue
        };
    }

    // Initial camera: looking down -Z, Y up.
    Camera camera;

    // Animation settings.
    const int frameCount = 72;
    const double panPerFrameDeg = 5.0;
    const double deg2rad = 3.14159265358979323846 / 180.0;

    for (int frame = 0; frame < frameCount; ++frame)
    {
        // Render the current frame.
        std::vector<CRTColor> pixels(imageHeight * imageWidth);
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

                // Step 5: build ray from the camera's basis.
                Ray ray = camera.generateRay(x_screen, y_screen);

                int pixelIndex = rowIdx * imageWidth + colIdx;

                // Closest-hit search: keep the nearest intersection.
                double closestT = 1e30;
                int closestIdx = -1;
                for (size_t i = 0; i < triangles.size(); ++i)
                {
                    double t = 0.0;
                    if (triangles[i].intersect(ray, t))
                    {
                        if (t < closestT)
                        {
                            closestT = t;
                            closestIdx = static_cast<int>(i);
                        }
                    }
                }

                if (closestIdx >= 0)
                {
                    pixels[pixelIndex] = colors[closestIdx];
                }
                else
                {
                    // Background: dark gradient based on ray direction.
                    pixels[pixelIndex] = CRTColor(
                        static_cast<int>(std::abs(ray.direction.x) * 40),
                        static_cast<int>(std::abs(ray.direction.y) * 40),
                        static_cast<int>(std::abs(ray.direction.z) * 60)
                    );
                }
            }
        }

        // Write the frame to a numbered PPM file.
        char fileName[64];
        std::snprintf(fileName, sizeof(fileName), "frame_%04d.ppm", frame);
        std::ofstream ppmFileStream(fileName, std::ios::out | std::ios::binary);
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

        // Advance the camera for the next frame.
        camera.pan(panPerFrameDeg * deg2rad);
    }

    return 0;
}
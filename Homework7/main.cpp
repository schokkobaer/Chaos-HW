#include <fstream>
#include <cmath>
#include <vector>
#include <cstdio>
#include <string>
#include <iostream>
#include <thread>
#include <algorithm>
#include <filesystem>

#include "rapidjson/document.h"
#include "rapidjson/istreamwrapper.h"

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
    int colorIndex = 0; // which object/material this triangle belongs to

    CRTTriangle(const CRTVector& v0, const CRTVector& v1, const CRTVector& v2, int colorIndex = 0)
        : v0(v0), v1(v1), v2(v2), colorIndex(colorIndex) {}

    CRTVector normal() const {
        return (v1 - v0).cross(v2 - v0).normalized();
    }

    // Möller–Trumbore ray-triangle intersection.
    // Returns true if the ray hits the triangle, and writes the hit distance into t.
    bool intersect(const Ray& ray, double& t) const {
        const double EPSILON = 1e-9;
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

struct Camera {
    CRTVector origin;
    CRTVector right{1, 0, 0};
    CRTVector up{0, 1, 0};
    CRTVector forward{0, 0, -1};

    // Apply a row-major 3x3 rotation matrix (as read from the scene file)
    // to the camera's basis vectors.
    void applyMatrix(const double m[9]) {
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

    Ray generateRay(double xScreen, double yScreen) const {
        CRTVector dir = (forward + right * xScreen + up * yScreen).normalized();
        return Ray(origin, dir);
    }
};

//------------------------------------------------------------------
// Scene: parsed from a .crtscene (JSON) file using RapidJSON
//------------------------------------------------------------------

struct Scene {
    int imageWidth = 1920;
    int imageHeight = 1080;
    CRTColor backgroundColor;
    Camera camera;
    std::vector<CRTTriangle> triangles;
};

static CRTColor colorFromUnitFloats(const rapidjson::Value& arr) {
    double r = arr[0].GetDouble();
    double g = arr[1].GetDouble();
    double b = arr[2].GetDouble();
    auto clamp255 = [](double c) {
        int v = static_cast<int>(std::round(c * 255.0));
        return std::max(0, std::min(255, v));
    };
    return CRTColor(clamp255(r), clamp255(g), clamp255(b));
}

// A small fixed palette used to give each object in the scene a distinct base color.
static const std::vector<CRTColor> kPalette = {
    CRTColor(220, 70, 70),
    CRTColor(70, 150, 230),
    CRTColor(90, 200, 120),
    CRTColor(230, 180, 50),
    CRTColor(180, 90, 220),
    CRTColor(60, 200, 200)
};

bool loadScene(const std::string& path, Scene& scene) {
    std::ifstream fileStream(path);
    if (!fileStream.is_open()) {
        std::cerr << "Failed to open scene file: " << path << std::endl;
        return false;
    }

    rapidjson::IStreamWrapper isw(fileStream);
    rapidjson::Document doc;
    doc.ParseStream(isw);
    if (doc.HasParseError()) {
        std::cerr << "Failed to parse JSON in: " << path << std::endl;
        return false;
    }

    // --- settings ---
    if (doc.HasMember("settings")) {
        const auto& settings = doc["settings"];
        if (settings.HasMember("background_color")) {
            scene.backgroundColor = colorFromUnitFloats(settings["background_color"]);
        }
        if (settings.HasMember("image_settings")) {
            const auto& imgSettings = settings["image_settings"];
            scene.imageWidth = imgSettings["width"].GetInt();
            scene.imageHeight = imgSettings["height"].GetInt();
        }
    }

    // --- camera ---
    if (doc.HasMember("camera")) {
        const auto& cameraNode = doc["camera"];
        if (cameraNode.HasMember("matrix")) {
            const auto& m = cameraNode["matrix"];
            double matrix[9];
            for (int i = 0; i < 9; ++i)
                matrix[i] = m[i].GetDouble();
            scene.camera.applyMatrix(matrix);
        }
        if (cameraNode.HasMember("position")) {
            const auto& p = cameraNode["position"];
            scene.camera.origin = CRTVector(p[0].GetDouble(), p[1].GetDouble(), p[2].GetDouble());
        }
    }

    // --- objects ---
    if (doc.HasMember("objects")) {
        const auto& objects = doc["objects"];
        for (rapidjson::SizeType objIdx = 0; objIdx < objects.Size(); ++objIdx) {
            const auto& obj = objects[objIdx];

            std::vector<CRTVector> vertices;
            const auto& vertsArr = obj["vertices"];
            vertices.reserve(vertsArr.Size() / 3);
            for (rapidjson::SizeType i = 0; i + 2 < vertsArr.Size(); i += 3) {
                vertices.emplace_back(
                    vertsArr[i].GetDouble(),
                    vertsArr[i + 1].GetDouble(),
                    vertsArr[i + 2].GetDouble()
                );
            }

            int colorIndex = static_cast<int>(objIdx) % static_cast<int>(kPalette.size());

            const auto& triArr = obj["triangles"];
            for (rapidjson::SizeType i = 0; i + 2 < triArr.Size(); i += 3) {
                int i0 = triArr[i].GetInt();
                int i1 = triArr[i + 1].GetInt();
                int i2 = triArr[i + 2].GetInt();
                scene.triangles.emplace_back(vertices[i0], vertices[i1], vertices[i2], colorIndex);
            }
        }
    }

    return true;
}

//------------------------------------------------------------------
// Rendering
//------------------------------------------------------------------

void renderScene(const Scene& scene, std::vector<CRTColor>& pixels) {
    const double aspectRatio = static_cast<double>(scene.imageWidth) / scene.imageHeight;
    pixels.assign(static_cast<size_t>(scene.imageWidth) * scene.imageHeight, scene.backgroundColor);

    // A fixed "headlight" style shading: brighter where the triangle faces the camera.
    const double ambient = 0.25;
    const double diffuse = 0.75;

    auto renderRows = [&](int rowStart, int rowEnd) {
        for (int rowIdx = rowStart; rowIdx < rowEnd; ++rowIdx) {
            for (int colIdx = 0; colIdx < scene.imageWidth; ++colIdx) {
                double x_raster = colIdx + 0.5;
                double y_raster = rowIdx + 0.5;

                double x_ndc = x_raster / scene.imageWidth;
                double y_ndc = y_raster / scene.imageHeight;

                double x_screen = (2.0 * x_ndc) - 1.0;
                double y_screen = 1.0 - (2.0 * y_ndc);

                x_screen *= aspectRatio;

                Ray ray = scene.camera.generateRay(x_screen, y_screen);

                double closestT = 1e30;
                int closestIdx = -1;
                for (size_t i = 0; i < scene.triangles.size(); ++i) {
                    double t = 0.0;
                    if (scene.triangles[i].intersect(ray, t) && t < closestT) {
                        closestT = t;
                        closestIdx = static_cast<int>(i);
                    }
                }

                if (closestIdx >= 0) {
                    const CRTTriangle& tri = scene.triangles[closestIdx];
                    CRTVector n = tri.normal();
                    double nDotV = std::fabs(n.dot(ray.direction));
                    double intensity = ambient + diffuse * nDotV;

                    const CRTColor& base = kPalette[tri.colorIndex];
                    int pixelIndex = rowIdx * scene.imageWidth + colIdx;
                    pixels[pixelIndex] = CRTColor(
                        std::min(255, static_cast<int>(base.r * intensity)),
                        std::min(255, static_cast<int>(base.g * intensity)),
                        std::min(255, static_cast<int>(base.b * intensity))
                    );
                }
            }
        }
    };

    unsigned int threadCount = std::max(1u, std::thread::hardware_concurrency());
    std::vector<std::thread> threads;
    int rowsPerThread = (scene.imageHeight + threadCount - 1) / threadCount;

    for (unsigned int t = 0; t < threadCount; ++t) {
        int rowStart = static_cast<int>(t) * rowsPerThread;
        int rowEnd = std::min(scene.imageHeight, rowStart + rowsPerThread);
        if (rowStart >= rowEnd)
            break;
        threads.emplace_back(renderRows, rowStart, rowEnd);
    }
    for (auto& th : threads)
        th.join();
}

void writePPM(const std::string& path, const Scene& scene, const std::vector<CRTColor>& pixels) {
    std::ofstream ppmFileStream(path, std::ios::out | std::ios::binary);
    ppmFileStream << "P6\n" << scene.imageWidth << " " << scene.imageHeight << "\n255\n";
    for (const CRTColor& c : pixels) {
        unsigned char rgb[3] = {
            static_cast<unsigned char>(c.r),
            static_cast<unsigned char>(c.g),
            static_cast<unsigned char>(c.b)
        };
        ppmFileStream.write(reinterpret_cast<const char*>(rgb), 3);
    }
}

int main(int argc, char** argv) {
    std::vector<std::string> sceneFiles;

    if (argc > 1) {
        for (int i = 1; i < argc; ++i)
            sceneFiles.emplace_back(argv[i]);
    } else {
        sceneFiles = {
            "scenes/scene0.crtscene",
            "scenes/scene1.crtscene",
            "scenes/scene2.crtscene",
            "scenes/scene3.crtscene",
            "scenes/scene4.crtscene"
        };
    }

    std::filesystem::create_directories("output");

    for (const auto& sceneFile : sceneFiles) {
        Scene scene;
        if (!loadScene(sceneFile, scene)) {
            std::cerr << "Skipping " << sceneFile << std::endl;
            continue;
        }

        std::cout << "Rendering " << sceneFile
                  << " (" << scene.imageWidth << "x" << scene.imageHeight
                  << ", " << scene.triangles.size() << " triangles)..." << std::endl;

        std::vector<CRTColor> pixels;
        renderScene(scene, pixels);

        std::filesystem::path p(sceneFile);
        std::string outPath = "output/" + p.stem().string() + ".ppm";
        writePPM(outPath, scene, pixels);

        std::cout << "  -> " << outPath << std::endl;
    }

    return 0;
}

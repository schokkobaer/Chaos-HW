#pragma once
#include "raytracer/Vector.h"
#include <string>
#include <vector>

namespace crt {

enum class TextureType {
    ALBEDO,
    EDGES,
    CHECKER,
    BITMAP
};

// A named texture, referenced by materials either by name or by index into Scene::textures.
class Texture {
public:
    std::string m_name;
    TextureType m_type = TextureType::ALBEDO;

    // ALBEDO
    CRTVector m_albedo{0.0, 0.0, 0.0};

    // EDGES (uses triangle barycentric coordinates, not uv)
    CRTVector m_edgeColor{0.0, 0.0, 0.0};
    CRTVector m_innerColor{0.0, 0.0, 0.0};
    double m_edgeWidth = 0.0;

    // CHECKER (uses interpolated uv coordinates)
    CRTVector m_colorA{0.0, 0.0, 0.0};
    CRTVector m_colorB{1.0, 1.0, 1.0};
    double m_squareSize = 1.0;

    // BITMAP (uses interpolated uv coordinates)
    bool loadBitmap(const std::string& filePath);

    // uvU/uvV are interpolated per-vertex uvs; baryU/baryV/baryW are the triangle-hit barycentric coordinates.
    CRTVector sample(double uvU, double uvV, double baryU, double baryV, double baryW) const;

private:
    int m_bitmapWidth = 0;
    int m_bitmapHeight = 0;
    std::vector<unsigned char> m_bitmapPixels; // RGB, 3 bytes per pixel, row-major top-to-bottom
};

} // namespace crt

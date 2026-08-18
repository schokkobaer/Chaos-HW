#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "raytracer/Texture.h"
#include <algorithm>
#include <cmath>

namespace crt {

bool Texture::loadBitmap(const std::string& filePath) {
    int channelsInFile = 0;
    unsigned char* pixels = stbi_load(filePath.c_str(), &m_bitmapWidth, &m_bitmapHeight, &channelsInFile, 3);
    if (!pixels) {
        m_bitmapWidth = 0;
        m_bitmapHeight = 0;
        return false;
    }
    m_bitmapPixels.assign(pixels, pixels + static_cast<size_t>(m_bitmapWidth) * m_bitmapHeight * 3);
    stbi_image_free(pixels);
    return true;
}

namespace {
double wrapToUnitInterval(double value) {
    double wrapped = value - std::floor(value);
    return std::clamp(wrapped, 0.0, 1.0);
}
} // namespace

CRTVector Texture::sample(double uvU, double uvV, double baryU, double baryV, double baryW) const {
    switch (m_type) {
        case TextureType::ALBEDO:
            return m_albedo;

        case TextureType::EDGES: {
            const bool onEdge = baryU < m_edgeWidth || baryV < m_edgeWidth || baryW < m_edgeWidth;
            return onEdge ? m_edgeColor : m_innerColor;
        }

        case TextureType::CHECKER: {
            if (m_squareSize <= 0.0) {
                return m_colorA;
            }
            const int cellU = static_cast<int>(std::floor(uvU / m_squareSize));
            const int cellV = static_cast<int>(std::floor(uvV / m_squareSize));
            const bool isEven = ((cellU + cellV) % 2 + 2) % 2 == 0;
            return isEven ? m_colorA : m_colorB;
        }

        case TextureType::BITMAP: {
            if (m_bitmapWidth <= 0 || m_bitmapHeight <= 0) {
                return CRTVector(1.0, 0.0, 1.0); // magenta fallback for a failed/missing load
            }
            const double wrappedU = wrapToUnitInterval(uvU);
            const double wrappedV = wrapToUnitInterval(uvV);
            int col = static_cast<int>(wrappedU * m_bitmapWidth);
            int row = static_cast<int>((1.0 - wrappedV) * m_bitmapHeight);
            col = std::clamp(col, 0, m_bitmapWidth - 1);
            row = std::clamp(row, 0, m_bitmapHeight - 1);
            const size_t pixelIndex = (static_cast<size_t>(row) * m_bitmapWidth + col) * 3;
            return CRTVector(
                m_bitmapPixels[pixelIndex + 0] / 255.0,
                m_bitmapPixels[pixelIndex + 1] / 255.0,
                m_bitmapPixels[pixelIndex + 2] / 255.0
            );
        }
    }
    return m_albedo;
}

bool Texture::loadBitmapHDR(const std::string& filePath) {
    int channelsInFile = 0;
    float* pixels = stbi_loadf(filePath.c_str(), &m_hdrWidth, &m_hdrHeight, &channelsInFile, 3);
    if (!pixels) {
        m_hdrWidth = 0;
        m_hdrHeight = 0;
        return false;
    }
    m_hdrPixels.assign(pixels, pixels + static_cast<size_t>(m_hdrWidth) * m_hdrHeight * 3);
    stbi_image_free(pixels);
    return true;
}

CRTVector Texture::sampleHDR(double uvU, double uvV) const {
    if (m_hdrWidth <= 0 || m_hdrHeight <= 0) {
        return CRTVector(1.0, 0.0, 1.0); // magenta fallback for a failed/missing load
    }
    const double wrappedU = wrapToUnitInterval(uvU);
    const double wrappedV = wrapToUnitInterval(uvV);
    int col = static_cast<int>(wrappedU * m_hdrWidth);
    int row = static_cast<int>((1.0 - wrappedV) * m_hdrHeight);
    col = std::clamp(col, 0, m_hdrWidth - 1);
    row = std::clamp(row, 0, m_hdrHeight - 1);
    const size_t pixelIndex = (static_cast<size_t>(row) * m_hdrWidth + col) * 3;
    return CRTVector(
        m_hdrPixels[pixelIndex + 0],
        m_hdrPixels[pixelIndex + 1],
        m_hdrPixels[pixelIndex + 2]
    );
}

} // namespace crt

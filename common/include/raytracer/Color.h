#pragma once

#include <algorithm>
#include <cmath>
namespace crt
{

    // A simple 8-bit-per-channel RGB color.
    struct CRTColor
    {
        int r, g, b;

        explicit CRTColor(int r = 0, int g = 0, int b = 0) : r(r), g(g), b(b) {}
        CRTColor operator+(const CRTColor &other) const
        {
            return CRTColor(
                std::clamp(r + other.r, 0, 255),
                std::clamp(g + other.g, 0, 255),
                std::clamp(b + other.b, 0, 255));
        }
        CRTColor operator-(const CRTColor &other) const
        {
            return CRTColor(
                std::clamp(r - other.r, 0, 255),
                std::clamp(g - other.g, 0, 255),
                std::clamp(b - other.b, 0, 255));
        }

        CRTColor scaleColor(double scale) const
        {
            return CRTColor(
                std::clamp(static_cast<int>(r * scale), 0, 255),
                std::clamp(static_cast<int>(g * scale), 0, 255),
                std::clamp(static_cast<int>(b * scale), 0, 255));
        }
    };

    // Attenuates a color based on another color. Both colors should be written have values between 0 and 255.
    // Therefore the attenuation color is divided by 255 to get the attenation scale.
   inline CRTColor attenuateColor(const CRTColor &color, const CRTColor &attenuation)
    {
        return CRTColor(
            std::min(255, static_cast<int>(color.r * attenuation.r / 255)),
            std::min(255, static_cast<int>(color.g * attenuation.g / 255)),
            std::min(255, static_cast<int>(color.b * attenuation.b / 255)));
    }
    struct Radiance
    {
        double r = 0.0, g = 0.0, b = 0.0;
        Radiance operator+(const Radiance& o) const { return {r + o.r, g + o.g, b + o.b}; }
        Radiance operator*(const Radiance& o) const { return {r * o.r, g * o.g, b * o.b}; }
        Radiance scaled(double s) const { return {r * s, g * s, b * s}; }
    };

    inline Radiance toRadiance(const CRTColor& c) { return {c.r / 255.0, c.g / 255.0, c.b / 255.0}; }

     inline CRTColor toCRTColor(const Radiance& hdr)
    {
        auto channel = [](double x) {
            const double v = std::max(0.0, x);
            return std::clamp(static_cast<int>((v / (v + 1.0)) * 255.0), 0, 255); // Reinhard
        };
        return CRTColor(channel(hdr.r), channel(hdr.g), channel(hdr.b));
    }
} // namespace crt

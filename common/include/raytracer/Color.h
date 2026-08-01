#pragma once

namespace crt {

// A simple 8-bit-per-channel RGB color.
struct CRTColor {
    int r, g, b;

    explicit CRTColor(int r = 0, int g = 0, int b = 0) : r(r), g(g), b(b) {}
};

} // namespace crt

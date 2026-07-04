#include <fstream>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <ctime>

static const int imageWidth = 1920;
static const int imageHeight = 1080;
static const int maxColorComponent = 255;

struct Rect {
    int startX, startY, width, height;
    int r, g, b;
};

int main() {
    srand(static_cast<unsigned int>(time(nullptr)));

    auto randColor = [](int& r, int& g, int& b) {
        r = rand() % 256;
        g = rand() % 256;
        b = rand() % 256;
    };

    auto colorDist = [](const Rect& a, const Rect& b) {
        return std::abs(a.r - b.r) + std::abs(a.g - b.g) + std::abs(a.b - b.b);
    };

    int x;
    std::cout << "Get the number of rectangles: ";
    std::cin >> x;
    std::cout << "Your number of rectangles you want to see is " << x << std::endl;

    std::vector<Rect> rects;
    Rect first = {0, 0, imageWidth, imageHeight, 0, 0, 0};
    randColor(first.r, first.g, first.b);
    rects.push_back(first);

    // Split x-1 times to arrive at x rectangles
    for (int i = 0; i < x - 1; ++i) {
        // Find index of rectangle with largest side
        int maxIdx = 0;
        int maxDim = 0;
        for (int j = 0; j < (int)rects.size(); ++j) {
            int dim = std::max(rects[j].width, rects[j].height);
            if (dim > maxDim) {
                maxDim = dim;
                maxIdx = j;
            }
        }

        Rect rect = rects[maxIdx];
        Rect r1, r2;

        if (rect.width >= rect.height) {
            // Split vertically (cut along width)
            int half = rect.width / 2;
            r1 = {rect.startX,        rect.startY, half,              rect.height, 0, 0, 0};
            r2 = {rect.startX + half, rect.startY, rect.width - half, rect.height, 0, 0, 0};
        } else {
            // Split horizontally (cut along height)
            int half = rect.height / 2;
            r1 = {rect.startX, rect.startY,        rect.width, half,               0, 0, 0};
            r2 = {rect.startX, rect.startY + half, rect.width, rect.height - half, 0, 0, 0};
        }

        randColor(r1.r, r1.g, r1.b);
        do { randColor(r2.r, r2.g, r2.b); } while (colorDist(r1, r2) < 100);

        rects.erase(rects.begin() + maxIdx);
        rects.push_back(r1);
        rects.push_back(r2);
    }

    std::ofstream ppmFileStream("crt_output_image.ppm", std::ios::out | std::ios::binary);
    ppmFileStream << "P3\n";
    ppmFileStream << imageWidth << " " << imageHeight << "\n";
    ppmFileStream << maxColorComponent << "\n";

    for (int rowIdx = 0; rowIdx < imageHeight; ++rowIdx) {
        for (int colIdx = 0; colIdx < imageWidth; ++colIdx) {
            for (const Rect& rect : rects) {
                if (colIdx >= rect.startX && colIdx < rect.startX + rect.width &&
                    rowIdx >= rect.startY && rowIdx < rect.startY + rect.height) {
                    int pr = std::clamp(rect.r + (rand() % 101 - 50), 0, 255);
                    int pg = std::clamp(rect.g + (rand() % 101 - 50), 0, 255);
                    int pb = std::clamp(rect.b + (rand() % 101 - 50), 0, 255);
                    ppmFileStream << pr << " " << pg << " " << pb << "\t";
                    break;
                }
            }
        }
        ppmFileStream << "\n";
    }

    ppmFileStream.close();
    return 0;
}
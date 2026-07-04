#include <fstream>
#include <iostream>
#include <cmath>
#include <algorithm>

static const int imageWidth  = 1920;
static const int imageHeight = 1080;
static const int maxColorComponent = 255;

// Background and shape colors
static const int bgR = 30,  bgG = 30,  bgB = 30;
static const int shR = 255, shG = 220, shB = 50;

static const int minDist = 50; // minimum pixel distance between defining points

bool isOnBorder(int col, int row) {
    return col == 0 || col == imageWidth - 1 || row == 0 || row == imageHeight - 1;
}

bool insideCircle(int col, int row, int cx, int cy, int radius) {
    int dx = col - cx, dy = row - cy;
    return dx * dx + dy * dy < radius * radius;
}

bool insideRect(int col, int row, int x1, int y1, int x2, int y2) {
    return col >= std::min(x1, x2) && col <= std::max(x1, x2) &&
           row >= std::min(y1, y2) && row <= std::max(y1, y2);
}

// Returns the signed area component of edge (A->B) relative to point P
float edgeSign(float ax, float ay, float bx, float by, float px, float py) {
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

bool insideTriangle(int col, int row, int x0, int y0, int x1, int y1, int x2, int y2) {
    float d0 = edgeSign(x0, y0, x1, y1, col, row);
    float d1 = edgeSign(x1, y1, x2, y2, col, row);
    float d2 = edgeSign(x2, y2, x0, y0, col, row);
    bool hasNeg = (d0 < 0) || (d1 < 0) || (d2 < 0);
    bool hasPos = (d0 > 0) || (d1 > 0) || (d2 > 0);
    return !(hasNeg && hasPos);
}

float dist(int ax, int ay, int bx, int by) {
    float dx = ax - bx, dy = ay - by;
    return std::sqrt(dx * dx + dy * dy);
}

int main() {
    std::cout << "Choose a shape:\n";
    std::cout << "  1 - Circle\n";
    std::cout << "  2 - Rectangle\n";
    std::cout << "  3 - Triangle\n";
    std::cout << "Choice: ";
    int choice;
    std::cin >> choice;

    int cx, cy, radius;
    int rx1, ry1, rx2, ry2;
    int tx0, ty0, tx1, ty1, tx2, ty2;

    if (choice == 1) {
        std::cout << "Center (cx cy): ";
        std::cin >> cx >> cy;
        std::cout << "Radius in pixels (min " << minDist << "): ";
        std::cin >> radius;
        while (radius < minDist) {
            std::cout << "Radius too small, must be at least " << minDist << ": ";
            std::cin >> radius;
        }
    } else if (choice == 2) {
        std::cout << "Two opposite corners, each pair at least " << minDist << "px apart on each axis.\n";
        do {
            std::cout << "Corner 1 (x1 y1): ";
            std::cin >> rx1 >> ry1;
            std::cout << "Corner 2 (x2 y2): ";
            std::cin >> rx2 >> ry2;
            if (std::abs(rx2 - rx1) < minDist || std::abs(ry2 - ry1) < minDist)
                std::cout << "Corners too close! Try again.\n";
        } while (std::abs(rx2 - rx1) < minDist || std::abs(ry2 - ry1) < minDist);
    } else {
        std::cout << "Three vertices, each pair at least " << minDist << "px apart.\n";
        bool tooClose = true;
        while (tooClose) {
            std::cout << "Vertex 0 (x y): "; std::cin >> tx0 >> ty0;
            std::cout << "Vertex 1 (x y): "; std::cin >> tx1 >> ty1;
            std::cout << "Vertex 2 (x y): "; std::cin >> tx2 >> ty2;
            tooClose = dist(tx0, ty0, tx1, ty1) < minDist ||
                       dist(tx1, ty1, tx2, ty2) < minDist ||
                       dist(tx0, ty0, tx2, ty2) < minDist;
            if (tooClose)
                std::cout << "Vertices too close! Try again.\n";
        }
    }

    std::ofstream ppmFileStream("crt_output_image.ppm", std::ios::out | std::ios::binary);
    ppmFileStream << "P3\n" << imageWidth << " " << imageHeight << "\n" << maxColorComponent << "\n";

    for (int rowIdx = 0; rowIdx < imageHeight; ++rowIdx) {
        for (int colIdx = 0; colIdx < imageWidth; ++colIdx) {
            bool inside = false;
            if (!isOnBorder(colIdx, rowIdx)) {
                if      (choice == 1) inside = insideCircle  (colIdx, rowIdx, cx, cy, radius);
                else if (choice == 2) inside = insideRect    (colIdx, rowIdx, rx1, ry1, rx2, ry2);
                else                  inside = insideTriangle(colIdx, rowIdx, tx0, ty0, tx1, ty1, tx2, ty2);
            }
            if (inside)
                ppmFileStream << shR << " " << shG << " " << shB << "\t";
            else
                ppmFileStream << bgR << " " << bgG << " " << bgB << "\t";
        }
        ppmFileStream << "\n";
    }

    ppmFileStream.close();
    return 0;
}

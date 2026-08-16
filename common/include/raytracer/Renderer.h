#pragma once

#include <vector>
#include "raytracer/Scene.h"

namespace crt
{
    constexpr double kShadowEpsilon = 1e-6;
    constexpr int kMaxRayDepth = 10;
    inline const CRTColor kLightColor(255, 230, 40);

    //Traces a ray through the scene and returns the color of a given pixel.
    crt::CRTColor traceRay(const Scene& scene, Ray& ray, int startRayDepth);

    //Renders the view of the scene from the camer point of view
    void renderScene(const Scene& scene, std::vector<CRTColor>& pixels);

}
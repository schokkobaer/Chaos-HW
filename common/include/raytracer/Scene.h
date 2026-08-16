#pragma once

#include "raytracer/Color.h"
#include "raytracer/Light.h"
#include "raytracer/Triangle.h"
#include "raytracer/Camera.h"
#include "raytracer/Object.h"
#include "raytracer/Material.h"

namespace crt
{
    struct Scene
    {
        int imageWidth = 1920;
        int imageHeight = 1080;
        CRTColor backgroundColor;
        Camera camera;
        std::vector<CRTLight> lights;
        std::vector<Object> objects;
        std::vector<crt::Material> materials;
    };

    struct HitRecord
    {
        double t = 0.0;
        CRTVector position;
        CRTVector normal;
        double u = 0.0;
        double v = 0.0;
        size_t objectIndex = 0;
    };

}

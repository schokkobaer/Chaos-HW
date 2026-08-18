#pragma once

#include "raytracer/Color.h"
#include "raytracer/Light.h"
#include "raytracer/Triangle.h"
#include "raytracer/Camera.h"
#include "raytracer/Object.h"
#include "raytracer/Material.h"
#include "raytracer/Texture.h"
#include "AccelerationTree.h"

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
        std::vector<crt::Texture> textures;
        int environmentTextureIndex = -1; // index into textures; -1 means use the procedural sky gradient
        AccelerationTree accelerationTree;
    };

    struct HitRecord
    {
        double t = 0.0;
        CRTVector position;
        CRTVector normal;
        double u = 0.0;
        double v = 0.0;
        double texU = 0.0;
        double texV = 0.0;
        size_t objectIndex = 0;
    };

}

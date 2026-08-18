#include "raytracer/Renderer.h"
#include <atomic>
#include <limits>
#include <thread>
#include <iostream>
#include <random>

namespace crt
{
    double composedLightForHit(const Scene &scene, const HitRecord &hit);
    double lightContributionForHit(const Scene &scene, const HitRecord &hit, const CRTLight &sceneLight);
    Radiance shadeHitRadiance(const Scene &scene, const HitRecord &hit);
    Radiance traceRayRadiance(const Scene &scene, Ray &ray, int startRayDepth);

    bool findClosestHit(const Scene &scene, const Ray &ray, HitRecord &hit)
    {
        double closestT = std::numeric_limits<double>::max();
        size_t closestTriangleIdx = 0;
        size_t closestObjectIdx = 0;
        double u = 0.0;
        double v = 0.0;
        bool hitFound = false;
        for (size_t objIdx = 0; objIdx < scene.objects.size(); ++objIdx)
        {
            const Object &object = scene.objects[objIdx];
            if (!object.m_boundingBox.intersects(ray))
            {
                continue;
            }

            for (size_t triIdx = 0; triIdx < object.m_triangles.size(); ++triIdx)
            {
                const std::optional<crt::TriangleHit> triangleHit = object.m_triangles[triIdx].intersect(ray);
                if (triangleHit.has_value())
                {

                    if (triangleHit->t < closestT)
                    {
                        hitFound = true;
                        closestT = triangleHit->t;
                        closestTriangleIdx = triIdx;
                        closestObjectIdx = objIdx;
                        u = triangleHit->u;
                        v = triangleHit->v;
                    }
                }
            }
        }
        if (!hitFound)
        {
            return false;
        }
        bool localIsSmoothShadingUsed = false;
        const int matIdx = scene.objects[closestObjectIdx].m_materialIndex;
        if (matIdx >= 0 && matIdx < static_cast<int>(scene.materials.size()))
        {
            localIsSmoothShadingUsed = scene.materials[matIdx].m_smoothShading;
        }
        const CRTTriangle &triangle = scene.objects[closestObjectIdx].m_triangles[closestTriangleIdx];
        hit.t = closestT;
        hit.position = ray.origin + ray.direction * closestT;
        if (localIsSmoothShadingUsed)
        {
            CRTVector interpolatedNormal = triangle.n0 * (1.0 - u - v) + triangle.n1 * u + triangle.n2 * v;
            hit.normal = interpolatedNormal.normalized();
        }
        else
        {
            hit.normal = triangle.normal();
        }
        // hit.normal = triangle.normal(); // This line is redundant and should be removed
        hit.u = u;
        hit.v = v;
        hit.texU = triangle.uv0.x * (1.0 - u - v) + triangle.uv1.x * u + triangle.uv2.x * v;
        hit.texV = triangle.uv0.y * (1.0 - u - v) + triangle.uv1.y * u + triangle.uv2.y * v;
        hit.objectIndex = closestObjectIdx;
        return true;
    }

    // Same job as findClosestHit, but searches scene.accelerationTree instead of walking
    // scene.objects directly. Kept separate so findClosestHit (and everything that calls it)
    // is untouched until this is verified to produce the same results.
    bool findClosestHitViaTree(const Scene &scene, const Ray &ray, HitRecord &hit)
    {
        const std::optional<AccelerationHit> accHit = scene.accelerationTree.intersectClosest(ray);
        if (!accHit.has_value())
        {
            return false;
        }

        const CRTTriangle &triangle = accHit->triangle;
        bool localIsSmoothShadingUsed = false;
        const int matIdx = scene.objects[triangle.objectIndex].m_materialIndex;
        if (matIdx >= 0 && matIdx < static_cast<int>(scene.materials.size()))
        {
            localIsSmoothShadingUsed = scene.materials[matIdx].m_smoothShading;
        }

        hit.t = accHit->t;
        hit.position = ray.origin + ray.direction * accHit->t;
        if (localIsSmoothShadingUsed)
        {
            CRTVector interpolatedNormal = triangle.n0 * (1.0 - accHit->u - accHit->v) + triangle.n1 * accHit->u + triangle.n2 * accHit->v;
            hit.normal = interpolatedNormal.normalized();
        }
        else
        {
            hit.normal = triangle.normal();
        }
        hit.u = accHit->u;
        hit.v = accHit->v;
        hit.texU = triangle.uv0.x * (1.0 - accHit->u - accHit->v) + triangle.uv1.x * accHit->u + triangle.uv2.x * accHit->v;
        hit.texV = triangle.uv0.y * (1.0 - accHit->u - accHit->v) + triangle.uv1.y * accHit->u + triangle.uv2.y * accHit->v;
        hit.objectIndex = static_cast<size_t>(triangle.objectIndex);
        return true;
    }

    // Resolves a material's color at a hit point, sampling its texture if one is referenced.
    CRTVector resolveAlbedo(const Scene &scene, const Material &material, const HitRecord &hit)
    {
        if (material.m_textureIndex >= 0 && material.m_textureIndex < static_cast<int>(scene.textures.size()))
        {
            const double baryW = 1.0 - hit.u - hit.v;
            return scene.textures[material.m_textureIndex].sample(hit.texU, hit.texV, hit.u, hit.v, baryW);
        }
        return CRTVector(material.m_albedo[0], material.m_albedo[1], material.m_albedo[2]);
    }

    Radiance shadeHitRadiance(const Scene &scene, const HitRecord &hit)
    {
        if (scene.objects[hit.objectIndex].m_isEmissive)
        {
            return toRadiance(kLightColor);
        }


        const double lightFactor = composedLightForHit(scene, hit);
        if (lightFactor <= 0.0)
        {
            return Radiance{};
        }

        const Object &obj = scene.objects[hit.objectIndex];
        bool objectHasValidMaterial = obj.m_materialIndex >= 0 && obj.m_materialIndex < static_cast<int>(scene.materials.size());

        double r_albedo{1};
        double g_albedo{1};
        double b_albedo{1};

        if (objectHasValidMaterial)
        {
            const CRTVector albedo = resolveAlbedo(scene, scene.materials[obj.m_materialIndex], hit);
            r_albedo = std::clamp(albedo.x, 0.0, 1.0);
            g_albedo = std::clamp(albedo.y, 0.0, 1.0);
            b_albedo = std::clamp(albedo.z, 0.0, 1.0);
        }
        return Radiance{r_albedo * lightFactor, g_albedo * lightFactor, b_albedo * lightFactor};
    }

    double composedLightForHit(const Scene &scene, const HitRecord &hit)
    {
        double lightSum = 0.0;
        if (scene.lights.empty())
        {
            std::cerr << "Warning: scene has no lights, using default light factor of 1.0" << std::endl;
            return 1.0;
        }
        for (const CRTLight &sceneLight : scene.lights)
        {
            lightSum += lightContributionForHit(scene, hit, sceneLight);
        }
        return lightSum;
    }

    double lightContributionForHit(const Scene &scene, const HitRecord &hit, const CRTLight &sceneLight)
    {
        constexpr double kPi = std::numbers::pi_v<double>;

        const CRTVector toLight = sceneLight.getPosition() - hit.position;
        const double distanceToLight = toLight.length();
        if (distanceToLight <= 0.0)
        {
            return 0.0;
        }

        const CRTVector lightDir = toLight * (1.0 / distanceToLight);
        const double cosLaw = std::max(0.0, hit.normal.dot(lightDir));
        if (cosLaw <= 0.0)
        {
            return 0.0;
        }

        const Ray shadowRay(hit.position + lightDir * kShadowEpsilon, lightDir);
        // Emissive/refractive owners are filtered via CRTTriangle::castsShadow, set at flatten time.
        if (scene.accelerationTree.intersectAny(shadowRay, distanceToLight - kShadowEpsilon))
        {
            return 0.0;
        }
        const double sphereArea = 4.0 * kPi * distanceToLight * distanceToLight;
        return (sceneLight.getIntensity() / sphereArea) * cosLaw;
    }

    Radiance traceRayRadiance(const Scene &scene, Ray &ray, int startRayDepth)
    {
        Radiance outputColor{};
        Radiance attenuation{1.0, 1.0, 1.0};
        while (startRayDepth < kMaxRayDepth)
        {

            HitRecord hit;
            if (!findClosestHitViaTree(scene, ray, hit))
            {
                // Ray escaped to background — apply accumulated attenuation
                outputColor = outputColor + toRadiance(scene.backgroundColor) * attenuation;
                return outputColor;
            }

            bool objectHasValidMaterial = scene.objects[hit.objectIndex].m_materialIndex >= 0 && scene.objects[hit.objectIndex].m_materialIndex < static_cast<int>(scene.materials.size());
            // Reflective material
            if (objectHasValidMaterial && scene.materials[scene.objects[hit.objectIndex].m_materialIndex].m_type == crt::MaterialType::REFLECTIVE)
            {
                if (ray.direction.dot(hit.normal) > 0.0)
                {
                    // Back face of one-sided mirror: shade as diffuse instead of reflecting
                    outputColor = outputColor + shadeHitRadiance(scene, hit) * attenuation;
                    return outputColor;
                }
                // Reflective material: ray hits the fron face fo the mirror, calculating the reflection
                const crt::Material &mat = scene.materials[scene.objects[hit.objectIndex].m_materialIndex];
                const CRTVector reflectiveAlbedo = resolveAlbedo(scene, mat, hit);
                attenuation = attenuation * Radiance{reflectiveAlbedo.x, reflectiveAlbedo.y, reflectiveAlbedo.z};
                ray.direction = ray.direction - hit.normal * (ray.direction.dot(hit.normal) * 2.0);
                ray.origin = hit.position + ray.direction * kShadowEpsilon;
                startRayDepth++;
                continue;
            }

            // Refractive material
            if (objectHasValidMaterial && scene.materials[scene.objects[hit.objectIndex].m_materialIndex].m_type == crt::MaterialType::REFRACTIVE)
            {

                CRTVector n = hit.normal;

                const crt::Material &mat = scene.materials[scene.objects[hit.objectIndex].m_materialIndex];
                double indexOfRefractionIn{1.0};
                double indexOfRefractionOut{mat.getIndexOfRefraction().value_or(1.0)};
                double cosThetaIn = -n.dot(ray.direction);
                // Ray is hitting from the inside of the material, so we need to swap the indexes.
                if (cosThetaIn < 0.0)
                {
                    std::swap(indexOfRefractionIn, indexOfRefractionOut);
                    n = n * -(1.0);
                    cosThetaIn = -cosThetaIn;
                }
                const double eta = indexOfRefractionIn / indexOfRefractionOut;
                const CRTVector refractiveAlbedo = resolveAlbedo(scene, mat, hit);
                attenuation = attenuation * Radiance{refractiveAlbedo.x, refractiveAlbedo.y, refractiveAlbedo.z};
                const double sinThetaout = eta * std::sqrt(std::max(0.0, 1.0 - cosThetaIn * cosThetaIn));

                double fresneleWeighting = 1.0;
                if (sinThetaout <= 1.0)
                {
                    const double r0 = (indexOfRefractionIn - indexOfRefractionOut) / (indexOfRefractionIn + indexOfRefractionOut);
                    fresneleWeighting = r0 * r0 + (1.0 - r0 * r0) * std::pow(1.0 - cosThetaIn, 5.0);

                    const double cosThetaOut = std::sqrt(std::max(0.0, 1 - sinThetaout * sinThetaout));
                    const CRTVector refractionDirection = n * (-cosThetaOut) + (ray.direction + n * cosThetaIn).normalized() * sinThetaout;
                    const CRTVector refractionOrigin = hit.position - n * kShadowEpsilon;
                    crt::Ray refractedRay(refractionOrigin, refractionDirection);

                    const Radiance refractedColor = traceRayRadiance(scene, refractedRay, startRayDepth + 1);
                    outputColor = outputColor + (refractedColor * attenuation).scaled(1.0 - fresneleWeighting);
                }

                // Reflection continuation (also handles TIR, where fresneleWeighting == 1.0)
                attenuation = attenuation.scaled(fresneleWeighting);
                ray.direction = ray.direction - n * (ray.direction.dot(n) * 2.0);
                ray.origin = hit.position + n * kShadowEpsilon;
                startRayDepth++;
                continue;
            }
            // Diffuse material or no material: shade and terminate the ray
            outputColor = outputColor + shadeHitRadiance(scene, hit) * attenuation;
            return outputColor;
        }
        // Should reach this point only numbers of ray depth is exceeded.
        return outputColor;
    }

    crt::CRTColor traceRay(const Scene &scene, Ray &ray, int startRayDepth)
    {
        return toCRTColor(traceRayRadiance(scene, ray, startRayDepth));
    }

    void renderRegion(const Scene &scene, std::vector<CRTColor> &pixels, int rowIdx, int colIdx, int rHeight, int rWidth)
    {
        constexpr int kSamplesPerPixel = 1;
        const double aspectRatio = static_cast<double>(scene.imageWidth) / static_cast<double>(scene.imageHeight);

        // Per-region RNG so jitter doesn't need synchronization across regions.
        std::mt19937 rng(static_cast<unsigned int>(rowIdx) * 9781u + static_cast<unsigned int>(colIdx) * 131u + 1u);
        std::uniform_real_distribution<double> jitter(0.0, 1.0);

        const int rowEnd = std::min(scene.imageHeight, rowIdx + rHeight);
        const int colEnd = std::min(scene.imageWidth, colIdx + rWidth);

        for (int row = rowIdx; row < rowEnd; ++row)
        {
            for (int col = colIdx; col < colEnd; ++col)
            {
                const int pixelIndex = row * scene.imageWidth + col;
                Radiance accumulated{};
                for (int sample = 0; sample < kSamplesPerPixel; ++sample)
                {
                    const double subX = jitter(rng);
                    const double subY = jitter(rng);
                    crt::Ray cameraRay = scene.camera.generateRayForPixel(
                        col,
                        row,
                        scene.imageWidth,
                        scene.imageHeight,
                        aspectRatio,
                        subX,
                        subY);
                    accumulated = accumulated + traceRayRadiance(scene, cameraRay, 0);
                }
                pixels[pixelIndex] = toCRTColor(accumulated.scaled(1.0 / kSamplesPerPixel));
            }
        }
    }

    void renderScene(const Scene &scene, std::vector<CRTColor> &pixels)
    {
        constexpr int kBucketSize = 8;
        pixels.assign(static_cast<size_t>(scene.imageWidth) * static_cast<size_t>(scene.imageHeight), scene.backgroundColor);

        struct Bucket
        {
            int rowIdx;
            int colIdx;
        };
        std::vector<Bucket> buckets;
        for (int rowIdx = 0; rowIdx < scene.imageHeight; rowIdx += kBucketSize)
        {
            for (int colIdx = 0; colIdx < scene.imageWidth; colIdx += kBucketSize)
            {
                buckets.push_back({rowIdx, colIdx});
            }
        }

        // Shared work queue so threads that finish cheap buckets (sky, background) pick up more work
        // instead of sitting idle while another thread churns through an expensive bucket (reflections/refractions).
        std::atomic<size_t> nextBucketIdx{0};
        auto worker = [&]()
        {
            size_t bucketIdx;
            while ((bucketIdx = nextBucketIdx.fetch_add(1, std::memory_order_relaxed)) < buckets.size())
            {
                const Bucket &bucket = buckets[bucketIdx];
                renderRegion(scene, pixels, bucket.rowIdx, bucket.colIdx, kBucketSize, kBucketSize);
            }
        };

        const unsigned int threadCount = std::max(1u, std::thread::hardware_concurrency());
        std::vector<std::thread> threads;
        threads.reserve(threadCount);
        for (unsigned int threadIdx = 0; threadIdx < threadCount; ++threadIdx)
        {
            threads.emplace_back(worker);
        }
        for (std::thread &t : threads)
        {
            t.join();
        }
    }
}
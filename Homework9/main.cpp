#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numbers>
#include <string>
#include <thread>
#include <vector>
#include<limits> //For std::numeric_limits
#include "raytracer/Color.h"
#include "raytracer/Light.h"
#include "raytracer/Triangle.h"
#include "raytracer/Camera.h"
#include "raytracer/Object.h"
#include "raytracer/Material.h"
#include "rapidjson/document.h"
#include "rapidjson/istreamwrapper.h"

namespace {

using crt::CRTTriangle;
using crt::CRTVector;
using crt::CRTColor;
using crt::CRTLight;
using crt::Camera;
using crt::Ray;
using crt::Object;

constexpr double kShadowEpsilon = 1e-6;
constexpr int kMaxRayDepth = 5;


struct Scene {
	int imageWidth = 1920;
	int imageHeight = 1080;
	CRTColor backgroundColor;
	Camera camera;
	std::vector<CRTLight> lights;
	std::vector<Object> objects;
	std::vector<crt::Material> materials;
};

struct HitRecord {
	double t = 0.0;
	CRTVector position;
	CRTVector normal;
	double u = 0.0;
	double v = 0.0;
	size_t objectIndex = 0;
};



static CRTColor colorFromUnitFloats(const rapidjson::Value& arr) 
{
	auto clamp255 = [](double c) {
		const int v = static_cast<int>(std::round(c * 255.0));
		return std::max(0, std::min(255, v));
	};
	return CRTColor(clamp255(arr[0].GetDouble()), clamp255(arr[1].GetDouble()), clamp255(arr[2].GetDouble()));
}

static const CRTColor kLightColor(255, 230, 40);

void appendSphereMesh(crt::Object& emptyLightObject,  const CRTVector& center, double radius, int stacks, int slices)
{
	constexpr double pi = std::numbers::pi_v<double>;
	emptyLightObject.m_triangles.reserve(2 * slices * (stacks - 1));
	auto spherePoint = [&](double theta, double phi) 
	{
		const double sinTheta = std::sin(theta);
		return CRTVector(
			center.x + radius * sinTheta * std::cos(phi),
			center.y + radius * std::cos(theta),
			center.z + radius * sinTheta * std::sin(phi)
		);
	};

	for (int stack = 0; stack < stacks; ++stack) 
	{
		const double theta0 = pi * static_cast<double>(stack) / static_cast<double>(stacks);
		const double theta1 = pi * static_cast<double>(stack + 1) / static_cast<double>(stacks);

		for (int slice = 0; slice < slices; ++slice) {
			const double phi0 = 2.0 * pi * static_cast<double>(slice) / static_cast<double>(slices);
			const double phi1 = 2.0 * pi * static_cast<double>(slice + 1) / static_cast<double>(slices);

			const CRTVector p00 = spherePoint(theta0, phi0);
			const CRTVector p01 = spherePoint(theta0, phi1);
			const CRTVector p10 = spherePoint(theta1, phi0);
			const CRTVector p11 = spherePoint(theta1, phi1);

			if (stack != 0) {
				emptyLightObject.m_triangles.emplace_back(p00, p10, p01);
			}
			if (stack != stacks - 1) {
				emptyLightObject.m_triangles.emplace_back(p01, p10, p11);
			}
		}
	}
	return;
}

void addLightSpheres(Scene& scene, double radius = 0.14, int stacks = 14, int slices = 24)
{
	scene.objects.reserve(scene.objects.size() + scene.lights.size());
	for (const CRTLight& sceneLight : scene.lights) 
	{
		Object& lightObject = scene.objects.emplace_back();
		lightObject.m_isEmissive = true;
		appendSphereMesh(lightObject, sceneLight.getPosition(), radius, stacks, slices);
	}
}

bool findClosestHit(const Scene& scene, const Ray& ray, HitRecord& hit) 
{
	double closestT = std::numeric_limits<double>::max();
	size_t closestTriangleIdx =0;
	size_t closestObjectIdx = 0;
	double u = 0.0;
	double v = 0.0;
	bool hitFound = false;
	for(size_t objIdx = 0; objIdx < scene.objects.size(); ++objIdx) 
	{
		const Object& object = scene.objects[objIdx];
	
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
	if (matIdx >= 0 && matIdx < static_cast<int>(scene.materials.size())) {
		localIsSmoothShadingUsed = scene.materials[matIdx].m_smoothShading;
	}
	const CRTTriangle& triangle = scene.objects[closestObjectIdx].m_triangles[closestTriangleIdx];
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
	hit.objectIndex = closestObjectIdx;
	return true;
}


double lightContributionForHit(const Scene& scene, const HitRecord& hit, const CRTLight& sceneLight) {
	constexpr double kPi = std::numbers::pi_v<double>;

	const CRTVector toLight = sceneLight.getPosition() - hit.position;
	const double distanceToLight = toLight.length();
	if (distanceToLight <= 0.0) {
		return 0.0;
	}

	const CRTVector lightDir = toLight * (1.0 / distanceToLight);
	const double cosLaw = std::max(0.0, hit.normal.dot(lightDir));
	if (cosLaw <= 0.0) {
		return 0.0;
	}

	const Ray shadowRay(hit.position + lightDir * kShadowEpsilon, lightDir);
	for (const Object& object : scene.objects) {
		if (object.m_isEmissive) {
			continue;
		}

		for (const CRTTriangle& triangle : object.m_triangles) {
			const std::optional<crt::TriangleHit> shadowHit = triangle.intersect(shadowRay);
			if (shadowHit.has_value() && shadowHit->t < distanceToLight - kShadowEpsilon) {
				return 0.0;
			}
		}
	}
	const double sphereArea = 4.0 * kPi * distanceToLight * distanceToLight;
	return (sceneLight.getIntensity() / sphereArea) * cosLaw;
}

double composedLightForHit(const Scene& scene, const HitRecord& hit) {
	double lightSum = 0.0;
	if (scene.lights.empty()) {
		std::cerr << "Warning: scene has no lights, using default light factor of 1.0" << std::endl;	
		return 1.0;
	}
	for (const CRTLight& sceneLight : scene.lights) {
		lightSum += lightContributionForHit(scene, hit, sceneLight);
	}
	return lightSum;
}

CRTColor shadeHit(const Scene& scene, const HitRecord& hit) {
	if (scene.objects[hit.objectIndex].m_isEmissive) {
		return kLightColor;
	}

	const double lightFactor = composedLightForHit(scene, hit);
	if (lightFactor <= 0.0) {
		return CRTColor(0, 0, 0);
	}

	const Object& obj = scene.objects[hit.objectIndex];
	bool objectHasValidMaterial = obj.m_materialIndex >=0 && obj.m_materialIndex < static_cast<int>(scene.materials.size());
	
	double r_albedo{1};
	double g_albedo{1};
	double b_albedo{1};
	
	if (objectHasValidMaterial)
	{
		r_albedo = std::clamp(scene.materials[obj.m_materialIndex].m_albedo[0] , 0.0, 1.0);
		g_albedo = std::clamp(scene.materials[obj.m_materialIndex].m_albedo[1] , 0.0, 1.0);
		b_albedo = std::clamp(scene.materials[obj.m_materialIndex].m_albedo[2] , 0.0, 1.0);
	}
	return CRTColor(
		std::min(255, static_cast<int>(255 * r_albedo * lightFactor)),
		std::min(255, static_cast<int>(255 * g_albedo * lightFactor)),
		std::min(255, static_cast<int>(255 * b_albedo * lightFactor))
	);
}

bool loadScene(const std::string& path, Scene& scene) {
	std::ifstream fileStream(path);
	if (!fileStream.is_open()) {
		std::cerr << "Failed to open scene file: " << path << std::endl;
		return false;
	}

	rapidjson::IStreamWrapper isw(fileStream);
	rapidjson::Document doc;
	doc.ParseStream(isw);
	if (doc.HasParseError()) {
		std::cerr << "Failed to parse JSON in: " << path << std::endl;
		return false;
	}

	if (doc.HasMember("settings")) {
		const auto& settings = doc["settings"];
		if (settings.HasMember("background_color")) {
			scene.backgroundColor = colorFromUnitFloats(settings["background_color"]);
		}
		if (settings.HasMember("image_settings")) {
			const auto& imageSettings = settings["image_settings"];
			scene.imageWidth = imageSettings["width"].GetInt();
			scene.imageHeight = imageSettings["height"].GetInt();
		}
	}

	if (doc.HasMember("camera")) {
		const auto& cameraNode = doc["camera"];
		if (cameraNode.HasMember("matrix")) {
			const auto& m = cameraNode["matrix"];
			double matrix[9];
			for (int i = 0; i < 9; ++i) {
				matrix[i] = m[i].GetDouble();
			}

			// Scene files store camera basis in the transposed layout compared to
			// Camera::applyMatrix, so transpose before applying.
			double transposed[9] = {
				matrix[0], matrix[3], matrix[6],
				matrix[1], matrix[4], matrix[7],
				matrix[2], matrix[5], matrix[8]
			};
			scene.camera.applyMatrix(transposed);
		}
		if (cameraNode.HasMember("position")) {
			const auto& p = cameraNode["position"];
			scene.camera.origin = CRTVector(p[0].GetDouble(), p[1].GetDouble(), p[2].GetDouble());
		}
	}

	if (doc.HasMember("lights")) {
		const auto& lights = doc["lights"];
		for (rapidjson::SizeType lightIdx = 0; lightIdx < lights.Size(); ++lightIdx) {
			const auto& light = lights[lightIdx];
			if (!light.HasMember("position")) {
				continue;
			}
			const auto& p = light["position"];
			const double intensity = light.HasMember("intensity") ? light["intensity"].GetDouble() : 1.0;
			scene.lights.push_back(CRTLight(CRTVector(p[0].GetDouble(), p[1].GetDouble(), p[2].GetDouble()), intensity));
		}
	}

	if (doc.HasMember("materials")) {
    const auto& mats = doc["materials"];
    for (rapidjson::SizeType i = 0; i < mats.Size(); ++i) {
        const auto& mat = mats[i];
        double albedoValues[3] = {0.0, 0.0, 0.0};
		bool smoothShadingFlag{false};
		crt::MaterialType materialType = crt::MaterialType::DIFFUSE;
        if (mat.HasMember("albedo")) {
           	const auto& albedoArray = mat["albedo"];
			albedoValues[0] = albedoArray[0].GetDouble();
			albedoValues[1] = albedoArray[1].GetDouble();
			albedoValues[2] = albedoArray[2].GetDouble();
        }
        if (mat.HasMember("smooth_shading")) {
            smoothShadingFlag = mat["smooth_shading"].GetBool();
        }
		if (mat.HasMember("type")) {
			    const std::string typeStr = mat["type"].GetString();
				if (typeStr == "diffuse") {
					materialType = crt::MaterialType::DIFFUSE;
				} else if (typeStr == "reflective") {
					materialType = crt::MaterialType::REFLECTIVE;
				} else if (typeStr == "refractive") {
					materialType = crt::MaterialType::REFRACTIVE;
				}
		}
		crt::Material material(albedoValues, materialType, smoothShadingFlag);
        scene.materials.push_back(material);
    }
	}
	if (doc.HasMember("objects")) {
		const auto& objects = doc["objects"];
		for (rapidjson::SizeType objIdx = 0; objIdx < objects.Size(); ++objIdx) {
			const auto& obj = objects[objIdx];
			Object& newObject = scene.objects.emplace_back();
			if (obj.HasMember("material_index")) {
				const int materialIndex = obj["material_index"].GetInt();
				newObject.m_materialIndex = materialIndex;
			}
			bool smoothShadingFlag = false;
			if(newObject.m_materialIndex>=0 &&scene.materials[newObject.m_materialIndex].m_smoothShading)
			{
				smoothShadingFlag = true;
			}
			
			std::vector<CRTVector> vertices;
			const auto& verts = obj["vertices"];
			vertices.reserve(verts.Size() / 3);
			// Zero-initialised accumulation buffer for area-weighted vertex normals.
			std::vector<CRTVector> vertexNormals(verts.Size() / 3, CRTVector(0, 0, 0));

			for (rapidjson::SizeType i = 0; i + 2 < verts.Size(); i += 3) {
				vertices.emplace_back(
					verts[i].GetDouble(),
					verts[i + 1].GetDouble(),
					verts[i + 2].GetDouble()
				);
			}

			const auto& tri = obj["triangles"];
			for (rapidjson::SizeType i = 0; i + 2 < tri.Size(); i += 3)
			{
				const int i0 = tri[i].GetInt();
				const int i1 = tri[i + 1].GetInt();
				const int i2 = tri[i + 2].GetInt();
				newObject.m_triangles.emplace_back(vertices[i0], vertices[i1], vertices[i2]);
				if (smoothShadingFlag)
				{
					crt::CRTVector faceNormal= newObject.m_triangles.back().getNormalVector();
					vertexNormals[i0] = vertexNormals[i0] + faceNormal;
					vertexNormals[i1] = vertexNormals[i1] + faceNormal;
					vertexNormals[i2] = vertexNormals[i2] + faceNormal;
				}
			}
			if (smoothShadingFlag)
			{
				// Resetting the length of the vertex normals to 1.0 for smooth shading.
				for (crt::CRTVector &normal : vertexNormals)
				{
					normal = normal.normalized();
				}
				//Assigning the computed vertices to the triangles for smooth shading.
				for (rapidjson::SizeType i = 0; i + 2 < tri.Size(); i += 3)
				{
					const int i0 = tri[i].GetInt();
					const int i1 = tri[i + 1].GetInt();
					const int i2 = tri[i + 2].GetInt();
					newObject.m_triangles[i / 3].setVertexNormals(vertexNormals[i0], vertexNormals[i1], vertexNormals[i2]);
				}

			}
		}
	}

	addLightSpheres(scene);
	return true;
}

bool smoothShadingMaterialIsUsed(const Scene& scene) {
	for (const Object& obj : scene.objects) {
		if (obj.m_materialIndex >= 0 && obj.m_materialIndex < static_cast<int>(scene.materials.size())) {
			const crt::Material& material = scene.materials[obj.m_materialIndex];
			if (material.m_smoothShading) {
				return true;
			}
		}
	}
	return false;
}
void renderScene(const Scene& scene, std::vector<CRTColor>& pixels) {
	const double aspectRatio = static_cast<double>(scene.imageWidth) / static_cast<double>(scene.imageHeight);
	pixels.assign(static_cast<size_t>(scene.imageWidth) * static_cast<size_t>(scene.imageHeight), scene.backgroundColor);

	auto renderRows = [&](int rowStart, int rowEnd) {
		for (int rowIdx = rowStart; rowIdx < rowEnd; ++rowIdx) {
			for (int colIdx = 0; colIdx < scene.imageWidth; ++colIdx) {
				const double xRaster = static_cast<double>(colIdx) + 0.5;
				const double yRaster = static_cast<double>(rowIdx) + 0.5;

				const double xNdc = xRaster / static_cast<double>(scene.imageWidth);
				const double yNdc = yRaster / static_cast<double>(scene.imageHeight);

				double xScreen = (2.0 * xNdc) - 1.0;
				const double yScreen = 1.0 - (2.0 * yNdc);
				xScreen *= aspectRatio;

				int rayDepth = 0;
				bool rayLostInBackground = false;
				bool rayIsAbsorbed = false;
				CRTColor attenuation(255, 255, 255);
				Ray ray = scene.camera.generateRay(xScreen, yScreen);
				while (rayDepth < kMaxRayDepth && !rayLostInBackground && !rayIsAbsorbed) 
				{
					const int pixelIndex = rowIdx * scene.imageWidth + colIdx;
					HitRecord hit;
					if (!findClosestHit(scene, ray, hit)) {
						// Ray escaped to background — apply accumulated attenuation
						pixels[pixelIndex] = CRTColor(
							std::min(255, scene.backgroundColor.r * attenuation.r / 255),
							std::min(255, scene.backgroundColor.g * attenuation.g / 255),
							std::min(255, scene.backgroundColor.b * attenuation.b / 255)
						);
						rayLostInBackground = true;
						break;
					}

					bool objectHasValidMaterial = scene.objects[hit.objectIndex].m_materialIndex >= 0 && scene.objects[hit.objectIndex].m_materialIndex < static_cast<int>(scene.materials.size());
					if (objectHasValidMaterial && scene.materials[scene.objects[hit.objectIndex].m_materialIndex].m_type == crt::MaterialType::REFLECTIVE)
					{
						if (ray.direction.dot(hit.normal) > 0.0) {
							// Back face of one-sided mirror: shade as diffuse instead of reflecting
							const CRTColor shadeResult = shadeHit(scene, hit);
							pixels[pixelIndex] = CRTColor(
								std::min(255, shadeResult.r * attenuation.r / 255),
								std::min(255, shadeResult.g * attenuation.g / 255),
								std::min(255, shadeResult.b * attenuation.b / 255)
							);
							rayIsAbsorbed = true;
							break;
						}
						const crt::Material& mat = scene.materials[scene.objects[hit.objectIndex].m_materialIndex];
						attenuation.r = static_cast<int>(attenuation.r * mat.m_albedo[0]);
						attenuation.g = static_cast<int>(attenuation.g * mat.m_albedo[1]);
						attenuation.b = static_cast<int>(attenuation.b * mat.m_albedo[2]);
						ray.direction = ray.direction - hit.normal * (ray.direction.dot(hit.normal) * 2.0);
						ray.origin = hit.position + ray.direction * kShadowEpsilon;
						rayDepth++;
						continue;
					}

					else 
					{

						const CRTColor shadeResult = shadeHit(scene, hit);
						pixels[pixelIndex] = CRTColor(
							std::min(255, shadeResult.r * attenuation.r / 255),
							std::min(255, shadeResult.g * attenuation.g / 255),
							std::min(255, shadeResult.b * attenuation.b / 255)
						);
						rayIsAbsorbed = true;
						break;
					}
				}
			}
		}
	};

	const unsigned int threadCount = std::max(1u, std::thread::hardware_concurrency());
	const int rowsPerThread = (scene.imageHeight + static_cast<int>(threadCount) - 1) / static_cast<int>(threadCount);

	std::vector<std::thread> threads;
	for (unsigned int threadIdx = 0; threadIdx < threadCount; ++threadIdx) {
		const int rowStart = static_cast<int>(threadIdx) * rowsPerThread;
		const int rowEnd = std::min(scene.imageHeight, rowStart + rowsPerThread);
		if (rowStart >= rowEnd) {
			break;
		}
		threads.emplace_back(renderRows, rowStart, rowEnd);
	}
	for (std::thread& t : threads) {
		t.join();
	}
}

void writePPM(const std::string& path, const Scene& scene, const std::vector<CRTColor>& pixels) {
	std::ofstream out(path, std::ios::out | std::ios::binary);
	out << "P6\n" << scene.imageWidth << " " << scene.imageHeight << "\n255\n";

	for (const CRTColor& color : pixels) {
		const unsigned char rgb[3] = {
			static_cast<unsigned char>(color.r),
			static_cast<unsigned char>(color.g),
			static_cast<unsigned char>(color.b)
		};
		out.write(reinterpret_cast<const char*>(rgb), 3);
	}
}

std::string resolveScenePath(const std::string& sceneFile) {
	const std::filesystem::path input(sceneFile);
	if (input.is_absolute() && std::filesystem::exists(input)) {
		return input.string();
	}

	const std::vector<std::filesystem::path> candidates = {
		input,
		std::filesystem::path("Homework8") / input,
		std::filesystem::path("../Homework8") / input,
		std::filesystem::path("../../Homework8") / input,
	};

	for (const auto& candidate : candidates) {
		if (std::filesystem::exists(candidate)) {
			return candidate.string();
		}
	}

	return sceneFile;
}

} // namespace

int main(int argc, char** argv) {
	std::vector<std::string> sceneFiles;
	if (argc > 1) {
		for (int i = 1; i < argc; ++i) {
			sceneFiles.emplace_back(argv[i]);
		}
	} else {
		sceneFiles = {"Homework8/Scenes/scene0.crtscene"};
	}


	std::filesystem::create_directories("output");

	for (const std::string& sceneFile : sceneFiles) {
		const std::string resolvedSceneFile = resolveScenePath(sceneFile);
		Scene scene;
		if (!loadScene(resolvedSceneFile, scene)) {
			std::cerr << "Failed to open scene file: " << resolvedSceneFile << std::endl;
			std::cerr << "Skipping " << sceneFile << std::endl;
			continue;
		}

		const std::filesystem::path scenePath(resolvedSceneFile);
			std::cout << "Rendering " << resolvedSceneFile 
					  << " (" << scene.imageWidth << "x" << scene.imageHeight<<" )..." << std::endl;

			std::vector<CRTColor> pixels;
			renderScene(scene, pixels);

			const std::string outputPath = "output/homework9/" + scenePath.stem().string()
				+ ".ppm";
			writePPM(outputPath, scene, pixels);

			std::cout << "  -> " << outputPath << std::endl;
	}

	return 0;
}

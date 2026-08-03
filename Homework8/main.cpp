#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numbers>
#include <string>
#include <thread>
#include <vector>
#include "raytracer/Color.h"
#include "raytracer/Light.h"
#include "raytracer/Triangle.h"
#include "raytracer/Camera.h"
#include "rapidjson/document.h"
#include "rapidjson/istreamwrapper.h"

namespace {

using crt::CRTTriangle;
using crt::CRTVector;
using crt::CRTColor;
using crt::CRTLight;
using crt::Camera;
using crt::Ray;

constexpr int kLightColorIndex = -1;
constexpr double kShadowEpsilon = 1e-6;


struct Scene {
	int imageWidth = 1920;
	int imageHeight = 1080;
	CRTColor backgroundColor;
	Camera camera;
	std::vector<CRTLight> lights;
	std::vector<CRTTriangle> triangles;
};

struct HitRecord {
	double t = 0.0;
	CRTVector position;
	CRTVector normal;
	int colorIndex = 0;
};

static const std::vector<CRTColor> kObjectBaseColors = {
	CRTColor(255, 0, 0),
	CRTColor(0, 0, 255),
	CRTColor(90, 200, 120),
	CRTColor(230, 180, 50),
	CRTColor(180, 90, 220),
	CRTColor(60, 200, 200)
};

static const std::vector<double> kObjectAlbedos = {
	0.35,
	0.50,
	0.70,
	0.85,
	0.60,
	0.75
};

static CRTColor colorFromUnitFloats(const rapidjson::Value& arr) {
	auto clamp255 = [](double c) {
		const int v = static_cast<int>(std::round(c * 255.0));
		return std::max(0, std::min(255, v));
	};
	return CRTColor(clamp255(arr[0].GetDouble()), clamp255(arr[1].GetDouble()), clamp255(arr[2].GetDouble()));
}

static const CRTColor kLightColor(255, 230, 40);

void appendSphereMesh(Scene& scene, const CRTVector& center, double radius, int stacks, int slices, int colorIndex) {
	constexpr double pi = std::numbers::pi_v<double>;

	auto spherePoint = [&](double theta, double phi) {
		const double sinTheta = std::sin(theta);
		return CRTVector(
			center.x + radius * sinTheta * std::cos(phi),
			center.y + radius * std::cos(theta),
			center.z + radius * sinTheta * std::sin(phi)
		);
	};

	for (int stack = 0; stack < stacks; ++stack) {
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
				scene.triangles.emplace_back(p00, p10, p01, colorIndex);
			}
			if (stack != stacks - 1) {
				scene.triangles.emplace_back(p01, p10, p11, colorIndex);
			}
		}
	}
}

void addLightSpheres(Scene& scene, double radius = 0.14, int stacks = 14, int slices = 24) {
	for (const CRTLight& sceneLight : scene.lights) {
		appendSphereMesh(scene, sceneLight.getPosition(), radius, stacks, slices, kLightColorIndex);
	}
}

bool findClosestHit(const Scene& scene, const Ray& ray, HitRecord& hit) {
	double closestT = 1e30;
	int closestIdx = -1;

	for (size_t triIdx = 0; triIdx < scene.triangles.size(); ++triIdx) {
		double t = 0.0;
		if (scene.triangles[triIdx].intersect(ray, t) && t < closestT) {
			closestT = t;
			closestIdx = static_cast<int>(triIdx);
		}
	}

	if (closestIdx < 0) {
		return false;
	}

	const CRTTriangle& triangle = scene.triangles[closestIdx];
	hit.t = closestT;
	hit.position = ray.origin + ray.direction * closestT;
	hit.normal = triangle.normal();
	hit.colorIndex = triangle.colorIndex;
	return true;
}

CRTColor objectBaseColorForIndex(int colorIndex) {
	const int paletteIndex = colorIndex % static_cast<int>(kObjectBaseColors.size());
	return kObjectBaseColors[paletteIndex < 0 ? 0 : paletteIndex];
}

double objectAlbedoForIndex(int colorIndex) {
	const int paletteIndex = colorIndex % static_cast<int>(kObjectAlbedos.size());
	return kObjectAlbedos[paletteIndex < 0 ? 0 : paletteIndex];
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
	for (const CRTTriangle& triangle : scene.triangles) {
		if (triangle.colorIndex == kLightColorIndex) {
			continue;
		}

		double shadowT = 0.0;
		if (triangle.intersect(shadowRay, shadowT) && shadowT < distanceToLight - kShadowEpsilon) {
			return 0.0;
		}
	}

	const double sphereArea = 4.0 * kPi * distanceToLight * distanceToLight;
	return (sceneLight.getIntensity() / sphereArea) * cosLaw;
}

double composedLightForHit(const Scene& scene, const HitRecord& hit) {
	double lightSum = 0.0;
	for (const CRTLight& sceneLight : scene.lights) {
		lightSum += lightContributionForHit(scene, hit, sceneLight);
	}
	return lightSum;
}

CRTColor shadeHit(const Scene& scene, const HitRecord& hit, double albedoScale) {
	if (hit.colorIndex == kLightColorIndex) {
		return kLightColor;
	}

	const double lightFactor = composedLightForHit(scene, hit);
	if (lightFactor <= 0.0) {
		return CRTColor(0, 0, 0);
	}

	const CRTColor baseColor = objectBaseColorForIndex(hit.colorIndex);
	const double albedo = std::clamp(objectAlbedoForIndex(hit.colorIndex) * albedoScale, 0.0, 1.0);
	return CRTColor(
		std::min(255, static_cast<int>(baseColor.r * albedo * lightFactor)),
		std::min(255, static_cast<int>(baseColor.g * albedo * lightFactor)),
		std::min(255, static_cast<int>(baseColor.b * albedo * lightFactor))
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

	if (doc.HasMember("objects")) {
		const auto& objects = doc["objects"];
		for (rapidjson::SizeType objIdx = 0; objIdx < objects.Size(); ++objIdx) {
			const auto& obj = objects[objIdx];

			std::vector<CRTVector> vertices;
			const auto& verts = obj["vertices"];
			vertices.reserve(verts.Size() / 3);
			for (rapidjson::SizeType i = 0; i + 2 < verts.Size(); i += 3) {
				vertices.emplace_back(
					verts[i].GetDouble(),
					verts[i + 1].GetDouble(),
					verts[i + 2].GetDouble()
				);
			}

			const int colorIndex = static_cast<int>(objIdx);
			const auto& tri = obj["triangles"];
			for (rapidjson::SizeType i = 0; i + 2 < tri.Size(); i += 3) {
				const int i0 = tri[i].GetInt();
				const int i1 = tri[i + 1].GetInt();
				const int i2 = tri[i + 2].GetInt();
				scene.triangles.emplace_back(vertices[i0], vertices[i1], vertices[i2], colorIndex);
			}
		}
	}

	addLightSpheres(scene);
	return true;
}

void renderScene(const Scene& scene, std::vector<CRTColor>& pixels, double albedoScale) {
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

				const Ray ray = scene.camera.generateRay(xScreen, yScreen);

				HitRecord hit;
				if (!findClosestHit(scene, ray, hit)) {
					continue;
				}

				const int pixelIndex = rowIdx * scene.imageWidth + colIdx;
				pixels[pixelIndex] = shadeHit(scene, hit, albedoScale);
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

	const std::vector<double> albedoScales = {0.33, 0.66, 1.0};

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
		for (size_t scaleIdx = 0; scaleIdx < albedoScales.size(); ++scaleIdx) {
			const double albedoScale = albedoScales[scaleIdx];
			std::cout << "Rendering " << resolvedSceneFile
					  << " scale=" << albedoScale
					  << " (" << scene.imageWidth << "x" << scene.imageHeight
					  << ", " << scene.triangles.size() << " triangles total)..." << std::endl;

			std::vector<CRTColor> pixels;
			renderScene(scene, pixels, albedoScale);

			const std::string outputPath = "output/" + scenePath.stem().string()
				+ "_albedo_" + std::to_string(scaleIdx + 1) + ".ppm";
			writePPM(outputPath, scene, pixels);

			std::cout << "  -> " << outputPath << std::endl;
		}
	}

	return 0;
}

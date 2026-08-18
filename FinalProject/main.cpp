#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numbers>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include<limits> //For std::numeric_limits
#include "raytracer/Scene.h"
#include "raytracer/Renderer.h"
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
using crt::Scene;
using crt::HitRecord;

constexpr double kShadowEpsilon = 1e-6;



static CRTColor colorFromUnitFloats(const rapidjson::Value& arr) 
{
	auto clamp255 = [](double c) {
		const int v = static_cast<int>(std::round(c * 255.0));
		return std::max(0, std::min(255, v));
	};
	return CRTColor(clamp255(arr[0].GetDouble()), clamp255(arr[1].GetDouble()), clamp255(arr[2].GetDouble()));
}

static const CRTColor kLightColor(255, 230, 40);

// Resolves scene resource paths (e.g. bitmap texture files) that are given relative to the finalProject folder.
std::string resolveResourcePath(const std::string& relativePath) {
	const std::filesystem::path absoluteCandidate(relativePath);
	if (absoluteCandidate.is_absolute() && std::filesystem::exists(absoluteCandidate)) {
		return absoluteCandidate.string();
	}

	std::string trimmed = relativePath;
	while (!trimmed.empty() && trimmed.front() == '/') {
		trimmed.erase(trimmed.begin());
	}
	const std::filesystem::path input(trimmed);
	if (std::filesystem::exists(input)) {
		return input.string();
	}

	const std::vector<std::filesystem::path> candidates = {
		std::filesystem::path("finalProject") / input,
		std::filesystem::path("../finalProject") / input,
		std::filesystem::path("../../finalProject") / input,
	};
	for (const auto& candidate : candidates) {
		if (std::filesystem::exists(candidate)) {
			return candidate.string();
		}
	}
	return trimmed;
}

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
		lightObject.computeBoundingBox();
	}
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

	std::string environmentMapName; // resolved to scene.environmentTextureIndex once textures are parsed below
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
		if (settings.HasMember("environment_map")) {
			environmentMapName = settings["environment_map"].GetString();
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
		if (cameraNode.HasMember("fov")) {
			scene.camera.fovYDegrees = cameraNode["fov"].GetDouble();
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

	std::unordered_map<std::string, int> textureNameToIndex;
	std::unordered_map<std::string, std::string> bitmapTextureNameToPath; // for the environment_map HDR reload below
	if (doc.HasMember("textures")) {
		const auto& texturesJson = doc["textures"];
		for (rapidjson::SizeType i = 0; i < texturesJson.Size(); ++i) {
			const auto& texJson = texturesJson[i];
			crt::Texture texture;
			texture.m_name = texJson["name"].GetString();
			const std::string typeStr = texJson["type"].GetString();
			if (typeStr == "albedo") {
				texture.m_type = crt::TextureType::ALBEDO;
				const auto& albedoArray = texJson["albedo"];
				texture.m_albedo = CRTVector(albedoArray[0].GetDouble(), albedoArray[1].GetDouble(), albedoArray[2].GetDouble());
			} else if (typeStr == "edges") {
				texture.m_type = crt::TextureType::EDGES;
				const auto& edgeColorArray = texJson["edge_color"];
				texture.m_edgeColor = CRTVector(edgeColorArray[0].GetDouble(), edgeColorArray[1].GetDouble(), edgeColorArray[2].GetDouble());
				const auto& innerColorArray = texJson["inner_color"];
				texture.m_innerColor = CRTVector(innerColorArray[0].GetDouble(), innerColorArray[1].GetDouble(), innerColorArray[2].GetDouble());
				texture.m_edgeWidth = texJson["edge_width"].GetDouble();
			} else if (typeStr == "checker") {
				texture.m_type = crt::TextureType::CHECKER;
				const auto& colorAArray = texJson["color_A"];
				texture.m_colorA = CRTVector(colorAArray[0].GetDouble(), colorAArray[1].GetDouble(), colorAArray[2].GetDouble());
				const auto& colorBArray = texJson["color_B"];
				texture.m_colorB = CRTVector(colorBArray[0].GetDouble(), colorBArray[1].GetDouble(), colorBArray[2].GetDouble());
				texture.m_squareSize = texJson["square_size"].GetDouble();
			} else if (typeStr == "bitmap") {
				texture.m_type = crt::TextureType::BITMAP;
				const std::string resolvedPath = resolveResourcePath(texJson["file_path"].GetString());
				if (!texture.loadBitmap(resolvedPath)) {
					std::cerr << "Failed to load bitmap texture: " << resolvedPath << std::endl;
				}
				bitmapTextureNameToPath[texture.m_name] = resolvedPath;
			}
			textureNameToIndex[texture.m_name] = static_cast<int>(scene.textures.size());
			scene.textures.push_back(std::move(texture));
		}
	}

	if (!environmentMapName.empty()) {
		const auto it = textureNameToIndex.find(environmentMapName);
		if (it != textureNameToIndex.end()) {
			scene.environmentTextureIndex = it->second;
			// Reload the same file as true linear HDR radiance (not the display-ready LDR
			// loadBitmap already did above) - see Texture::loadBitmapHDR.
			const auto pathIt = bitmapTextureNameToPath.find(environmentMapName);
			if (pathIt != bitmapTextureNameToPath.end()) {
				if (!scene.textures[scene.environmentTextureIndex].loadBitmapHDR(pathIt->second)) {
					std::cerr << "Failed to load environment_map as HDR: " << pathIt->second << std::endl;
				}
			}
		} else {
			std::cerr << "Unknown texture referenced by environment_map: " << environmentMapName << std::endl;
		}
	}

	if (doc.HasMember("materials")) {
    const auto& mats = doc["materials"];
    for (rapidjson::SizeType i = 0; i < mats.Size(); ++i) {
        const auto& mat = mats[i];
        double albedoValues[3] = {1.0, 1.0, 1.0};
		bool smoothShadingFlag{false};
		crt::MaterialType materialType = crt::MaterialType::DIFFUSE;
		int textureIndex = -1;
        if (mat.HasMember("albedo")) {
           	const auto& albedoValue = mat["albedo"];
			if (albedoValue.IsString()) {
				const std::string textureName = albedoValue.GetString();
				const auto it = textureNameToIndex.find(textureName);
				if (it != textureNameToIndex.end()) {
					textureIndex = it->second;
				} else {
					std::cerr << "Unknown texture referenced by material: " << textureName << std::endl;
				}
			} else {
				albedoValues[0] = albedoValue[0].GetDouble();
				albedoValues[1] = albedoValue[1].GetDouble();
				albedoValues[2] = albedoValue[2].GetDouble();
			}
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
		material.m_textureIndex = textureIndex;

		// If the material is refractive, check for index_of_refraction
		if (materialType == crt::MaterialType::REFRACTIVE && mat.HasMember("ior")) {
			double indexOfRefraction = mat["ior"].GetDouble();
			material.setIndexOfRefraction(indexOfRefraction);
		}
		
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

			std::vector<CRTVector> uvs;
			if (obj.HasMember("uvs")) {
				const auto& uvsJson = obj["uvs"];
				uvs.reserve(uvsJson.Size() / 3);
				for (rapidjson::SizeType i = 0; i + 2 < uvsJson.Size(); i += 3) {
					uvs.emplace_back(
						uvsJson[i].GetDouble(),
						uvsJson[i + 1].GetDouble(),
						uvsJson[i + 2].GetDouble()
					);
				}
			}

			const auto& tri = obj["triangles"];
			for (rapidjson::SizeType i = 0; i + 2 < tri.Size(); i += 3)
			{
				const int i0 = tri[i].GetInt();
				const int i1 = tri[i + 1].GetInt();
				const int i2 = tri[i + 2].GetInt();
				newObject.m_triangles.emplace_back(vertices[i0], vertices[i1], vertices[i2]);
				if (!uvs.empty())
				{
					newObject.m_triangles.back().setVertexUVs(uvs[i0], uvs[i1], uvs[i2]);
				}
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
			newObject.computeBoundingBox();
		}
	}

	//addLightSpheres(scene);
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

// Flattens every object's triangles into a single list for the acceleration tree, tagging
// each triangle with its owning object's index (via objectIndex) so material/emissive
// identity survives once triangles are pulled out of their Object grouping.
std::vector<CRTTriangle> flattenTriangles(const Scene& scene) {
	size_t totalTriangleCount = 0;
	for (const Object& object : scene.objects) {
		totalTriangleCount += object.m_triangles.size();
	}

	std::vector<CRTTriangle> flattened;
	flattened.reserve(totalTriangleCount);
	for (size_t objIdx = 0; objIdx < scene.objects.size(); ++objIdx) {
		const Object& object = scene.objects[objIdx];
		bool castsShadow = !object.m_isEmissive;
		if (castsShadow && object.m_materialIndex >= 0 &&
			object.m_materialIndex < static_cast<int>(scene.materials.size()) &&
			scene.materials[object.m_materialIndex].m_type == crt::MaterialType::REFRACTIVE) {
			castsShadow = false;
		}
		for (CRTTriangle triangle : object.m_triangles) {
			triangle.objectIndex = static_cast<int>(objIdx);
			triangle.castsShadow = castsShadow;
			flattened.push_back(triangle);
		}
	}
	return flattened;
}


crt::Ray cameraRayForPixel(const Scene& scene, int colIdx, int rowIdx, double aspectRatio)
{
	const double xRaster = static_cast<double>(colIdx) + 0.5;
	const double yRaster = static_cast<double>(rowIdx) + 0.5;

	const double xNdc = xRaster / static_cast<double>(scene.imageWidth);
	const double yNdc = yRaster / static_cast<double>(scene.imageHeight);

	double xScreen = (2.0 * xNdc) - 1.0;
	const double yScreen = 1.0 - (2.0 * yNdc);
	xScreen *= aspectRatio;

	return scene.camera.generateRay(xScreen, yScreen);
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

// Renders one frame of the current scene.camera state to outputDir/baseName_NNNN.ppm.
// Returns the render time in seconds, for the caller to accumulate/report.
double renderAndWriteFrame(Scene& scene, const std::string& outputDir, const std::string& baseName, int frameIndex, int frameCount) {
	std::vector<CRTColor> pixels;
	const auto renderStart = std::chrono::steady_clock::now();
	renderScene(scene, pixels);
	const std::chrono::duration<double> renderSeconds = std::chrono::steady_clock::now() - renderStart;

	std::ostringstream frameName;
	frameName << baseName << "_" << std::setw(4) << std::setfill('0') << frameIndex << ".ppm";
	const std::string outputPath = outputDir + "/" + frameName.str();
	writePPM(outputPath, scene, pixels);

	std::cout << "  frame " << (frameIndex + 1) << "/" << frameCount << " -> " << outputPath
			   << " (" << renderSeconds.count() << "s)" << std::endl;
	return renderSeconds.count();
}

// Renders frameCount frames of the camera orbiting 360 degrees around pivot (world Y axis),
// always aimed at pivot, starting from scene.camera's current distance/height. Writes each
// frame as outputDir/baseName_NNNN.ppm. Restores scene.camera to its original value after.
void renderOrbitClip(Scene& scene, const CRTVector& pivot, int frameCount, const std::string& outputDir, const std::string& baseName) {
	std::filesystem::create_directories(outputDir);

	const Camera originalCamera = scene.camera;
	const CRTVector offset = originalCamera.origin - pivot;
	constexpr double kTwoPi = 2.0 * std::numbers::pi_v<double>;

	double totalRenderSeconds = 0.0;
	for (int frame = 0; frame < frameCount; ++frame) {
		const double angle = kTwoPi * static_cast<double>(frame) / static_cast<double>(frameCount);
		const CRTVector newOrigin = pivot + offset.rotateY(angle);
		scene.camera = Camera(newOrigin, pivot - newOrigin, CRTVector(0, 1, 0));

		totalRenderSeconds += renderAndWriteFrame(scene, outputDir, baseName, frame, frameCount);
	}

	scene.camera = originalCamera;
	std::cout << "Orbit clip render time: " << totalRenderSeconds << "s ("
			   << (totalRenderSeconds / frameCount) << "s/frame avg)" << std::endl;
}

// Renders a two-phase clip: a "vertigo"/dolly-zoom opening (dollyFrames) followed by a circular
// orbit (orbitFrames) around pivot. During the dolly phase, the camera slides toward or away
// from pivot along a fixed direction while fovStart animates to fovEnd; distance is solved each
// frame so that distance * tan(fov/2) stays constant, which keeps the subject's apparent size
// fixed while the background stretches/compresses - the actual Hitchcock "Vertigo" illusion,
// not just an arbitrary simultaneous move+zoom. The orbit phase then circles at the dolly's
// final distance and fovEnd, held constant. Writes one continuous frame sequence and restores
// scene.camera afterward.
void renderVertigoOrbitClip(Scene& scene, const CRTVector& pivot, int dollyFrames, int orbitFrames,
							 double fovStartDegrees, double fovEndDegrees,
							 const std::string& outputDir, const std::string& baseName) {
	std::filesystem::create_directories(outputDir);
	constexpr double kPi = std::numbers::pi_v<double>;
	constexpr double kTwoPi = 2.0 * kPi;
	auto halfTanFov = [](double degrees) { return std::tan(degrees * (kPi / 180.0) * 0.5); };

	const Camera originalCamera = scene.camera;
	const CRTVector initialOffset = originalCamera.origin - pivot;
	const double initialDistance = initialOffset.length();
	const CRTVector dollyDirection = initialOffset.normalized();
	const double startHalfTan = halfTanFov(fovStartDegrees);

	const int totalFrames = dollyFrames + orbitFrames;
	double totalRenderSeconds = 0.0;
	int frameIndex = 0;

	// Phase 1: dolly zoom / vertigo. Camera moves along a fixed direction (no orbiting yet)
	// while FOV animates; distance is solved from the size-preservation invariant above.
	double finalDistance = initialDistance;
	for (int i = 0; i < dollyFrames; ++i) {
		const double t = dollyFrames > 1 ? static_cast<double>(i) / (dollyFrames - 1) : 1.0;
		const double fov = fovStartDegrees + (fovEndDegrees - fovStartDegrees) * t;
		const double distance = initialDistance * startHalfTan / halfTanFov(fov);
		finalDistance = distance;

		const CRTVector newOrigin = pivot + dollyDirection * distance;
		scene.camera = Camera(newOrigin, pivot - newOrigin, CRTVector(0, 1, 0));
		scene.camera.fovYDegrees = fov;

		totalRenderSeconds += renderAndWriteFrame(scene, outputDir, baseName, frameIndex, totalFrames);
		++frameIndex;
	}

	// Phase 2: circular orbit at the dolly's final distance/FOV, showing the subject from all sides.
	const CRTVector orbitOffset = dollyDirection * finalDistance;
	for (int i = 0; i < orbitFrames; ++i) {
		const double angle = kTwoPi * static_cast<double>(i) / static_cast<double>(orbitFrames);
		const CRTVector newOrigin = pivot + orbitOffset.rotateY(angle);
		scene.camera = Camera(newOrigin, pivot - newOrigin, CRTVector(0, 1, 0));
		scene.camera.fovYDegrees = fovEndDegrees;

		totalRenderSeconds += renderAndWriteFrame(scene, outputDir, baseName, frameIndex, totalFrames);
		++frameIndex;
	}

	scene.camera = originalCamera;
	std::cout << "Vertigo+orbit clip render time: " << totalRenderSeconds << "s ("
			   << (totalRenderSeconds / totalFrames) << "s/frame avg)" << std::endl;
}

// Three-phase clip:
//  0) Camera travels in a straight line (fixed FOV) through a glass sphere placed a short
//     distance in front of its starting position, revealing pivot beyond it.
//  1) "Vertigo" dolly-zoom continues from wherever phase 0 ended: distance decreases while FOV
//     widens from fovStartDegrees, solved each frame so distance*tan(fov/2) stays constant
//     (apparent subject size fixed, background stretches). fovEnd is *solved*, not fixed, so
//     that distance lands exactly on targetMinDistance regardless of how far phase 0 already
//     traveled - avoids the "ends up inside the subject's geometry" bug from a hardcoded fovEnd.
//  2) Circular orbit at that final distance/FOV.
// Adds the glass sphere as new scene geometry and rebuilds scene.accelerationTree to include
// it. Restores scene.camera (but not the added geometry) when done.
void renderGlassRevealVertigoOrbitClip(
	Scene& scene, const CRTVector& pivot,
	double glassSphereRadius, double glassSphereDistanceFromStart,
	int glassFrames, int dollyFrames, int orbitFrames,
	double fovStartDegrees, double targetMinDistance, double maxFovDegrees,
	const std::string& outputDir, const std::string& baseName) {
	std::filesystem::create_directories(outputDir);
	constexpr double kPi = std::numbers::pi_v<double>;
	constexpr double kTwoPi = 2.0 * kPi;
	auto halfTanFov = [](double degrees) { return std::tan(degrees * (kPi / 180.0) * 0.5); };

	const Camera originalCamera = scene.camera;
	const CRTVector initialOffset = originalCamera.origin - pivot;
	const double initialDistance = initialOffset.length();
	const CRTVector direction = initialOffset.normalized();
	const double startHalfTan = halfTanFov(fovStartDegrees);

	const int totalFrames = glassFrames + dollyFrames + orbitFrames;
	double totalRenderSeconds = 0.0;
	int frameIndex = 0;

	// --- Phase 0 setup: add the glass sphere and rebuild the tree to include it ---
	const double sphereCenterDistance = initialDistance - glassSphereDistanceFromStart;
	const CRTVector sphereCenter = pivot + direction * sphereCenterDistance;
	{
		Object& glassObject = scene.objects.emplace_back();
		double glassAlbedo[3] = {1.0, 1.0, 1.0};
		// smooth_shading=true requires real vertex normals below - appendSphereMesh doesn't set
		// them (they default to (0,0,0)), which previously produced a NaN-normalized normal on
		// every hit; that NaN silently poisoned the ray with no visible artifact at all, making
		// the sphere invisible despite being geometrically present and correctly intersected.
		crt::Material glassMaterial(glassAlbedo, crt::MaterialType::REFRACTIVE, true);
		glassMaterial.setIndexOfRefraction(1.5);
		glassObject.m_materialIndex = static_cast<int>(scene.materials.size());
		scene.materials.push_back(glassMaterial);
		appendSphereMesh(glassObject, sphereCenter, glassSphereRadius, 24, 32);
		for (CRTTriangle& triangle : glassObject.m_triangles) {
			triangle.setVertexNormals(
				(triangle.v0 - sphereCenter).normalized(),
				(triangle.v1 - sphereCenter).normalized(),
				(triangle.v2 - sphereCenter).normalized());
		}
		glassObject.computeBoundingBox();
	}
	scene.accelerationTree = crt::AccelerationTree::build(flattenTriangles(scene));

	// --- Phase 0: straight-line travel from the start position to just past the sphere ---
	const double glassEndDistance = sphereCenterDistance - glassSphereRadius * 1.5;
	for (int i = 0; i < glassFrames; ++i) {
		const double t = glassFrames > 1 ? static_cast<double>(i) / (glassFrames - 1) : 1.0;
		const double distance = initialDistance + (glassEndDistance - initialDistance) * t;
		const CRTVector newOrigin = pivot + direction * distance;
		scene.camera = Camera(newOrigin, pivot - newOrigin, CRTVector(0, 1, 0));
		scene.camera.fovYDegrees = fovStartDegrees;

		totalRenderSeconds += renderAndWriteFrame(scene, outputDir, baseName, frameIndex, totalFrames);
		++frameIndex;
	}

	// --- Phase 1: vertigo dolly-zoom from the glass phase's end distance ---
	const double vertigoStartDistance = glassEndDistance;
	// Two independent safety limits: don't get closer than targetMinDistance (avoids ending up
	// inside the subject's own geometry), and don't widen past maxFovDegrees regardless (an
	// extreme FOV alone reads as disorienting even at a "safe" distance). Take whichever is hit
	// first - i.e. the smaller, less extreme fovEnd - since fovStartDegrees varies per call and
	// the distance needed to reach targetMinDistance isn't the same swing every time.
	const double fovEndFromDistance = 2.0 * std::atan(vertigoStartDistance * startHalfTan / targetMinDistance) * (180.0 / kPi);
	const double fovEndDegrees = std::min(fovEndFromDistance, maxFovDegrees);

	double finalDistance = vertigoStartDistance;
	for (int i = 0; i < dollyFrames; ++i) {
		const double t = dollyFrames > 1 ? static_cast<double>(i) / (dollyFrames - 1) : 1.0;
		const double fov = fovStartDegrees + (fovEndDegrees - fovStartDegrees) * t;
		const double distance = vertigoStartDistance * startHalfTan / halfTanFov(fov);
		finalDistance = distance;

		const CRTVector newOrigin = pivot + direction * distance;
		scene.camera = Camera(newOrigin, pivot - newOrigin, CRTVector(0, 1, 0));
		scene.camera.fovYDegrees = fov;

		totalRenderSeconds += renderAndWriteFrame(scene, outputDir, baseName, frameIndex, totalFrames);
		++frameIndex;
	}

	// --- Phase 2: circular orbit at the final distance/FOV ---
	const CRTVector orbitOffset = direction * finalDistance;
	for (int i = 0; i < orbitFrames; ++i) {
		const double angle = kTwoPi * static_cast<double>(i) / static_cast<double>(orbitFrames);
		const CRTVector newOrigin = pivot + orbitOffset.rotateY(angle);
		scene.camera = Camera(newOrigin, pivot - newOrigin, CRTVector(0, 1, 0));
		scene.camera.fovYDegrees = fovEndDegrees;

		totalRenderSeconds += renderAndWriteFrame(scene, outputDir, baseName, frameIndex, totalFrames);
		++frameIndex;
	}

	scene.camera = originalCamera;
	std::cout << "Glass+vertigo+orbit clip: solved fovEnd=" << fovEndDegrees << " deg, final distance="
			   << finalDistance << ". Render time: " << totalRenderSeconds << "s ("
			   << (totalRenderSeconds / totalFrames) << "s/frame avg)" << std::endl;
}

// Renders the 90-frame orbit clip for known final-project scenes, reusing the scene's own
// already-loaded, tree-built state. No-op for scenes without a known orbit configuration.
void renderKnownOrbitClip(const std::string& sceneStem, Scene& scene) {
	if (sceneStem == "scene1") {
		// object 1 is the dragon mesh (object 0 is the ground plane).
		const CRTVector dragonCenter = (scene.objects[1].m_boundingBox.min + scene.objects[1].m_boundingBox.max) * 0.5;
		std::cout << "Rendering vertigo dolly-zoom + orbit clip around dragon (scene1)..." << std::endl;
		// 1s vertigo dolly-zoom (dragon stays the same size, background stretches) into 2s of orbit.
		// Glass sphere placed 4 units in front of the camera's starting position; pass through
		// it (20 frames), vertigo dolly-zoom to within 11 units of the dragon while FOV widens
		// from 55 degrees - a wider, "further away" establishing FOV (25 frames), then orbit
		// (45 frames) to show it off. 90 frames total, matching the earlier clip's length.
		renderGlassRevealVertigoOrbitClip(scene, dragonCenter, 1.2, 4.0, 20, 25, 45, 55.0, 11.0, 80.0,
										   "output/finalProject/orbit_scene1", "scene1_orbit");
	} else if (sceneStem == "scene2") {
		// objects 2 (glass) and 3 (mirror) are the two spheres; pivot is their combined center.
		// scene2's own camera sits *outside* the room looking at a solid front wall (verified by
		// rendering it unmodified - solid gray, no spheres visible), so the orbit uses its own
		// interior starting position instead of scene.camera's original (exterior) one.
		const CRTVector glassCenter = (scene.objects[2].m_boundingBox.min + scene.objects[2].m_boundingBox.max) * 0.5;
		const CRTVector mirrorCenter = (scene.objects[3].m_boundingBox.min + scene.objects[3].m_boundingBox.max) * 0.5;
		const CRTVector pivot = (glassCenter + mirrorCenter) * 0.5;
		// Room interior is only ~3 units deep and the spheres fill much of it, so start close
		// and slightly above rather than reusing scene.camera's (exterior, blocked) position.
		scene.camera.origin = pivot + CRTVector(0.0, 1.2, 0.85);
		std::cout << "Rendering orbit clip around glass/mirror spheres (scene2)..." << std::endl;
		renderOrbitClip(scene, pivot, 90, "output/finalProject/orbit_scene2", "scene2_orbit");
	} else if (sceneStem == "my_scene") {
		// Blender-exported cube scene; orbit around the origin using the scene's own
		// (already well-framed) camera position as the starting distance/height.
		std::cout << "Rendering orbit clip around cube (my_scene)..." << std::endl;
		renderOrbitClip(scene, CRTVector(0.0, 0.0, 0.0), 90, "output/finalProject/orbit_my_scene", "my_scene_orbit");
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
		sceneFiles = {"/home/ckai/Learning/C++_Chaos/RayTracing/Homework/Homework11/Scenes/homework11_scene2.crtscene"};
	}


#if defined(NDEBUG)
	constexpr const char* kBuildConfig = "Release (NDEBUG defined)";
#else
	constexpr const char* kBuildConfig = "Debug (NDEBUG not defined)";
#endif
	std::cout << "Build configuration: " << kBuildConfig << std::endl;

	std::filesystem::create_directories("output/finalProject");

	const auto programStart = std::chrono::steady_clock::now();
	double totalRenderSeconds = 0.0;

	for (const std::string& sceneFile : sceneFiles) {
		const std::string resolvedSceneFile = resolveScenePath(sceneFile);
		Scene scene;

		const auto loadStart = std::chrono::steady_clock::now();
		if (!loadScene(resolvedSceneFile, scene)) {
			std::cerr << "Failed to open scene file: " << resolvedSceneFile << std::endl;
			std::cerr << "Skipping " << sceneFile << std::endl;
			continue;
		}
		const std::chrono::duration<double> loadSeconds = std::chrono::steady_clock::now() - loadStart;

		scene.accelerationTree = crt::AccelerationTree::build(flattenTriangles(scene));

		const std::filesystem::path scenePath(resolvedSceneFile);
			std::cout << "Rendering " << resolvedSceneFile 
					  << " (" << scene.imageWidth << "x" << scene.imageHeight<<" )..." << std::endl;

			std::vector<CRTColor> pixels;
			const auto renderStart = std::chrono::steady_clock::now();
			renderScene(scene, pixels);
			const std::chrono::duration<double> renderSeconds = std::chrono::steady_clock::now() - renderStart;
			totalRenderSeconds += renderSeconds.count();

			const std::string outputPath = "output/finalProject/" + scenePath.stem().string()
				+ ".ppm";
			const auto writeStart = std::chrono::steady_clock::now();
			writePPM(outputPath, scene, pixels);
			const std::chrono::duration<double> writeSeconds = std::chrono::steady_clock::now() - writeStart;

			std::cout << "  -> " << outputPath << std::endl;
			std::cout << "  Timing: load=" << loadSeconds.count() << "s render=" << renderSeconds.count()
					   << "s write=" << writeSeconds.count() << "s" << std::endl;

			renderKnownOrbitClip(scenePath.stem().string(), scene);
	}

	const std::chrono::duration<double> totalSeconds = std::chrono::steady_clock::now() - programStart;
	std::cout << "Total render time (all scenes): " << totalRenderSeconds << "s" << std::endl;
	std::cout << "Total wall time: " << totalSeconds.count() << "s" << std::endl;

	return 0;
}

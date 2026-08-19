#include "SceneLoader.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_map>

#include "rapidjson/document.h"
#include "rapidjson/istreamwrapper.h"

namespace crt {

namespace {

CRTColor colorFromUnitFloats(const rapidjson::Value &arr)
{
	auto clamp255 = [](double c)
	{
		const int v = static_cast<int>(std::round(c * 255.0));
		return std::max(0, std::min(255, v));
	};
	return CRTColor(clamp255(arr[0].GetDouble()), clamp255(arr[1].GetDouble()), clamp255(arr[2].GetDouble()));
}

// Resolves scene resource paths (e.g. bitmap texture files) that are given relative to the
// FinalProject folder, regardless of whether the binary is run from the repo root or its own
// build directory.
std::string resolveResourcePath(const std::string &relativePath)
{
	const std::filesystem::path absoluteCandidate(relativePath);
	if (absoluteCandidate.is_absolute() && std::filesystem::exists(absoluteCandidate))
	{
		return absoluteCandidate.string();
	}

	std::string trimmed = relativePath;
	while (!trimmed.empty() && trimmed.front() == '/')
	{
		trimmed.erase(trimmed.begin());
	}
	const std::filesystem::path input(trimmed);
	if (std::filesystem::exists(input))
	{
		return input.string();
	}

	const std::vector<std::filesystem::path> candidates = {
		std::filesystem::path("FinalProject") / input,
		std::filesystem::path("../FinalProject") / input,
		std::filesystem::path("../../FinalProject") / input,
	};
	for (const auto &candidate : candidates)
	{
		if (std::filesystem::exists(candidate))
		{
			return candidate.string();
		}
	}
	return trimmed;
}

} // namespace

bool loadScene(const std::string &path, Scene &scene)
{
	std::ifstream fileStream(path);
	if (!fileStream.is_open())
	{
		std::cerr << "Failed to open scene file: " << path << std::endl;
		return false;
	}

	rapidjson::IStreamWrapper isw(fileStream);
	rapidjson::Document doc;
	doc.ParseStream(isw);
	if (doc.HasParseError())
	{
		std::cerr << "Failed to parse JSON in: " << path << std::endl;
		return false;
	}

	std::string environmentMapName; // resolved to scene.environmentTextureIndex once textures are parsed below
	if (doc.HasMember("settings"))
	{
		const auto &settings = doc["settings"];
		if (settings.HasMember("background_color"))
		{
			scene.backgroundColor = colorFromUnitFloats(settings["background_color"]);
		}
		if (settings.HasMember("image_settings"))
		{
			const auto &imageSettings = settings["image_settings"];
			scene.imageWidth = imageSettings["width"].GetInt();
			scene.imageHeight = imageSettings["height"].GetInt();
		}
		if (settings.HasMember("environment_map"))
		{
			environmentMapName = settings["environment_map"].GetString();
		}
	}

	if (doc.HasMember("camera"))
	{
		const auto &cameraNode = doc["camera"];
		if (cameraNode.HasMember("matrix"))
		{
			const auto &m = cameraNode["matrix"];
			double matrix[9];
			for (int i = 0; i < 9; ++i)
			{
				matrix[i] = m[i].GetDouble();
			}

			// Scene files store camera basis in the transposed layout compared to
			// Camera::applyMatrix, so transpose before applying.
			double transposed[9] = {
				matrix[0], matrix[3], matrix[6],
				matrix[1], matrix[4], matrix[7],
				matrix[2], matrix[5], matrix[8]};
			scene.camera.applyMatrix(transposed);
		}
		if (cameraNode.HasMember("position"))
		{
			const auto &p = cameraNode["position"];
			scene.camera.origin = CRTVector(p[0].GetDouble(), p[1].GetDouble(), p[2].GetDouble());
		}
		if (cameraNode.HasMember("fov"))
		{
			scene.camera.fovYDegrees = cameraNode["fov"].GetDouble();
		}
	}

	if (doc.HasMember("lights"))
	{
		const auto &lights = doc["lights"];
		for (rapidjson::SizeType lightIdx = 0; lightIdx < lights.Size(); ++lightIdx)
		{
			const auto &light = lights[lightIdx];
			if (!light.HasMember("position"))
			{
				continue;
			}
			const auto &p = light["position"];
			const double intensity = light.HasMember("intensity") ? light["intensity"].GetDouble() : 1.0;
			scene.lights.push_back(CRTLight(CRTVector(p[0].GetDouble(), p[1].GetDouble(), p[2].GetDouble()), intensity));
		}
	}

	std::unordered_map<std::string, int> textureNameToIndex;
	std::unordered_map<std::string, std::string> bitmapTextureNameToPath; // for the environment_map HDR reload below
	if (doc.HasMember("textures"))
	{
		const auto &texturesJson = doc["textures"];
		for (rapidjson::SizeType i = 0; i < texturesJson.Size(); ++i)
		{
			const auto &texJson = texturesJson[i];
			Texture texture;
			texture.m_name = texJson["name"].GetString();
			const std::string typeStr = texJson["type"].GetString();
			if (typeStr == "albedo")
			{
				texture.m_type = TextureType::ALBEDO;
				const auto &albedoArray = texJson["albedo"];
				texture.m_albedo = CRTVector(albedoArray[0].GetDouble(), albedoArray[1].GetDouble(), albedoArray[2].GetDouble());
			}
			else if (typeStr == "edges")
			{
				texture.m_type = TextureType::EDGES;
				const auto &edgeColorArray = texJson["edge_color"];
				texture.m_edgeColor = CRTVector(edgeColorArray[0].GetDouble(), edgeColorArray[1].GetDouble(), edgeColorArray[2].GetDouble());
				const auto &innerColorArray = texJson["inner_color"];
				texture.m_innerColor = CRTVector(innerColorArray[0].GetDouble(), innerColorArray[1].GetDouble(), innerColorArray[2].GetDouble());
				texture.m_edgeWidth = texJson["edge_width"].GetDouble();
			}
			else if (typeStr == "checker")
			{
				texture.m_type = TextureType::CHECKER;
				const auto &colorAArray = texJson["color_A"];
				texture.m_colorA = CRTVector(colorAArray[0].GetDouble(), colorAArray[1].GetDouble(), colorAArray[2].GetDouble());
				const auto &colorBArray = texJson["color_B"];
				texture.m_colorB = CRTVector(colorBArray[0].GetDouble(), colorBArray[1].GetDouble(), colorBArray[2].GetDouble());
				texture.m_squareSize = texJson["square_size"].GetDouble();
			}
			else if (typeStr == "bitmap")
			{
				texture.m_type = TextureType::BITMAP;
				const std::string resolvedPath = resolveResourcePath(texJson["file_path"].GetString());
				if (!texture.loadBitmap(resolvedPath))
				{
					std::cerr << "Failed to load bitmap texture: " << resolvedPath << std::endl;
				}
				bitmapTextureNameToPath[texture.m_name] = resolvedPath;
			}
			textureNameToIndex[texture.m_name] = static_cast<int>(scene.textures.size());
			scene.textures.push_back(std::move(texture));
		}
	}

	if (!environmentMapName.empty())
	{
		const auto it = textureNameToIndex.find(environmentMapName);
		if (it != textureNameToIndex.end())
		{
			scene.environmentTextureIndex = it->second;
			// Reload the same file as true linear HDR radiance (not the display-ready LDR
			// loadBitmap already did above) - see Texture::loadBitmapHDR.
			const auto pathIt = bitmapTextureNameToPath.find(environmentMapName);
			if (pathIt != bitmapTextureNameToPath.end())
			{
				if (!scene.textures[scene.environmentTextureIndex].loadBitmapHDR(pathIt->second))
				{
					std::cerr << "Failed to load environment_map as HDR: " << pathIt->second << std::endl;
				}
			}
		}
		else
		{
			std::cerr << "Unknown texture referenced by environment_map: " << environmentMapName << std::endl;
		}
	}

	if (doc.HasMember("materials"))
	{
		const auto &mats = doc["materials"];
		for (rapidjson::SizeType i = 0; i < mats.Size(); ++i)
		{
			const auto &mat = mats[i];
			double albedoValues[3] = {1.0, 1.0, 1.0};
			bool smoothShadingFlag{false};
			MaterialType materialType = MaterialType::DIFFUSE;
			int textureIndex = -1;
			if (mat.HasMember("albedo"))
			{
				const auto &albedoValue = mat["albedo"];
				if (albedoValue.IsString())
				{
					const std::string textureName = albedoValue.GetString();
					const auto it = textureNameToIndex.find(textureName);
					if (it != textureNameToIndex.end())
					{
						textureIndex = it->second;
					}
					else
					{
						std::cerr << "Unknown texture referenced by material: " << textureName << std::endl;
					}
				}
				else
				{
					albedoValues[0] = albedoValue[0].GetDouble();
					albedoValues[1] = albedoValue[1].GetDouble();
					albedoValues[2] = albedoValue[2].GetDouble();
				}
			}
			if (mat.HasMember("smooth_shading"))
			{
				smoothShadingFlag = mat["smooth_shading"].GetBool();
			}
			if (mat.HasMember("type"))
			{
				const std::string typeStr = mat["type"].GetString();
				if (typeStr == "diffuse")
				{
					materialType = MaterialType::DIFFUSE;
				}
				else if (typeStr == "reflective")
				{
					materialType = MaterialType::REFLECTIVE;
				}
				else if (typeStr == "refractive")
				{
					materialType = MaterialType::REFRACTIVE;
				}
			}
			Material material(albedoValues, materialType, smoothShadingFlag);
			material.m_textureIndex = textureIndex;

			// If the material is refractive, check for index_of_refraction
			if (materialType == MaterialType::REFRACTIVE && mat.HasMember("ior"))
			{
				double indexOfRefraction = mat["ior"].GetDouble();
				material.setIndexOfRefraction(indexOfRefraction);
			}

			scene.materials.push_back(material);
		}
	}
	if (doc.HasMember("objects"))
	{
		const auto &objects = doc["objects"];
		for (rapidjson::SizeType objIdx = 0; objIdx < objects.Size(); ++objIdx)
		{
			const auto &obj = objects[objIdx];
			Object &newObject = scene.objects.emplace_back();
			if (obj.HasMember("material_index"))
			{
				const int materialIndex = obj["material_index"].GetInt();
				newObject.m_materialIndex = materialIndex;
			}
			bool smoothShadingFlag = false;
			if (newObject.m_materialIndex >= 0 && scene.materials[newObject.m_materialIndex].m_smoothShading)
			{
				smoothShadingFlag = true;
			}

			std::vector<CRTVector> vertices;
			const auto &verts = obj["vertices"];
			vertices.reserve(verts.Size() / 3);
			// Zero-initialised accumulation buffer for area-weighted vertex normals.
			std::vector<CRTVector> vertexNormals(verts.Size() / 3, CRTVector(0, 0, 0));

			for (rapidjson::SizeType i = 0; i + 2 < verts.Size(); i += 3)
			{
				vertices.emplace_back(
					verts[i].GetDouble(),
					verts[i + 1].GetDouble(),
					verts[i + 2].GetDouble());
			}

			std::vector<CRTVector> uvs;
			if (obj.HasMember("uvs"))
			{
				const auto &uvsJson = obj["uvs"];
				uvs.reserve(uvsJson.Size() / 3);
				for (rapidjson::SizeType i = 0; i + 2 < uvsJson.Size(); i += 3)
				{
					uvs.emplace_back(
						uvsJson[i].GetDouble(),
						uvsJson[i + 1].GetDouble(),
						uvsJson[i + 2].GetDouble());
				}
			}

			const auto &tri = obj["triangles"];
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
					CRTVector faceNormal = newObject.m_triangles.back().getNormalVector();
					vertexNormals[i0] = vertexNormals[i0] + faceNormal;
					vertexNormals[i1] = vertexNormals[i1] + faceNormal;
					vertexNormals[i2] = vertexNormals[i2] + faceNormal;
				}
			}
			if (smoothShadingFlag)
			{
				// Resetting the length of the vertex normals to 1.0 for smooth shading.
				for (CRTVector &normal : vertexNormals)
				{
					normal = normal.normalized();
				}
				// Assigning the computed vertices to the triangles for smooth shading.
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

	return true;
}

std::vector<CRTTriangle> flattenTriangles(const Scene &scene)
{
	size_t totalTriangleCount = 0;
	for (const Object &object : scene.objects)
	{
		totalTriangleCount += object.m_triangles.size();
	}

	std::vector<CRTTriangle> flattened;
	flattened.reserve(totalTriangleCount);
	for (size_t objIdx = 0; objIdx < scene.objects.size(); ++objIdx)
	{
		const Object &object = scene.objects[objIdx];
		bool castsShadow = !object.m_isEmissive;
		if (castsShadow && object.m_materialIndex >= 0 &&
			object.m_materialIndex < static_cast<int>(scene.materials.size()) &&
			scene.materials[object.m_materialIndex].m_type == MaterialType::REFRACTIVE)
		{
			castsShadow = false;
		}
		for (CRTTriangle triangle : object.m_triangles)
		{
			triangle.objectIndex = static_cast<int>(objIdx);
			triangle.castsShadow = castsShadow;
			flattened.push_back(triangle);
		}
	}
	return flattened;
}

} // namespace crt

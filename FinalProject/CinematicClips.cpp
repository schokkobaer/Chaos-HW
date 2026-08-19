#include "CinematicClips.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numbers>
#include <sstream>

#include "raytracer/Renderer.h"
#include "SceneLoader.h"

namespace crt {

namespace {

constexpr double kPi = std::numbers::pi_v<double>;
constexpr double kTwoPi = 2.0 * kPi;

double halfTanFov(double degrees)
{
	return std::tan(degrees * (kPi / 180.0) * 0.5);
}

// The dolly-zoom ("vertigo") invariant: distance * tan(fov/2) stays constant, so the subject's
// apparent size on screen is unchanged while the camera moves and FOV changes - only the
// background's perspective (compression/stretching) changes. This is the actual Hitchcock
// "Vertigo" effect, not just an arbitrary simultaneous move+zoom.
double dollyZoomDistanceForFov(double referenceDistance, double referenceFovDegrees, double targetFovDegrees)
{
	return referenceDistance * halfTanFov(referenceFovDegrees) / halfTanFov(targetFovDegrees);
}

// Inverse of dollyZoomDistanceForFov: solves for the FOV that reaches targetDistance under the
// same distance*tan(fov/2) invariant.
double dollyZoomFovForDistance(double referenceDistance, double referenceFovDegrees, double targetDistance)
{
	return 2.0 * std::atan(referenceDistance * halfTanFov(referenceFovDegrees) / targetDistance) * (180.0 / kPi);
}

void appendSphereMesh(Object &emptyLightObject, const CRTVector &center, double radius, int stacks, int slices)
{
	emptyLightObject.m_triangles.reserve(2 * slices * (stacks - 1));
	auto spherePoint = [&](double theta, double phi)
	{
		const double sinTheta = std::sin(theta);
		return CRTVector(
			center.x + radius * sinTheta * std::cos(phi),
			center.y + radius * std::cos(theta),
			center.z + radius * sinTheta * std::sin(phi));
	};

	for (int stack = 0; stack < stacks; ++stack)
	{
		const double theta0 = kPi * static_cast<double>(stack) / static_cast<double>(stacks);
		const double theta1 = kPi * static_cast<double>(stack + 1) / static_cast<double>(stacks);

		for (int slice = 0; slice < slices; ++slice)
		{
			const double phi0 = kTwoPi * static_cast<double>(slice) / static_cast<double>(slices);
			const double phi1 = kTwoPi * static_cast<double>(slice + 1) / static_cast<double>(slices);

			const CRTVector p00 = spherePoint(theta0, phi0);
			const CRTVector p01 = spherePoint(theta0, phi1);
			const CRTVector p10 = spherePoint(theta1, phi0);
			const CRTVector p11 = spherePoint(theta1, phi1);

			if (stack != 0)
			{
				emptyLightObject.m_triangles.emplace_back(p00, p10, p01);
			}
			if (stack != stacks - 1)
			{
				emptyLightObject.m_triangles.emplace_back(p01, p10, p11);
			}
		}
	}
	return;
}

// Adds a refractive glass sphere (radius, centered at `center`) to the scene as new geometry
// and rebuilds scene.accelerationTree to include it.
//
// Vertex normals are set explicitly below - appendSphereMesh doesn't compute them, so they'd
// otherwise default to (0,0,0). With smooth_shading enabled that zero normal normalizes to NaN,
// which silently poisons every ray hitting the sphere: no error, no visible artifact, just an
// object that's geometrically present and correctly intersected yet invisible in the render.
void addGlassSphereToScene(Scene &scene, const CRTVector &center, double radius)
{
	Object &glassObject = scene.objects.emplace_back();
	double glassAlbedo[3] = {1.0, 1.0, 1.0};
	Material glassMaterial(glassAlbedo, MaterialType::REFRACTIVE, true);
	glassMaterial.setIndexOfRefraction(1.5);
	glassObject.m_materialIndex = static_cast<int>(scene.materials.size());
	scene.materials.push_back(glassMaterial);

	appendSphereMesh(glassObject, center, radius, 24, 32);
	for (CRTTriangle &triangle : glassObject.m_triangles)
	{
		triangle.setVertexNormals(
			(triangle.v0 - center).normalized(),
			(triangle.v1 - center).normalized(),
			(triangle.v2 - center).normalized());
	}
	glassObject.computeBoundingBox();

	scene.accelerationTree = AccelerationTree::build(flattenTriangles(scene));
}

// Renders one frame of the current scene.camera state to outputDir/baseName_NNNN.ppm.
// Returns the render time in seconds, for the caller to accumulate/report.
double renderAndWriteFrame(Scene &scene, const std::string &outputDir, const std::string &baseName, int frameIndex, int frameCount)
{
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
void renderOrbitClip(Scene &scene, const CRTVector &pivot, int frameCount, const std::string &outputDir, const std::string &baseName)
{
	std::filesystem::create_directories(outputDir);

	const Camera originalCamera = scene.camera;
	const CRTVector offset = originalCamera.origin - pivot;

	double totalRenderSeconds = 0.0;
	for (int frame = 0; frame < frameCount; ++frame)
	{
		const double angle = kTwoPi * static_cast<double>(frame) / static_cast<double>(frameCount);
		const CRTVector newOrigin = pivot + offset.rotateY(angle);
		scene.camera = Camera(newOrigin, pivot - newOrigin, CRTVector(0, 1, 0));

		totalRenderSeconds += renderAndWriteFrame(scene, outputDir, baseName, frame, frameCount);
	}

	scene.camera = originalCamera;
	std::cout << "Orbit clip render time: " << totalRenderSeconds << "s ("
			  << (totalRenderSeconds / frameCount) << "s/frame avg)" << std::endl;
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
	Scene &scene, const CRTVector &pivot,
	double glassSphereRadius, double glassSphereDistanceFromStart,
	int glassFrames, int dollyFrames, int orbitFrames,
	double fovStartDegrees, double targetMinDistance, double maxFovDegrees,
	const std::string &outputDir, const std::string &baseName)
{
	std::filesystem::create_directories(outputDir);

	const Camera originalCamera = scene.camera;
	const CRTVector initialOffset = originalCamera.origin - pivot;
	const double initialDistance = initialOffset.length();
	const CRTVector direction = initialOffset.normalized();

	const int totalFrames = glassFrames + dollyFrames + orbitFrames;
	double totalRenderSeconds = 0.0;
	int frameIndex = 0;

	// --- Phase 0 setup: add the glass sphere and rebuild the tree to include it ---
	const double sphereCenterDistance = initialDistance - glassSphereDistanceFromStart;
	const CRTVector sphereCenter = pivot + direction * sphereCenterDistance;
	addGlassSphereToScene(scene, sphereCenter, glassSphereRadius);

	// --- Phase 0: straight-line travel from the start position to just past the sphere ---
	const double glassEndDistance = sphereCenterDistance - glassSphereRadius * 1.5;
	for (int i = 0; i < glassFrames; ++i)
	{
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
	const double fovEndFromDistance = dollyZoomFovForDistance(vertigoStartDistance, fovStartDegrees, targetMinDistance);
	const double fovEndDegrees = std::min(fovEndFromDistance, maxFovDegrees);

	double finalDistance = vertigoStartDistance;
	for (int i = 0; i < dollyFrames; ++i)
	{
		const double t = dollyFrames > 1 ? static_cast<double>(i) / (dollyFrames - 1) : 1.0;
		const double fov = fovStartDegrees + (fovEndDegrees - fovStartDegrees) * t;
		const double distance = dollyZoomDistanceForFov(vertigoStartDistance, fovStartDegrees, fov);
		finalDistance = distance;

		const CRTVector newOrigin = pivot + direction * distance;
		scene.camera = Camera(newOrigin, pivot - newOrigin, CRTVector(0, 1, 0));
		scene.camera.fovYDegrees = fov;

		totalRenderSeconds += renderAndWriteFrame(scene, outputDir, baseName, frameIndex, totalFrames);
		++frameIndex;
	}

	// --- Phase 2: circular orbit at the final distance/FOV ---
	const CRTVector orbitOffset = direction * finalDistance;
	for (int i = 0; i < orbitFrames; ++i)
	{
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

} // namespace

void writePPM(const std::string &path, const Scene &scene, const std::vector<CRTColor> &pixels)
{
	std::ofstream out(path, std::ios::out | std::ios::binary);
	out << "P6\n"
		<< scene.imageWidth << " " << scene.imageHeight << "\n255\n";

	for (const CRTColor &color : pixels)
	{
		const unsigned char rgb[3] = {
			static_cast<unsigned char>(color.r),
			static_cast<unsigned char>(color.g),
			static_cast<unsigned char>(color.b)};
		out.write(reinterpret_cast<const char *>(rgb), 3);
	}
}

void renderKnownOrbitClip(const std::string &sceneStem, Scene &scene)
{
	if (sceneStem == "scene1")
	{
		// object 1 is the dragon mesh (object 0 is the ground plane).
		const CRTVector dragonCenter = (scene.objects[1].m_boundingBox.min + scene.objects[1].m_boundingBox.max) * 0.5;
		std::cout << "Rendering vertigo dolly-zoom + orbit clip around dragon (scene1)..." << std::endl;

		// 1s vertigo dolly-zoom (dragon stays the same size, background stretches) into 2s of
		// orbit, 90 frames total, matching the earlier clip's length: pass through the glass
		// sphere (kGlassPhaseFrames), vertigo dolly-zoom in (kDollyPhaseFrames), then orbit to
		// show it off (kOrbitPhaseFrames).
		constexpr double kGlassSphereRadius = 1.2;
		constexpr double kGlassSphereDistanceFromCamera = 4.0; // sphere placed this far in front of the camera's starting position
		constexpr int kGlassPhaseFrames = 20;
		constexpr int kDollyPhaseFrames = 25;
		constexpr int kOrbitPhaseFrames = 45;
		constexpr double kDollyFovStartDegrees = 55.0; // wider, "further away" establishing FOV
		constexpr double kDollyTargetMinDistance = 11.0; // how close the dolly-zoom ends up to the dragon
		constexpr double kDollyMaxFovDegrees = 80.0; // safety cap regardless of kDollyTargetMinDistance

		renderGlassRevealVertigoOrbitClip(scene, dragonCenter, kGlassSphereRadius, kGlassSphereDistanceFromCamera,
										  kGlassPhaseFrames, kDollyPhaseFrames, kOrbitPhaseFrames,
										  kDollyFovStartDegrees, kDollyTargetMinDistance, kDollyMaxFovDegrees,
										  "output/finalProject/orbit_scene1", "scene1_orbit");
	}
	else if (sceneStem == "scene2")
	{
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
	}
	else if (sceneStem == "my_scene")
	{
		// Blender-exported cube scene; orbit around the origin using the scene's own
		// (already well-framed) camera position as the starting distance/height.
		std::cout << "Rendering orbit clip around cube (my_scene)..." << std::endl;
		renderOrbitClip(scene, CRTVector(0.0, 0.0, 0.0), 90, "output/finalProject/orbit_my_scene", "my_scene_orbit");
	}
}

} // namespace crt

#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include "raytracer/Scene.h"
#include "raytracer/Renderer.h"
#include "SceneLoader.h"
#include "CinematicClips.h"

namespace
{

	using crt::CRTColor;
	using crt::resolveExistingPath;
	using crt::Scene;

	// Resolves a scene file argument against the FinalProject/Scenes folder, regardless of whether
	// the binary is run from the repo root or its own build directory.
	std::string resolveScenePath(const std::string &sceneFile)
	{
		return resolveExistingPath(sceneFile, {
												  "FinalProject/Scenes",
												  "../FinalProject/Scenes",
												  "../../FinalProject/Scenes",
											  });
	}

} // namespace

int main(int argc, char **argv)
{
	std::vector<std::string> sceneFiles;
	if (argc > 1)
	{
		for (int i = 1; i < argc; ++i)
		{
			sceneFiles.emplace_back(argv[i]);
		}
	}
	else
	{
		std::cerr << "Usage: " << argv[0] << " <scene1.crtscene> [<scene2.crtscene> ...]" << std::endl;
		return 1;
	}

#if defined(NDEBUG)
	constexpr const char *kBuildConfig = "Release (NDEBUG defined)";
#else
	constexpr const char *kBuildConfig = "Debug (NDEBUG not defined)";
#endif
	std::cout << "Build configuration: " << kBuildConfig << std::endl;

	std::filesystem::create_directories("output/finalProject");

	const auto programStart = std::chrono::steady_clock::now();
	double totalRenderSeconds = 0.0;

	for (const std::string &sceneFile : sceneFiles)
	{
		const std::string resolvedSceneFile = resolveScenePath(sceneFile);
		Scene scene;

		const auto loadStart = std::chrono::steady_clock::now();
		if (!loadScene(resolvedSceneFile, scene))
		{
			std::cerr << "Failed to open scene file: " << resolvedSceneFile << std::endl;
			std::cerr << "Skipping " << sceneFile << std::endl;
			continue;
		}
		const std::chrono::duration<double> loadSeconds = std::chrono::steady_clock::now() - loadStart;

		scene.accelerationTree = crt::AccelerationTree::build(flattenTriangles(scene));

		const std::filesystem::path scenePath(resolvedSceneFile);
		std::cout << "Rendering " << resolvedSceneFile
				  << " (" << scene.imageWidth << "x" << scene.imageHeight << " )..." << std::endl;

		std::vector<CRTColor> pixels;
		const auto renderStart = std::chrono::steady_clock::now();
		renderScene(scene, pixels);
		const std::chrono::duration<double> renderSeconds = std::chrono::steady_clock::now() - renderStart;
		totalRenderSeconds += renderSeconds.count();

		const std::string outputPath = "output/finalProject/" + scenePath.stem().string() + ".ppm";
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

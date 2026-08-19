add_executable(finalProject main.cpp SceneLoader.cpp CinematicClips.cpp)
target_link_libraries(finalProject PRIVATE raytracer_common)
target_include_directories(finalProject PRIVATE
	${CMAKE_SOURCE_DIR}/Resources/parser/rapidjson/include
)

add_executable(homework13 main.cpp)
target_link_libraries(homework13 PRIVATE raytracer_common)
target_include_directories(homework13 PRIVATE
	${CMAKE_SOURCE_DIR}/Resources/parser/rapidjson/include
)

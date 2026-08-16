add_executable(homework11 main.cpp)
target_link_libraries(homework11 PRIVATE raytracer_common)
target_include_directories(homework11 PRIVATE
	${CMAKE_SOURCE_DIR}/Resources/parser/rapidjson/include
)

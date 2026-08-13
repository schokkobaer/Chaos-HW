add_executable(homework9 main.cpp)
target_link_libraries(homework9 PRIVATE raytracer_common)
target_include_directories(homework9 PRIVATE
	${CMAKE_SOURCE_DIR}/Resources/parser/rapidjson/include
)

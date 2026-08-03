add_executable(homework8 main.cpp)
target_link_libraries(homework8 PRIVATE raytracer_common)
target_include_directories(homework8 PRIVATE
	${CMAKE_SOURCE_DIR}/Resources/parser/rapidjson/include
)

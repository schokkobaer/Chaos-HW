add_executable(homework14 main.cpp)
target_link_libraries(homework14 PRIVATE raytracer_common)
target_include_directories(homework14 PRIVATE
	${CMAKE_SOURCE_DIR}/Resources/parser/rapidjson/include
)

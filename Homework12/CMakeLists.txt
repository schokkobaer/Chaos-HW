add_executable(homework12 main.cpp)
target_link_libraries(homework12 PRIVATE raytracer_common)
target_include_directories(homework12 PRIVATE
	${CMAKE_SOURCE_DIR}/Resources/parser/rapidjson/include
)

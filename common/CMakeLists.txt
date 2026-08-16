add_library(raytracer_common STATIC
    src/Vector.cpp
    src/Triangle.cpp
    src/Camera.cpp
    src/Renderer.cpp
)
target_include_directories(raytracer_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(raytracer_common PUBLIC cxx_std_20)

add_library(raytracer_common STATIC
    src/Vector.cpp
    src/Triangle.cpp
    src/Camera.cpp
    src/Renderer.cpp
    src/Texture.cpp
    src/Object.cpp
    src/AABB.cpp
)
target_include_directories(raytracer_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_include_directories(raytracer_common PRIVATE
    ${CMAKE_SOURCE_DIR}/Resources/stb
)

target_compile_features(raytracer_common PUBLIC cxx_std_20)

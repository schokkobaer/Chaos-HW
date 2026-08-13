cmake_minimum_required(VERSION 3.16)
project(RayTracingHomework LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Shared camera/vector/geometry library used by Homework8 and later.
# Homework2-7 keep their own self-contained main.cpp files and are not
# built through this CMake project.
add_subdirectory(common)

add_subdirectory(Homework8)
add_subdirectory(Homework9)

# To add the next homework, create HomeworkN/ with its own CMakeLists.txt
# (see Homework8/CMakeLists.txt for the pattern) and add:
#   add_subdirectory(HomeworkN)

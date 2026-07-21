cmake_minimum_required(VERSION 3.20)
project(Hades LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(HADES_ENABLE_CUDA "Enable CUDA device adapter (Hades-Core)" OFF)
option(HADES_ENABLE_HIP  "Enable HIP device adapter (Hades-Core)"  OFF)
option(HADES_BUILD_TESTS "Build Hades-Benchmark's own unit tests" OFF)

find_package(Threads REQUIRED)

add_subdirectory(src/Core)
add_subdirectory(src/Codegen)
add_subdirectory(src/Driver)

if(HADES_BUILD_TESTS)
    add_subdirectory(src/Core/tests)
endif()

target_link_libraries(Hades-Core    PUBLIC Threads::Threads)
target_link_libraries(Hades-Codegen PUBLIC Hades-Core)
target_link_libraries(Hades-Driver  PUBLIC Hades-Core Hades-Codegen)

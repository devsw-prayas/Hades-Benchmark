cmake_minimum_required(VERSION 3.20)
project(Hades VERSION 3.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(HADES_ENABLE_CUDA "Enable CUDA device adapter (Hades-Core)" OFF)
option(HADES_ENABLE_HIP  "Enable HIP device adapter (Hades-Core)"  OFF)
option(HADES_BUILD_TESTS "Build Hades-Benchmark's own unit tests" OFF)

find_package(Threads REQUIRED)

add_subdirectory(src/Core)
add_subdirectory(src/Codegen)
add_subdirectory(src/Driver)

if(HADES_BUILD_TESTS)
    add_subdirectory(src/Core/tests)
    add_subdirectory(src/Codegen/tests)
endif()

target_link_libraries(Hades-Core    PUBLIC Threads::Threads)
target_link_libraries(Hades-Codegen PUBLIC Hades-Core)
target_link_libraries(Hades-Driver  PUBLIC Hades-Core Hades-Codegen)

# --version prints this triplet; --build=cmake's generated suite CMakeLists.txt
# needs this repo root to add_subdirectory Hades-Core.
target_compile_definitions(Hades-Driver PRIVATE
    HADES_VERSION="${PROJECT_VERSION}"
    HADES_BENCHMARK_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
)

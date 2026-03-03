cmake_minimum_required(VERSION 3.20)
project(Hades LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE Hades_HEADERS  CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/Public/*.h")
file(GLOB_RECURSE Hades_SOURCE   CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/Private/*.cpp")
file(GLOB_RECURSE Hades_INL      CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/Public/*.inl")

set(HADES_BARRIER_SPIN_COUNT    "1024" CACHE STRING "Spin iterations before condvar sleep in CyclicBarrier")
set(HADES_ENGINE_QUEUE_CAPACITY "64"   CACHE STRING "SPSC queue capacity for HadesEngine (must be power of two)")

add_library(Hades SHARED
    ${Hades_HEADERS}
    ${Hades_SOURCE}
    ${Hades_INL}
)
target_compile_definitions(Hades PRIVATE Hades_EXPORTS)

target_include_directories(Hades PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Public
)

target_precompile_headers(Hades PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/Public/Hades.h)

target_compile_definitions(Hades PRIVATE
    HADES_SHARED
    HADES_BUILDING_RUNTIME
    HADES_BARRIER_SPIN_COUNT=${HADES_BARRIER_SPIN_COUNT}
    HADES_ENGINE_QUEUE_CAPACITY=${HADES_ENGINE_QUEUE_CAPACITY}
)

if(MSVC)
    target_compile_options(Hades PRIVATE /W4 /WX /permissive-)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(Hades PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        -Wshadow
        -Wconversion
        -Wsign-conversion
    )
endif()

find_package(Threads REQUIRED)
target_link_libraries(Hades PUBLIC Threads::Threads)

option(HADES_ENABLE_CUDA "Enable CUDA device adapter" OFF)
option(HADES_ENABLE_HIP  "Enable HIP device adapter"  OFF)

if(HADES_ENABLE_CUDA)
    enable_language(CUDA)
    find_package(CUDAToolkit REQUIRED)
    target_link_libraries(Hades PRIVATE CUDA::cudart)
    target_compile_definitions(Hades PRIVATE HADES_ENABLE_CUDA)
endif()

if(HADES_ENABLE_HIP)
    find_package(hip REQUIRED)
    target_link_libraries(Hades PRIVATE hip::host)
    target_compile_definitions(Hades PRIVATE HADES_ENABLE_HIP)
endif()
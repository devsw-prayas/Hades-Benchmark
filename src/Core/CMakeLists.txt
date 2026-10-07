cmake_minimum_required(VERSION 3.20)

# No vcpkg dependencies; opt out of a user-wide `vcpkg integrate install` (its post-build step needs pwsh).
if(NOT DEFINED CMAKE_VS_GLOBALS)
    set(CMAKE_VS_GLOBALS "VcpkgEnabled=false")
endif()
project(Hades-Core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE HadesCore_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/Public/*.h")
file(GLOB_RECURSE HadesCore_SOURCE  CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/Private/*.cpp")
file(GLOB_RECURSE HadesCore_INL     CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/Public/*.inl")

add_library(Hades-Core SHARED
    ${HadesCore_HEADERS}
    ${HadesCore_SOURCE}
    ${HadesCore_INL}
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES
    ${HadesCore_HEADERS}
    ${HadesCore_SOURCE}
    ${HadesCore_INL}
)

# AUTO follows _DEBUG (set by the debug CRT); ON/OFF pin it. Hades-scoped, so it never collides with a host's DEBUG.
set(HADES_ENABLE_DEBUG_CHECKS AUTO CACHE STRING "Hades asserts: AUTO (follow _DEBUG), ON or OFF")
set_property(CACHE HADES_ENABLE_DEBUG_CHECKS PROPERTY STRINGS AUTO ON OFF)
if (HADES_ENABLE_DEBUG_CHECKS STREQUAL "ON")
    target_compile_definitions(Hades-Core PUBLIC HADES_DEBUG_CHECKS=1)
elseif (HADES_ENABLE_DEBUG_CHECKS STREQUAL "OFF")
    target_compile_definitions(Hades-Core PUBLIC HADES_DEBUG_CHECKS=0)
endif()

target_compile_definitions(Hades-Core
    PUBLIC  HADES_SHARED
    PRIVATE HADES_BUILDING_RUNTIME
)

target_include_directories(Hades-Core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Public
)

target_precompile_headers(Hades-Core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/Public/Hades.h)

set_target_properties(Hades-Core PROPERTIES FOLDER "Reflection")

target_compile_options(Hades-Core PRIVATE
    $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>>:/arch:AVX2 /W4 /permissive->
    $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>>>:-mavx2 -Wall -Wextra>
)

if(HADES_ENABLE_CUDA)
    enable_language(CUDA)
    find_package(CUDAToolkit REQUIRED)
    target_link_libraries(Hades-Core PRIVATE CUDA::cudart)
    target_compile_definitions(Hades-Core PRIVATE HADES_ENABLE_CUDA)
endif()

if(HADES_ENABLE_HIP)
    find_package(hip REQUIRED)
    target_link_libraries(Hades-Core PRIVATE hip::host)
    target_compile_definitions(Hades-Core PRIVATE HADES_ENABLE_HIP)
endif()

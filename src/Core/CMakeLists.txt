cmake_minimum_required(VERSION 3.20)
project(Hades-Core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE HadesCore_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/Public/*.h")
file(GLOB_RECURSE HadesCore_SOURCE  CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/Private/*.cpp")
file(GLOB_RECURSE HadesCore_INL     CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/Public/*.inl")

add_library(Hades-Core STATIC
    ${HadesCore_HEADERS}
    ${HadesCore_SOURCE}
    ${HadesCore_INL}
)

target_include_directories(Hades-Core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Public
)

target_precompile_headers(Hades-Core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/Public/Hades.h)

target_compile_options(Hades-Core PRIVATE
    $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:MSVC>>:/arch:AVX2 /W4 /permissive->
    $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<CXX_COMPILER_ID:MSVC>>>:-mavx2 -Wall -Wextra>
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

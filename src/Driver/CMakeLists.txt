cmake_minimum_required(VERSION 3.20)
project(Hades-Driver LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE HadesDriver_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/Public/*.h")
file(GLOB_RECURSE HadesDriver_SOURCE  CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/Private/*.cpp")
file(GLOB_RECURSE HadesDriver_INL     CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/Public/*.inl")

add_executable(Hades-Driver
    ${HadesDriver_HEADERS}
    ${HadesDriver_SOURCE}
    ${HadesDriver_INL}
)

target_include_directories(Hades-Driver PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Public
)

target_precompile_headers(Hades-Driver PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/Public/HadesDriver.h)

target_compile_options(Hades-Driver PRIVATE
    $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:MSVC>>:/arch:AVX2 /W4 /permissive->
    $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<CXX_COMPILER_ID:MSVC>>>:-mavx2 -Wall -Wextra>
)

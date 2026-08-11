cmake_minimum_required(VERSION 3.20)
project(Hades-Codegen LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE HadesCodegen_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/Public/*.h")
file(GLOB_RECURSE HadesCodegen_SOURCE  CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/Private/*.cpp")
file(GLOB_RECURSE HadesCodegen_INL     CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/Public/*.inl")

add_library(Hades-Codegen SHARED
    ${HadesCodegen_HEADERS}
    ${HadesCodegen_SOURCE}
    ${HadesCodegen_INL}
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES
    ${HadesCodegen_HEADERS}
    ${HadesCodegen_SOURCE}
    ${HadesCodegen_INL}
)

target_compile_definitions(Hades-Codegen PRIVATE HADES_CODEGEN_EXPORTS)

target_include_directories(Hades-Codegen PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Public
)

target_precompile_headers(Hades-Codegen PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/Public/HadesCodegen.h)

set_target_properties(Hades-Codegen PROPERTIES FOLDER "Reflection")

target_compile_options(Hades-Codegen PRIVATE
    $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:MSVC>>:/arch:AVX2 /W4 /permissive->
    $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<CXX_COMPILER_ID:MSVC>>>:-mavx2 -Wall -Wextra>
)

cmake_minimum_required(VERSION 3.20)

# No vcpkg dependencies; opt out of a user-wide `vcpkg integrate install` (its post-build step needs pwsh).
if(NOT DEFINED CMAKE_VS_GLOBALS)
    set(CMAKE_VS_GLOBALS "VcpkgEnabled=false")
endif()
project(Hades-Codegen-Tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(Hades-Codegen-Tests
    main.cpp
    CodegenTests.cpp
)
target_link_libraries(Hades-Codegen-Tests PRIVATE Hades-Codegen)

target_compile_options(Hades-Codegen-Tests PRIVATE
    $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>>:/W4 /permissive->
    $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>>>:-Wall -Wextra>
)

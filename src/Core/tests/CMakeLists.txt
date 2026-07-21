cmake_minimum_required(VERSION 3.20)
project(Hades-Core-Tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(Hades-Core-Tests TomlIOTests.cpp)
target_link_libraries(Hades-Core-Tests PRIVATE Hades-Core)

target_compile_options(Hades-Core-Tests PRIVATE
    $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:MSVC>>:/W4 /permissive->
    $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<CXX_COMPILER_ID:MSVC>>>:-Wall -Wextra>
)

cmake_minimum_required(VERSION 3.30)
project(named_pipes)

set(CMAKE_CXX_STANDARD 26)

add_library(BuildSystem IPCManagerBS.cpp
        Manager.cpp)

add_library(Compiler IPCManagerCompiler.cpp
        Manager.cpp)

add_executable(BuildSystemTest BuildSystemTest.cpp
        Testing.cpp)
add_executable(CompilerTest CompilerTest.cpp
        Testing.cpp)

target_link_libraries(BuildSystemTest BuildSystem)
target_link_libraries(CompilerTest Compiler)
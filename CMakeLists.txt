cmake_minimum_required(VERSION 3.30)
project(named_pipes)

set(CMAKE_CXX_STANDARD 26)

add_library(BuildSystem src/IPCManagerBS.cpp
        src/Manager.cpp)
target_include_directories(BuildSystem PUBLIC include)

add_library(Compiler src/IPCManagerCompiler.cpp
        src/Manager.cpp)
target_include_directories(Compiler PUBLIC include)

add_executable(BuildSystemTest tests/BuildSystemTest.cpp
        tests/Testing.cpp)
add_executable(CompilerTest tests/CompilerTest.cpp
        tests/Testing.cpp)

target_link_libraries(BuildSystemTest PRIVATE BuildSystem)
target_link_libraries(CompilerTest PRIVATE Compiler)
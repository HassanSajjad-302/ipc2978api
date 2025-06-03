cmake_minimum_required(VERSION 3.30)
project(named_pipes)

set(CMAKE_CXX_STANDARD 17)

add_library(fmt src/fmt/format.cc src/fmt/os.cc)

add_library(BuildSystem src/IPCManagerBS.cpp
        src/Manager.cpp)
target_include_directories(BuildSystem PUBLIC include src/fmt)

add_library(Compiler src/IPCManagerCompiler.cpp
        src/Manager.cpp)
target_include_directories(Compiler PUBLIC include src/fmt)

target_link_libraries(BuildSystem PUBLIC fmt)
target_link_libraries(Compiler PUBLIC fmt)

add_executable(BuildSystemTest tests/BuildSystemTest.cpp
        tests/Testing.cpp)
add_executable(CompilerTest tests/CompilerTest.cpp
        tests/Testing.cpp)

target_link_libraries(BuildSystemTest PRIVATE BuildSystem)
target_link_libraries(CompilerTest PRIVATE Compiler)

add_executable(Tests tests/test.cpp)
target_link_libraries(Tests PUBLIC Compiler)
target_link_libraries(Tests PUBLIC BuildSystem)
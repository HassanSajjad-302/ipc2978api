cmake_minimum_required(VERSION 3.30)
project(named_pipes)

set(CMAKE_CXX_STANDARD 17)


add_library(BuildSystem src/IPCManagerBS.cpp
        src/Manager.cpp)

add_library(Compiler src/IPCManagerCompiler.cpp
        src/Manager.cpp)

target_include_directories(BuildSystem PUBLIC include)
target_include_directories(Compiler PUBLIC include)


add_executable(BuildSystemTest tests/BuildSystemTest.cpp
        tests/Testing.cpp)
add_executable(CompilerTest tests/CompilerTest.cpp
        tests/Testing.cpp)

target_link_libraries(BuildSystemTest PRIVATE BuildSystem)
target_link_libraries(CompilerTest PRIVATE Compiler)

add_executable(ClangTest tests/ClangTest.cpp tests/Testing.cpp)

set_target_properties(BuildSystemTest PROPERTIES CXX_STANDARD 23)
set_target_properties(CompilerTest PROPERTIES CXX_STANDARD 23)
set_target_properties(ClangTest PROPERTIES CXX_STANDARD 23)

target_link_libraries(ClangTest  BuildSystem)
target_link_libraries(ClangTest Compiler)
target_include_directories(ClangTest PUBLIC include)

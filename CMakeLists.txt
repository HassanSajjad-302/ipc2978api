cmake_minimum_required(VERSION 3.30)
project(named_pipes)

set(CMAKE_CXX_STANDARD 17)

#add_link_options(-fsanitize=thread -fno-omit-frame-pointer)
#add_compile_options(-fno-exceptions -fno-rtti)

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

add_subdirectory(tests/fmt)

target_link_libraries(BuildSystemTest PUBLIC BuildSystem fmt::fmt)
target_link_libraries(CompilerTest PUBLIC Compiler fmt::fmt)

add_executable(ClangTest tests/ClangTest.cpp tests/Testing.cpp)

target_link_libraries(ClangTest PRIVATE BuildSystem Compiler)
target_include_directories(ClangTest PUBLIC include)

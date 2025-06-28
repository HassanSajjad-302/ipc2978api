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


add_library(fmt tests/fmt/src/format.cc tests/fmt/src/os.cc)
target_include_directories(fmt PUBLIC tests/fmt/include)

add_library(Testing tests/Testing.cpp)
target_include_directories(Testing PUBLIC include)
target_link_libraries(Testing PUBLIC fmt)

add_executable(BuildSystemTest tests/BuildSystemTest.cpp)
add_executable(CompilerTest tests/CompilerTest.cpp)


target_link_libraries(BuildSystemTest PUBLIC BuildSystem Testing)
target_link_libraries(CompilerTest PUBLIC Compiler Testing)

add_executable(ClangTest tests/ClangTest.cpp)

target_link_libraries(ClangTest PUBLIC BuildSystem Compiler Testing)
target_include_directories(ClangTest PUBLIC include)
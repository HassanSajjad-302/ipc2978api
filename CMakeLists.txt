cmake_minimum_required(VERSION 3.30)
project(named_pipes)

set(CMAKE_CXX_STANDARD 17)

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang" OR "${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
    add_link_options(-fsanitize=address -fno-omit-frame-pointer)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer -fno-exceptions -fno-rtti)
elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "MSVC")
    if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
        add_compile_options($<$<CONFIG:Debug>:/fsanitize=address>)
    endif ()
    string(REGEX REPLACE "/GR" "/GR-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REGEX REPLACE "/EHsc" "/EHs-c-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    add_definitions(-D_HAS_EXCEPTIONS=0) # for STL
endif ()

add_library(Compiler src/IPCManagerCompiler.cpp
        src/Manager.cpp)

add_library(BuildSystem src/IPCManagerBS.cpp
        src/Manager.cpp)

target_include_directories(Compiler PUBLIC include)
target_include_directories(BuildSystem PUBLIC include)


add_library(fmt tests/fmt/src/format.cc tests/fmt/src/os.cc)
target_include_directories(fmt PUBLIC tests/fmt/include)

add_library(Testing tests/Testing.cpp)
target_include_directories(Testing PUBLIC include)
target_link_libraries(Testing PUBLIC fmt Compiler BuildSystem)

add_executable(CompilerTest tests/CompilerTest.cpp)
add_executable(BuildSystemTest tests/BuildSystemTest.cpp)

target_compile_definitions(BuildSystemTest PRIVATE
        COMPILER_TEST="$<TARGET_FILE:CompilerTest>"
)

target_link_libraries(CompilerTest PUBLIC Compiler Testing)
target_link_libraries(BuildSystemTest PUBLIC BuildSystem Compiler Testing)
add_dependencies(BuildSystemTest CompilerTest)

add_executable(ClangTest tests/ClangTest.cpp)

target_link_libraries(ClangTest PUBLIC BuildSystem Compiler Testing)
target_include_directories(ClangTest PUBLIC include)
set(IPC2978_CLANG_EXECUTABLE "" CACHE FILEPATH "Clang rebuilt with the current IPC2978 library")
if (IPC2978_CLANG_EXECUTABLE)
    get_filename_component(IPC2978_CLANG_EXECUTABLE "${IPC2978_CLANG_EXECUTABLE}" ABSOLUTE)
    target_compile_definitions(ClangTest PRIVATE IPC2978_CLANG_EXECUTABLE="${IPC2978_CLANG_EXECUTABLE}")
endif ()

enable_testing()
add_executable(ResultTest tests/ResultTest.cpp)
target_include_directories(ResultTest PRIVATE include)
add_test(NAME ResultTest COMMAND ResultTest)
add_test(
        NAME MyTest
        COMMAND $<TARGET_FILE:BuildSystemTest>  # resolves correct path on all platforms
)
add_executable(MappingTest tests/MappingTest.cpp)
target_link_libraries(MappingTest PRIVATE Compiler BuildSystem)
add_test(NAME MappingTest COMMAND MappingTest)
if (IPC2978_CLANG_EXECUTABLE)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/ClangTest-work")
    add_test(NAME ClangTest COMMAND ClangTest "${IPC2978_CLANG_EXECUTABLE}")
    set_tests_properties(ClangTest PROPERTIES
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/ClangTest-work"
            TIMEOUT 120)
endif ()

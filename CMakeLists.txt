cmake_minimum_required(VERSION 3.30)
project(named_pipes)

set(CMAKE_CXX_STANDARD 26)

add_library(BuildSystem IPCManagerBS.cpp Messages.cpp
        Manager.cpp)

add_library(Compiler IPCManagerCompiler.cpp Messages.cpp
        Manager.cpp)

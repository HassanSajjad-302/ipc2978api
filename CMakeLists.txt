cmake_minimum_required(VERSION 3.30)
project(named_pipes)

set(CMAKE_CXX_STANDARD 26)

add_executable(named_pipes main.cpp)
add_executable(client client.cpp)

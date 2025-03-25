cmake_minimum_required(VERSION 3.30)
project(named_pipes)

set(CMAKE_CXX_STANDARD 26)

add_executable(server server.cpp Messages.cpp
        Manager.cpp)

add_executable(client client.cpp Messages.cpp
        Manager.cpp)

cmake_minimum_required(VERSION 3.30)
project(named_pipes)

set(CMAKE_CXX_STANDARD 26)

add_library(server server.cpp Messages.cpp
        Manager.cpp)

add_library(client client.cpp Messages.cpp
        Manager.cpp)

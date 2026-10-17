cmake_minimum_required(VERSION 4.3)
project(lorikeet)

set(CMAKE_CXX_STANDARD 20)

add_executable(lorikeet main.cpp
        Server.cpp
        Server.h
        Json.cpp
        Json.h
        Database.cpp
        Database.h
        UIEngine.cpp
        UIEngine.h
)

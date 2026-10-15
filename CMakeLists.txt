cmake_minimum_required(VERSION 4.3)
project(language_learning_backend)

set(CMAKE_CXX_STANDARD 20)

add_executable(language_learning_backend main.cpp
        Server.cpp
        Server.h
        Json.cpp
        Json.h
        Database.cpp
        Database.h
)

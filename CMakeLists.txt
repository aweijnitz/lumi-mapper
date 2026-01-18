cmake_minimum_required(VERSION 3.20)

project(projection-mapper VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_RENDERER "Build the renderer (requires openFrameworks)" ON)

enable_testing()

add_subdirectory(core)
add_subdirectory(server)
if(BUILD_RENDERER)
    add_subdirectory(renderer)
endif()
add_subdirectory(clients/commandlineclient)

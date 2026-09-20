cmake_minimum_required(VERSION 3.10)

project(lua-ffi C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FindPkgConfig)

add_subdirectory(src)

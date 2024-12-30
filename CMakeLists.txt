cmake_minimum_required(VERSION 3.29)
project(imagenmanipulation)

set(CMAKE_CXX_STANDARD 20)

add_executable(imagenmanipulation main.cpp PngReader.h PngReader.cpp)

find_package(ZLIB REQUIRED)
target_link_libraries(imagenmanipulation PRIVATE ZLIB::ZLIB)
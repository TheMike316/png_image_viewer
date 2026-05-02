cmake_minimum_required(VERSION 4.2)
project(png_image_viewer C)

find_package(raylib CONFIG REQUIRED)
find_package(ZLIB REQUIRED)

set(CMAKE_C_STANDARD 99)

add_executable(png_image_viewer main.c)

target_link_libraries(png_image_viewer PRIVATE ZLIB::ZLIB)
target_link_libraries(png_image_viewer PRIVATE raylib)

cmake_minimum_required(VERSION 2.8)
cmake_policy(SET CMP0015 NEW)

project(libdick)

include_directories( . )

link_directories()

set(CMAKE_CXX_COMPILER "clang++")
set(CMAKE_CXX_FLAGS "-std=c++14 -Wall -Werror -g -O0 -DDICK_LOG_ENABLE")

add_library(dick dick.cpp)
target_link_libraries(dick m allegro_monolith)

add_executable(demo demo.cpp)
target_link_libraries(demo dick)

add_custom_target(distr
    COMMAND rm -rf distr
    COMMAND mkdir distr
    COMMAND cp ../dick.h distr
    COMMAND cp libdick.a distr
    DEPENDS dick)

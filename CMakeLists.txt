cmake_minimum_required(VERSION 4.1)
project(shit)

set(CMAKE_CXX_STANDARD 20)

add_executable(shit main.cpp)

target_compile_options(shit PRIVATE
        -Wall -Wextra -O3
)
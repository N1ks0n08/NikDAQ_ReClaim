cmake_minimum_required(VERSION 3.20)
project(NikDAQ_ReClaim CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(main main.cpp)
add_executable(oracle_checkV2 oracle_checkV2.cpp)
add_executable(benchmark benchmark.cpp)
add_executable(spsc_test spsc_queue.cpp)

target_compile_options(spsc_test PRIVATE
    -Wall -Wextra -g -fsanitize=thread)
target_link_options(spsc_test PRIVATE -fsanitize=thread)
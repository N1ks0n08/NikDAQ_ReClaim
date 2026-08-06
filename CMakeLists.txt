cmake_minimum_required(VERSION 3.20)
project(NikDAQ_ReClaim CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(main main.cpp)
add_executable(oracle_checkV2 oracle_checkV2.cpp)
add_executable(benchmark benchmark.cpp)

find_package(Threads REQUIRED)

add_executable(spsc_bench spsc_bench.cpp)
target_compile_options(spsc_bench PRIVATE -O2 -march=native)
target_link_libraries(spsc_bench PRIVATE Threads::Threads)
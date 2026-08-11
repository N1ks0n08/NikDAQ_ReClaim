cmake_minimum_required(VERSION 3.20)
project(NikDAQ_ReClaim CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Feeds IntelliSense the real compiler flags
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Bare `cmake -S . -B build` otherwise gives no -O and no -g
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

add_compile_options(-Wall -Wextra)

find_package(Threads REQUIRED)

# --- Block 1: order book & matching core ---
add_executable(main main.cpp)
add_executable(oracle_checkV2 oracle_checkV2.cpp)

add_executable(benchmark orderbook_benchmark.cpp)
target_compile_options(benchmark PRIVATE -O2)

# --- Block 2: concurrency ---
# -march=native is host-specific; this binary may not run on EC2
add_executable(spsc_bench spsc_bench.cpp)
target_compile_options(spsc_bench PRIVATE -O2 -march=native)
target_link_libraries(spsc_bench PRIVATE Threads::Threads)

# --- Block 3: protocol I/O ---
add_executable(echo_server echo_server.cpp)
target_link_libraries(echo_server PRIVATE Threads::Threads)
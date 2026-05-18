cmake_minimum_required(VERSION 3.10)
project(CustomAllocatorBenchmark)

set(CMAKE_CXX_STANDARD 14)

# Find the Google Benchmark library you installed
find_package(benchmark REQUIRED)

# Compile your engine and the benchmark together
add_executable(alloc_bench benchmark.cpp mm.cpp)

# Link the Google Benchmark library and pthread (POSIX threads)
target_link_libraries(alloc_bench benchmark::benchmark pthread)
cmake_minimum_required(VERSION 3.20)
project(syc)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED on)

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g -DSYC_DEBUG -DSYC_LOG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

add_definitions(
        # -DSYC_LOG
        -DSYC_STDIN
        # -DSYC_STDOUT
        -DSYC_ASSERTS
)
file(GLOB_RECURSE src_files "src/*")
include_directories(src)

add_executable(syc ${src_files} src/mr_passes/use_def.cpp src/mr_passes/use_def.hpp src/mr_passes/reg_restore.cpp src/mr_passes/move_coalesce.cpp)

cmake_minimum_required(VERSION 3.20)
project(syc)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED on)

set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wno-format-security")
set(CMAKE_CXX_FLAGS_DEBUG "-g -DSYC_DEBUG -DSYC_LOG -fno-omit-frame-pointer -fsanitize=address -fsanitize-address-use-after-scope")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

add_definitions(
        # -DSYC_LOG
        -DSYC_STDIN
        -DSYC_STDOUT
        -DSYC_ASSERTS
)
file(GLOB_RECURSE src_files "src/*")
include_directories(src)

add_executable(syc ${src_files})

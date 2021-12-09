cmake_minimum_required(VERSION 3.00)
project(syc)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED on)

set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wno-format-security")
set(CMAKE_CXX_FLAGS_DEBUG "-g -DSYC_DEBUG -DSYC_LOG -fno-omit-frame-pointer -fsanitize=address -fsanitize-address-use-after-scope")
# set(CMAKE_CXX_FLAGS_DEBUG "-g -DSYC_DEBUG -DSYC_LOG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

IF(NOT DEFINED ENV{PLAIN})
    add_definitions(
        -DSYC_STDIN
        -DSYC_STDOUT
        -DSYC_ASSERTS
        -DSYC_DUMP
    )
ENDIF()

file(GLOB_RECURSE src_files "src/*")
include_directories(src src/passes src/mr_passes)

add_executable(syc ${src_files})

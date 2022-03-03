
cmake_minimum_required(VERSION 3.10)

project(utf7 VERSION 0.0.1 LANGUAGES C)

option(utf7_build_tests "Turn on to build tests." ON)
option(utf7_build_conv7 "Turn on to build command line utility to convert from-to utf7/utf8." ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(utf7_SOURCES
    utf7.c
    utf7.h
    tests/utf8.c
    tests/utf8.h
)

add_library(utf7 ${utf7_SOURCES} )

if (${utf7_build_tests})
add_executable(tests utf7.c tests/utf8.c tests/tests.c )
endif()

if (${utf7_build_conv7})
add_executable(conv7 utf7.c tests/utf8.c tests/utf8.c tests/conv7.c)
endif()

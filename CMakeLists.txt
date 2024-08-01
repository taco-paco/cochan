cmake_minimum_required(VERSION 3.5)
project(co-chan VERSION 0.2.0 LANGUAGES CXX)

option(ENABLE_TESTS "Enable tests" ON)

add_library(cochan INTERFACE)
target_include_directories(cochan INTERFACE include/)

# TODO: remove?
set_property(TARGET cochan PROPERTY CXX_STANDARD 20)

if (ENABLE_TESTS)
    add_subdirectory(tests)
endif ()
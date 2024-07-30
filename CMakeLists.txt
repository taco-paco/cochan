cmake_minimum_required(VERSION 3.5)
project(co-chan VERSION 0.2.0 LANGUAGES CXX)

option(ENABLE_TESTS "Enable tests" ON)

add_library(co_chan
        include/co_chan/channel.hpp
        include/co_chan/sender.hpp
        include/co_chan/receiver.hpp
        include/co_chan/utils.hpp

        src/channel.cpp
        src/sender.cpp
        src/receiver.cpp
)

target_include_directories(co_chan PUBLIC include/)

# TODO: remove?
set_property(TARGET co_chan PROPERTY CXX_STANDARD 20)

if (ENABLE_TESTS)
    add_subdirectory(tests)
endif ()
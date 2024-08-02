cmake_minimum_required(VERSION 3.5)
project(co-chan VERSION 0.0.1 LANGUAGES CXX)

option(ENABLE_TESTS "Enable tests" OFF)

add_library(cochan INTERFACE)
target_include_directories(cochan
        INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
set_property(TARGET cochan PROPERTY CXX_STANDARD 20)

include(GNUInstallDirs)
install(TARGETS cochan
        EXPORT cochan_targets
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(EXPORT cochan_targets
        FILE cochan-targets.cmake
        NAMESPACE cochan::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cochan
)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.hpp"
)

if (ENABLE_TESTS)
    add_subdirectory(tests)
endif ()

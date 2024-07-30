option(WITH_LIBCORO "Tests with libcoro" ON)

add_executable(playground playground.cpp)
set_property(TARGET playground PROPERTY CXX_STANDARD 20)

add_executable(test test.cpp dummy_coro.hpp)
target_link_libraries(test PUBLIC co_chan)
set_property(TARGET test PROPERTY CXX_STANDARD 20)

add_executable(sender_drop sender_drop.cpp dummy_coro.hpp)
target_link_libraries(sender_drop PUBLIC co_chan)
set_property(TARGET sender_drop PROPERTY CXX_STANDARD 20)

if (WITH_LIBCORO)
    find_package(PkgConfig REQUIRED)

    add_executable(test_with_libcoro test_with_libcoro.cpp)

    #    pkg_check_modules(LIBCORO REQUIRED libcoro)
    target_include_directories(test_with_libcoro PRIVATE /Users/edwinpaco/Documents/projects/cpp/libcoro/install/include)
    target_link_libraries(test_with_libcoro co_chan /Users/edwinpaco/Documents/projects/cpp/libcoro/install/lib/libcoro.a)

    set_property(TARGET test_with_libcoro PROPERTY CXX_STANDARD 20)
endif ()
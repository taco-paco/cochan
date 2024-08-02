option(WITH_LIBCORO "Tests with libcoro" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(GTest)
if (NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
    )

    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif ()


add_executable(sender_receiver_test sender_receiver_test.cpp dummy_coro.hpp)
target_link_libraries(sender_receiver_test PRIVATE GTest::gtest GTest::gtest_main cochan)
set_property(TARGET sender_receiver_test PROPERTY CXX_STANDARD 20)

if (WITH_LIBCORO)
    add_subdirectory(libcoro)
endif ()
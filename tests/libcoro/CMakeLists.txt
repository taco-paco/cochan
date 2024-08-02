#find_package(PkgConfig REQUIRED)
#pkg_check_modules(LIBCORO REQUIRED libcoro)

enable_testing()

add_executable(sender_receiver_libcoro_test sender_receiver_test.cpp)
target_include_directories(sender_receiver_libcoro_test PRIVATE /Users/edwinpaco/Documents/projects/cpp/libcoro/install/include)
target_link_libraries(sender_receiver_libcoro_test PUBLIC GTest::gtest GTest::gtest_main cochan /Users/edwinpaco/Documents/projects/cpp/libcoro/install/lib/libcoro.a)
set_property(TARGET sender_receiver_libcoro_test PROPERTY CXX_STANDARD 20)

add_executable(awaitables_libcoro_test awaitables_test.cpp)
target_include_directories(awaitables_libcoro_test PRIVATE /Users/edwinpaco/Documents/projects/cpp/libcoro/install/include)
target_link_libraries(awaitables_libcoro_test PUBLIC GTest::gtest GTest::gtest_main cochan /Users/edwinpaco/Documents/projects/cpp/libcoro/install/lib/libcoro.a)
set_property(TARGET awaitables_libcoro_test PROPERTY CXX_STANDARD 20)

add_executable(close_libcoro_test close_test.cpp)
target_include_directories(close_libcoro_test PRIVATE /Users/edwinpaco/Documents/projects/cpp/libcoro/install/include)
target_link_libraries(close_libcoro_test PUBLIC GTest::gtest GTest::gtest_main cochan /Users/edwinpaco/Documents/projects/cpp/libcoro/install/lib/libcoro.a)
set_property(TARGET close_libcoro_test PROPERTY CXX_STANDARD 20)

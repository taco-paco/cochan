#find_package(PkgConfig REQUIRED)
#pkg_check_modules(LIBCORO REQUIRED libcoro)

enable_testing()

add_executable(test_with_libcoro sender_receiver_test.cpp)
target_include_directories(test_with_libcoro PRIVATE /Users/edwinpaco/Documents/projects/cpp/libcoro/install/include)
target_link_libraries(test_with_libcoro PUBLIC GTest::gtest GTest::gtest_main cochan /Users/edwinpaco/Documents/projects/cpp/libcoro/install/lib/libcoro.a)

set_property(TARGET test_with_libcoro PROPERTY CXX_STANDARD 20)

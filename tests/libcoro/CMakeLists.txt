find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBCORO REQUIRED libcoro)

enable_testing()

add_executable(sender_receiver_libcoro_test sender_receiver_test.cpp)
target_include_directories(sender_receiver_libcoro_test PRIVATE ${LIBCORO_INCLUDE_DIRS})
target_link_directories(sender_receiver_libcoro_test PRIVATE ${LIBCORO_LIBRARY_DIRS})
target_link_libraries(sender_receiver_libcoro_test PRIVATE GTest::gtest_main cochan ${LIBCORO_LIBRARIES})
set_property(TARGET sender_receiver_libcoro_test PROPERTY CXX_STANDARD 20)

add_executable(awaitables_libcoro_test awaitables_test.cpp)
target_include_directories(awaitables_libcoro_test PRIVATE ${LIBCORO_INCLUDE_DIRS})
target_link_directories(awaitables_libcoro_test PRIVATE ${LIBCORO_LIBRARY_DIRS})
target_link_libraries(awaitables_libcoro_test PRIVATE GTest::gtest_main cochan ${LIBCORO_LIBRARIES})
set_property(TARGET awaitables_libcoro_test PROPERTY CXX_STANDARD 20)

add_executable(close_libcoro_test close_test.cpp)
target_include_directories(close_libcoro_test PRIVATE ${LIBCORO_INCLUDE_DIRS})
target_link_directories(close_libcoro_test PRIVATE ${LIBCORO_LIBRARY_DIRS})
target_link_libraries(close_libcoro_test PRIVATE GTest::gtest_main cochan ${LIBCORO_LIBRARIES})
set_property(TARGET close_libcoro_test PROPERTY CXX_STANDARD 20)

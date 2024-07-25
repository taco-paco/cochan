add_executable(playground playground.cpp)
set_property(TARGET playground PROPERTY CXX_STANDARD 20)


add_executable(test test.cpp)
target_link_libraries(test PUBLIC co_chan)
set_property(TARGET test PROPERTY CXX_STANDARD 20)

add_executable(sender_drop sender_drop.cpp)
target_link_libraries(sender_drop PUBLIC co_chan)
set_property(TARGET sender_drop PROPERTY CXX_STANDARD 20)

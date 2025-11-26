cmake_minimum_required(VERSION 3.10)
project(robot)

set(CMAKE_CXX_STANDARD 17)

add_executable(robot_control
    receiver.cpp
    movement.cpp
)

target_link_libraries(robot_control wiringPi)

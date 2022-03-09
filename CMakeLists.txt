cmake_minimum_required(VERSION 3.13)

project(event_bus)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(readerwriterqueue-1.0.2 EXCLUDE_FROM_ALL)
include_directories(readerwriterqueue-1.0.2)
add_executable(event_bus main.cpp event_producer.cpp)
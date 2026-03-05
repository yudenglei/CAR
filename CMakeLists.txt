# CAR - High Performance CAE PCB Data Relationship System
cmake_minimum_required(VERSION 3.16)
project(CAR VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

find_package(Threads REQUIRED)

option(BUILD_TESTS "Build test programs" ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Create test executable
if(BUILD_TESTS)
    add_executable(car_test test/car_test.cpp)
    target_link_libraries(car_test PRIVATE Threads::Threads)
    enable_testing()
    add_test(NAME car_test COMMAND car_test)
endif()

message(STATUS "CAR Configuration:")
message(STATUS "  Version: ${CAR_VERSION}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Tests: ${BUILD_TESTS}")

cmake_minimum_required(VERSION 3.16)
project(CAR VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_executable(car_test test/car_test.cpp)
target_include_directories(car_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(car_test PRIVATE Threads::Threads)

enable_testing()
add_test(NAME car_test COMMAND car_test)

cmake_minimum_required(VERSION 3.0.0)

project(asyncio VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_COMPILE_FEATURES "${CMAKE_CXX_COMPILE_FEATURES} /MT /GR-")
add_definitions(-DCOROUTINE_FEATURE_COROFINALLY -DCOROUTINE_FEATURE_EXPRIMENTAL)

include_directories(modules third_party)

add_executable(asyncio src/main.cc)
target_link_libraries(asyncio ws2_32)

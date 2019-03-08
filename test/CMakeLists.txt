cmake_minimum_required(VERSION 3.1)
project(object_test)

file(DOWNLOAD "https://github.com/catchorg/Catch2/releases/download/v2.7.0/catch.hpp"
              "${CMAKE_CURRENT_LIST_DIR}/catch.hpp"
              EXPECTED_HASH SHA512=24e918e61d27a3047a78014fdbdc7dddaf90c40942d72d6fc92343fd6b078ef1f4fd6723a22b6c92b32bec361192edf5122332d3ac591b231ab37336492e8013)

file(GLOB SRC_FILES *.cpp)

add_executable(${PROJECT_NAME} ${SRC_FILES})

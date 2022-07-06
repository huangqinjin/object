cmake_minimum_required(VERSION 3.8)
project(object_test)

file(DOWNLOAD "https://github.com/catchorg/Catch2/releases/download/v2.13.9/catch.hpp"
              "${CMAKE_CURRENT_LIST_DIR}/catch.hpp"
              EXPECTED_HASH SHA512=3ff722c4c85ff16e6121705c1a5b97f51fabab13dc8594efc213d81bbc485dc4af7bec1ffbe55a732b363e605e9a4c0703e28c975e3b3ea676e2889fedd01b43)

file(GLOB SRC_FILES *.cpp)

add_executable(${PROJECT_NAME} ${SRC_FILES})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)
target_compile_options(${PROJECT_NAME} PUBLIC $<$<BOOL:${MSVC}>:/permissive->)

find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} PUBLIC Threads::Threads)

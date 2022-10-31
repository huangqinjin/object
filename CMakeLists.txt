cmake_minimum_required(VERSION 3.8)
project(object)

if(NOT DEFINED BUILD_TESTING OR BUILD_TESTING)
	enable_testing()
	add_subdirectory(test)
endif()

cmake_minimum_required(VERSION 3.20)

project(Sera CXX C)

include(cmake/utils.cmake)

option(BUILD_SERA_DLL OFF)

find_package(Vulkan REQUIRED)
if(NOT ${Vulkan_FOUND})
	message(FATAL_ERROR "Vulkan not found!")
endif()

# Dependencies
add_subdirectory(vendor)
add_subdirectory(Sera)
add_subdirectory(SeraApp)

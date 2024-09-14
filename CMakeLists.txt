cmake_minimum_required(VERSION 3.20)

project(Sera CXX C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/utils.cmake)

option(BUILD_SERA_DLL OFF)
# if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
# 	add_compile_definitions(PLATFORM_WIN32=1)
# endif()

# Dependencies
add_subdirectory(vendor)

# Our app
add_subdirectory(Sera)
add_subdirectory(SeraApp)

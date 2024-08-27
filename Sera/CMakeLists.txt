cmake_minimum_required(VERSION 3.20)

project(Sera CXX C)
PRINT_PROJECT_CONFIGURING_MESSAGE()

set(CMAKE_CXX_STANDARD 17)

file(GLOB_RECURSE 
	SERA_SOURCES 
		src/*.cpp
		src/Input/*.cpp
)
set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/vendor/imgui")
set(IMGUI_SRCS 
	"${IMGUI_DIR}/imgui.cpp"
	"${IMGUI_DIR}/imgui_demo.cpp"
	"${IMGUI_DIR}/imgui_draw.cpp"
	"${IMGUI_DIR}/imgui_tables.cpp"
	"${IMGUI_DIR}/imgui_widgets.cpp"
)
set(IMGUI_BACKEND 
	"${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
	"${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp"
)

if(BUILD_SERA_DLL)
	message("${PROJECT_NAME} building as shared library")
	add_library(${PROJECT_NAME} SHARED "${SERA_SOURCES};${IMGUI_SRCS};${IMGUI_BACKEND}")
else()
	add_library(${PROJECT_NAME} "${SERA_SOURCES};${IMGUI_SRCS};${IMGUI_BACKEND}")
endif()

target_link_libraries(Sera PUBLIC glfw spdlog glm "${Vulkan_LIBRARIES}")

target_include_directories("${PROJECT_NAME}" 
	PUBLIC
	"include/Sera"
	"${IMGUI_DIR}/" 
	"${CMAKE_SOURCE_DIR}/vendor/glfw/include"
	"${CMAKE_SOURCE_DIR}/vendor/glm"
	"${CMAKE_SOURCE_DIR}/vendor/spdlog/include"
	"${CMAKE_SOURCE_DIR}/vendor/stb"
	"${Vulkan_INCLUDE_DIRS}"
)

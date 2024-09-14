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
	add_library(${PROJECT_NAME} SHARED "${SERA_SOURCES};${IMGUI_SRCS}")
	
else()
	add_library(${PROJECT_NAME} "${SERA_SOURCES};${IMGUI_SRCS}")
endif()

target_link_libraries("${PROJECT_NAME}" PUBLIC glfw 
	spdlog 
	glm 
	Diligent-GraphicsEngine)

target_include_directories("${PROJECT_NAME}" 
	PUBLIC
	"include/Sera"
	"${IMGUI_DIR}/" 
	"${CMAKE_SOURCE_DIR}/vendor/glfw/include"
	"${CMAKE_SOURCE_DIR}/vendor/glm"
	"${CMAKE_SOURCE_DIR}/vendor/spdlog/include"
	"${CMAKE_SOURCE_DIR}/vendor/stb"
	"${CMAKE_SOURCE_DIR}/vendor/DiligentCore"
)
message("Diligent inclde dir : ${CMAKE_SOURCE_DIR}/vendor/DiligentCore")
# automatic shader recompiling with source code
if(WIN32)
	add_custom_command(TARGET "${PROJECT_NAME}"
		POST_BUILD DEPENDS "${PROJECT_NAME}"
		COMMAND "compile.bat"
		WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/Assets/shaders/" 
		COMMENT "Compiling basic triangle shaders"
	)
else()
	message("DONT FORGET TO COMPILE SHADERS")
endif()

copy_required_dlls(Sera)

# Macros for build configs
add_compile_definitions(
	$<$<CONFIG:Debug>:SR_DEBUG>
)
add_compile_definitions(
	$<$<CONFIG:Release>:SR_RELEASE>
)

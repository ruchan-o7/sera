cmake_minimum_required(VERSION 3.20)

project(Sera CXX C)
PRINT_PROJECT_CONFIGURING_MESSAGE()

set(CMAKE_CXX_STANDARD 17)

file(GLOB_RECURSE 
	SERA_SOURCES 
		src/*.cpp
		src/Input/*.cpp
)

if(BUILD_SERA_DLL)
	message("${PROJECT_NAME} building as shared library")
	add_library(${PROJECT_NAME} SHARED "${SERA_SOURCES}")
	
else()
	add_library(${PROJECT_NAME} "${SERA_SOURCES};src/imgui_demo.cpp")
endif()

set_target_properties(Sera PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
target_compile_features(Sera PUBLIC cxx_std_14)


target_include_directories("${PROJECT_NAME}" 
	PUBLIC
	"include/Sera"
	"${CMAKE_SOURCE_DIR}/vendor/glfw/include"
	"${CMAKE_SOURCE_DIR}/vendor/glm"
	"${CMAKE_SOURCE_DIR}/vendor/spdlog/include"
	"${CMAKE_SOURCE_DIR}/vendor/stb"
	"${CMAKE_SOURCE_DIR}/vendor/Diligent/DiligentCore"
	"${CMAKE_SOURCE_DIR}/vendor/Diligent/DiligentTools/ThirdParty/imgui"
	"${CMAKE_SOURCE_DIR}/vendor/Diligent/DiligentTools/Imgui/interface"
)

target_compile_definitions(Sera PRIVATE UNICODE)

target_compile_definitions(Sera PRIVATE ENGINE_DLL=1)
target_link_libraries("${PROJECT_NAME}" PUBLIC 
	glfw 
	spdlog 
	glm 
	Diligent-Common
	Diligent-GraphicsTools
	Diligent-Imgui
)

if(D3D11_SUPPORTED)
    target_link_libraries(Sera PRIVATE Diligent-GraphicsEngineD3D11-shared)
endif()
if(D3D12_SUPPORTED)
    target_link_libraries(Sera PRIVATE Diligent-GraphicsEngineD3D12-shared)
endif()
if(GL_SUPPORTED)
    target_link_libraries(Sera PRIVATE Diligent-GraphicsEngineOpenGL-shared)
endif()
if(VULKAN_SUPPORTED)
    target_link_libraries(Sera PRIVATE Diligent-GraphicsEngineVk-shared)
endif()
if(METAL_SUPPORTED)
    target_link_libraries(Sera PRIVATE Diligent-GraphicsEngineMetal-shared)
endif()
if(PLATFORM_WIN32)
	copy_required_dlls(Sera)
endif()
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

# copy_required_dlls(Sera)

# Macros for build configs
add_compile_definitions(
	$<$<CONFIG:Debug>:SR_DEBUG>
)
add_compile_definitions(
	$<$<CONFIG:Release>:SR_RELEASE>
)

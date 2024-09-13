project(SeraApp CXX C)

set(CMAKE_CXX_STANDARD 17)

PRINT_PROJECT_CONFIGURING_MESSAGE()

add_executable(${PROJECT_NAME} "src/Application.cpp")

target_include_directories(
	${PROJECT_NAME} PRIVATE 
	"${CMAKE_SOURCE_DIR}/Sera/include"
	"../vendor/imgui/"
)
target_link_libraries("${PROJECT_NAME}" PRIVATE Sera)

copy_required_dlls(SeraApp)

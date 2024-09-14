message("Configuring dependencies")

# Dependency configs
option(GLFW_BUILD_EXAMPLES OFF)
option(GLFW_BUILD_TESTS OFF)

option(SPDLOG_BUILD_EXAMPLE OFF)
option(SPDLOG_BUILD_EXAMPLE_HO OFF)
option(SPDLOG_BUILD_TESTS OFF)
option(SPDLOG_BUILD_TESTS_HO OFF)

option(GLM_TEST_ENABLE_CXX_17 ON)

add_subdirectory(glfw)
add_subdirectory(glm)
add_subdirectory(spdlog)
add_subdirectory(Diligent/DiligentCore)

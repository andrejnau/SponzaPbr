cmake_minimum_required(VERSION 3.15)
project(SponzaPbr)

include(CMakeDependentOption)
option(VULKAN_SUPPORT "Vulkan support" ON)
cmake_dependent_option(DIRECTX_SUPPORT "DirectX 12 support" ON "WIN32" OFF)
cmake_dependent_option(METAL_SUPPORT "Metal support" ON "APPLE" OFF)
option(BUILD_SAMPLES "Build samples" ON)
option(BUILD_TESTING "Build unit tests" ON)

set(project_root "${CMAKE_CURRENT_SOURCE_DIR}")
set(assets_path "${project_root}/assets/")
add_library(SponzaPbrAssets INTERFACE)
target_compile_definitions(SponzaPbrAssets INTERFACE ASSETS_PATH="${assets_path}")

list(PREPEND CMAKE_MODULE_PATH "${project_root}/cmake")
include(cmake_settings)
include(compiler_settings)

add_subdirectory(3rdparty/FlyWrapper)
include(3rdparty/imgui)
include(3rdparty/glfw)

if (BUILD_TESTING)
    enable_testing()
endif()

add_subdirectory(src)

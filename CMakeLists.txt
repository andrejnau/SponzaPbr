cmake_minimum_required(VERSION 3.15)
project(SponzaPbr)

set(project_root "${CMAKE_CURRENT_SOURCE_DIR}")
set(assets_path "${project_root}/assets/")
add_definitions(-DASSETS_PATH="${assets_path}")

list(PREPEND CMAKE_MODULE_PATH "${project_root}/cmake")
include(cmake_settings)
include(compiler_settings)

add_subdirectory(3rdparty/FlyCube)
include(3rdparty/imgui)

add_subdirectory(src)

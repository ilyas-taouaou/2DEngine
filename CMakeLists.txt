cmake_minimum_required(VERSION 3.28)
project(2DEngine)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)

add_subdirectory(submodules/SDL)
add_subdirectory(submodules/Vulkan-Headers)
add_subdirectory(submodules/volk)

add_executable(2DEngine main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3 Vulkan::Headers volk::volk_headers)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED on)
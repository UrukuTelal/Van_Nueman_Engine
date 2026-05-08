find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

include_directories(${Vulkan_INCLUDE_DIRS} ${GLFW_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/renderer)

add_library(vn-renderer STATIC vulkan_renderer.cpp)
target_link_libraries(vn-renderer ${Vulkan_LIBRARIES} glfw)

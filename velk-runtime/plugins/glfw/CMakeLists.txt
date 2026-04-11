find_package(Vulkan REQUIRED)

add_library(velk_runtime_glfw SHARED
    src/glfw_plugin.h
    src/glfw_plugin.cpp
    src/glfw_window.h
    src/glfw_window.cpp
    include/velk-runtime/plugins/glfw/plugin.h
)

target_link_libraries(velk_runtime_glfw PRIVATE velk velk_runtime velk_render velk_vk velk_ui glfw)

target_include_directories(velk_runtime_glfw
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE ${Vulkan_INCLUDE_DIRS}
)

target_compile_definitions(velk_runtime_glfw PRIVATE VELK_EXPORTS VK_NO_PROTOTYPES)

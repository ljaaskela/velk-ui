add_library(velk_render SHARED
    src/render_plugin.h
    src/render_plugin.cpp
    src/renderer.h
    src/renderer.cpp
    src/surface.h
    include/velk-ui/plugins/render/plugin.h
    include/velk-ui/plugins/render/intf_render_backend.h
)

target_link_libraries(velk_render PRIVATE velk velk_ui)

target_include_directories(velk_render
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

option(VELK_RENDER_DEBUG "Enable verbose render logging" OFF)

target_compile_definitions(velk_render PRIVATE VELK_EXPORTS)

if(VELK_RENDER_DEBUG)
    target_compile_definitions(velk_render PRIVATE VELK_RENDER_DEBUG)
endif()

# =============================================================================
# velk-scene: scene model + renderer.
#
# Owns IScene / IElement / IVisual interfaces and the render paths
# (Forward / Deferred / RayTrace) that consume them. velk-ui depends on
# velk-scene for these primitives; velk-scene depends on velk-render
# for GPU plumbing.
#
# See design-notes/architecture_layers.md for the layering plan.
# =============================================================================

add_library(velk_scene SHARED
    # Plugin entry
    src/plugin.h
    src/plugin.cpp

    # Concrete scene-model implementations
    src/element.h
    src/element.cpp
    src/scene.h
    src/scene.cpp

    # Renderer + render paths
    src/renderer.h
    src/renderer.cpp
    src/view_renderer.h
    src/rasterizer.h
    src/rasterizer.cpp
    src/deferred_lighter.h
    src/deferred_lighter.cpp
    src/ray_tracer.h
    src/ray_tracer.cpp

    # Scene-side rendering plumbing
    src/scene_bvh.h
    src/scene_bvh.cpp
    src/scene_collector.h
    src/scene_collector.cpp
    src/batch_builder.h
    src/batch_builder.cpp
    src/frame_data_manager.h
    src/frame_data_manager.cpp
    src/frame_snippet_registry.h
    src/frame_snippet_registry.cpp
    src/gpu_resource_manager.h
    src/gpu_resource_manager.cpp
    src/env_helper.h
    src/env_helper.cpp
    src/default_ui_shaders.h

    # Public interfaces (scene model + renderer surface)
    include/velk-scene/plugin.h
    include/velk-scene/types.h
    include/velk-scene/api/element.h
    include/velk-scene/api/scene.h
    include/velk-scene/api/trait.h
    include/velk-scene/interface/intf_scene_plugin.h
    include/velk-scene/interface/intf_scene.h
    include/velk-scene/interface/intf_scene_observer.h
    include/velk-scene/interface/intf_element.h
    include/velk-scene/interface/intf_visual.h
    include/velk-scene/interface/intf_renderer.h
    include/velk-scene/interface/intf_trait.h
    include/velk-scene/interface/intf_environment.h
    include/velk-scene/interface/intf_render_to_texture.h
    include/velk-scene/ext/material_shaders.h
)

target_link_libraries(velk_scene PRIVATE velk velk_importer velk_render)

target_include_directories(velk_scene
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(velk_scene PRIVATE VELK_EXPORTS)

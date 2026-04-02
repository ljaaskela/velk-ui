add_library(velk_ui SHARED
    src/plugin.h
    src/plugin.cpp
    src/element.h
    src/element.cpp
    src/scene.h
    src/scene.cpp
    src/layout_solver.h
    src/layout_solver.cpp
    src/layout/stack.h
    src/layout/stack.cpp
    src/constraint/fixed_size.h
    src/constraint/fixed_size.cpp
    src/visual/rect_visual.h
    src/visual/rect_visual.cpp
    src/visual/rounded_rect_visual.h
    src/visual/rounded_rect_visual.cpp
    src/transform/trs.h
    src/transform/trs.cpp
    src/transform/matrix.h
    src/transform/matrix.cpp
    src/material/shader_material.h
    src/material/gradient_material.h
    src/material/gradient_material.cpp
    src/import/align_type_extension.h
    src/import/align_type_extension.cpp
    src/import/dim_type_extension.h
    src/import/dim_type_extension.cpp
    include/velk-ui/interface/intf_element.h
    include/velk-ui/interface/intf_font.h
    include/velk-ui/interface/intf_render_context.h
    include/velk-ui/interface/intf_renderer.h
    include/velk-ui/interface/intf_layout_trait.h
    include/velk-ui/interface/intf_transform_trait.h
    include/velk-ui/interface/intf_scene.h
    include/velk-ui/interface/intf_surface.h
    include/velk-ui/interface/intf_scene_observer.h
    include/velk-ui/interface/trait/intf_stack.h
    include/velk-ui/interface/trait/intf_fixed_size.h
    include/velk-ui/interface/trait/intf_trs.h
    include/velk-ui/interface/trait/intf_matrix.h
    include/velk-ui/api/trait/trs.h
    include/velk-ui/api/trait/matrix.h
    include/velk-ui/interface/intf_visual.h
    include/velk-ui/interface/intf_texture_provider.h
    include/velk-ui/ext/trait.h
    include/velk-ui/ext/visual.h
    include/velk-ui/api/trait/fixed_size.h
    include/velk-ui/api/trait/stack.h
    include/velk-ui/api/element.h
    include/velk-ui/api/material/gradient.h
    include/velk-ui/api/material/shader.h
    include/velk-ui/api/scene.h
    include/velk-ui/api/trait.h
    include/velk-ui/api/visual.h
    include/velk-ui/api/visual/rect.h
    include/velk-ui/api/visual/rounded_rect.h
    include/velk-ui/interface/intf_gradient.h
    include/velk-ui/interface/intf_material.h
    include/velk-ui/interface/intf_material_internal.h
    include/velk-ui/interface/intf_trait.h
    include/velk-ui/plugin.h
    include/velk-ui/types.h
)

target_link_libraries(velk_ui PRIVATE velk velk_importer)

target_include_directories(velk_ui
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

option(VELK_LAYOUT_DEBUG "Enable verbose layout logging" OFF)

target_compile_definitions(velk_ui PRIVATE VELK_EXPORTS)

if(VELK_LAYOUT_DEBUG)
    target_compile_definitions(velk_ui PRIVATE VELK_LAYOUT_DEBUG)
endif()

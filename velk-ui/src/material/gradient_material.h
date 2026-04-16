#ifndef VELK_UI_GRADIENT_MATERIAL_H
#define VELK_UI_GRADIENT_MATERIAL_H

#include <velk-render/ext/material.h>
#include <velk-render/interface/intf_render_context.h>
#include <velk-ui/interface/intf_gradient.h>
#include <velk-ui/plugin.h>

namespace velk::ui {

/**
 * @brief Built-in linear gradient material.
 *
 * Lazily compiles a gradient shader on first use via create_shader_material().
 * Provides gradient parameters (start_color, end_color, angle) as GPU data
 * that the shader reads via buffer_reference.
 */
class GradientMaterial : public ::velk::ext::Material<GradientMaterial, IGradient>
{
public:
    VELK_CLASS_UID(ClassId::Material::Gradient, "GradientMaterial");

    uint64_t get_pipeline_handle(IRenderContext& ctx) override;
    size_t gpu_data_size() const override;
    ReturnValue write_gpu_data(void* out, size_t size) const override;

    string_view get_fill_src() const override;
    string_view get_fill_fn_name() const override;
    string_view get_fill_include_name() const override;
};

} // namespace velk::ui

#endif // VELK_UI_GRADIENT_MATERIAL_H
